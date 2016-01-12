#define ARP_UN_PATH "/tmp/arp_path-2324"
char TOUR_UN_PATH[100];

#define AA19_PROTO 0x999
#define MAX_REQ 100

char self_ip_str[14];
char *self_hw_addr;
int self_if_idx;
int un_sockfd, ll_sockfd;
struct sockaddr_un arp_un_addr;
struct sockaddr_un tour_un_addr;
int tour_un_addr_len = sizeof(struct sockaddr_un);
int running_request_idx = -1;

struct request {
	int request_status;		//1 for rcvd, 2 for replied
	int request_idx;
	struct sockaddr_un request_un_addr;
	char requested_hw_addr[6];
	char requested_ip_addr[14];
	struct list_head list;
}req;

struct arp_cache {
	char ip_str[14];
	char hw_addr[6];
	time_t ts;
	struct list_head list;
}cache;

struct arp_packet {
	char src_ip_str[14];
	char src_hw_addr_str[6];
	char req_ip_str[14];
	char req_hw_addr_str[6];
}arp_pack;

void print_self_name(){
	char hostname[5];
	gethostname(hostname, sizeof(hostname));
	printf("%s\n",hostname);
}
void print_hwaddr( char* hw_addr){
	int i;
	i = 0;
	while(i<6){
		if(i != 5)
			printf("%.2x:", hw_addr[i] & 0xff);
		else
			printf("%.2x", hw_addr[i] & 0xff);

		if(i == 5)
			break;
		i++;
	}
	printf("\n");
}


int broadcast_ll_msg( char ip_str[]){
	int ret = 0;
	struct sockaddr_ll *bc_addr;
	const unsigned char broadcast_addr[] = {0xff,0xff,0xff,0xff,0xff,0xff};
	const unsigned char zero_hw_addr[] = {0x00,0x00,0x00,0x00,0x00,0x00};
	void *buffer;
	struct ethhdr* eh;

	buffer = malloc(14 + sizeof(struct arp_packet));
	bc_addr = (struct sockaddr_ll*) malloc(sizeof(struct sockaddr_ll));
	eh = buffer;	

	bc_addr->sll_family = AF_PACKET;
	bc_addr->sll_ifindex = self_if_idx;
	bc_addr->sll_halen = IF_HADDR;
	bc_addr->sll_protocol = htons(AA19_PROTO);
	memcpy(bc_addr->sll_addr, broadcast_addr, IF_HADDR);
	bc_addr->sll_addr[6] = 0x00;	
	bc_addr->sll_addr[7] = 0x00;	
	memcpy(&arp_pack.src_ip_str, self_ip_str, 14);
	memcpy(&arp_pack.src_hw_addr_str, self_hw_addr, 6 );
	memcpy(&arp_pack.req_ip_str, ip_str, 14);
	memcpy(&arp_pack.req_hw_addr_str, zero_hw_addr, 6 );
	
	//filling ethernet layer packet
	memcpy(buffer, broadcast_addr, 6);
	memcpy(buffer+6, self_hw_addr, 6);
	memcpy(buffer+14, (void*)&arp_pack, sizeof(struct arp_packet));
	eh->h_proto = htons(AA19_PROTO);

	printf("Broadcasting from : ");
	print_self_name();
	ret = sendto(ll_sockfd, buffer, 14+ sizeof(arp_pack) , 0, (SA*)bc_addr, sizeof(struct sockaddr_ll) );
	//printf("broadcast ret is %d\n", ret);
	return ret;
}

int check_entry_exists(char ip_str[]){
	struct arp_cache *temp;
	
	temp = (struct arp_cache*) malloc(sizeof(struct arp_cache));
	list_for_each_entry_reverse(temp, &(cache.list), list){
		if( strncmp(temp->ip_str, ip_str, 14) == 0 )
			return 1;
	}
	return 0;
}

int get_entry( char ip_str[], char* hw_addr){
	
	struct arp_cache *temp;
	
	temp = (struct arp_cache*) malloc(sizeof(struct arp_cache));
	list_for_each_entry_reverse(temp, &(cache.list), list){
		if( strncmp(temp->ip_str, ip_str, 14) == 0 ){
			memcpy(hw_addr, temp->hw_addr, 6);
			return 1;
		}
	}
	return -1;

}

int print_cache( ){

	struct arp_cache *temp;
	int i =1;
	printf("__________________________\n");
	printf("\nPrinting  cache entries\n");
	printf("--------------------------\n");
	//temp = (struct arp_cache*) malloc(sizeof(struct arp_cache));
	list_for_each_entry_reverse(temp, &(cache.list), list){
		printf("%d. ", i);
		printf("%s - ", temp->ip_str);
		print_hwaddr(temp->hw_addr);
		printf("\n");
		i++;
	}
	printf("\n------------------------\n");
}
int update_cache( char ip_str[], char hw_addr[]){

	struct arp_cache *temp;
	int exists = 0;
	
	printf("Updating cache...\n");
	list_for_each_entry_reverse(temp, &(cache.list), list){
		if( strncmp(temp->ip_str, ip_str, 14) == 0 ){
			temp->ts = time(NULL);
			exists = 1;
			return 0;
		}
	}
	if( exists == 0){
		temp = (struct arp_cache*) malloc(sizeof(struct arp_cache));
		printf("Adding new entries to table\n");
		strncpy(temp->ip_str, ip_str, 14);
		memcpy(temp->hw_addr, hw_addr, 6);
		temp->ts = time(NULL);
		printf("Adding ip : %s\n", temp->ip_str);
		printf("Adding hw : ");
		print_hwaddr(temp->hw_addr);
		list_add(&(temp->list), &(cache.list));
		return 0;
	}
		printf("end of update cache\n");
	return -1;
	
}

int discover_node(char ip_str[]){
	int ret;
	printf("Discovering hardware address for ip address  %s\n", ip_str);
	ret = broadcast_ll_msg(ip_str);
	return 0;
}

int cache_hw_addr_lookup( char ip_str[], char* hw_addr){
	int ret = 0;

	//printf("ip str is %s\n", ip_str);
	if((ret = check_entry_exists(ip_str)) == 1){
		printf("Cache table has entry for IP address\nReturning the Hardware address from cache table\n");
		ret = get_entry( ip_str, hw_addr);		
	}
	else{
		printf("Cache table doesn't have netry for IP.\n");
		
		ret = discover_node( ip_str);
		/*
		if((ret = check_entry_exists(ip_str)) == 1){
			ret = get_entry( ip_str, hw_addr);		
		}
		else{
			printf("\n******ALERT 1 in find_hw_addr *****\n");	
			printf("Unable to find entry in cache even after discovery\n");
		}
		*/
		ret = -1;
	}
	return ret;
}


