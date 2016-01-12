#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_systm.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <wait.h>
#include "unp.h"
#include "list.h"

#define ETH_P_IP 0x0800
#define IPPROTO_TOUR 0x00D3
#define MULTICAST_PORT 20852
#define LOCAL_ID 240
#define TTL_OUT 64
#define MULTICAST_ADDR "239.192.10.13" 
#define ARP_UN_PATH "/tmp/arp_path-2324" 
#define CLI_UN_PATH "/tmp/tour2324-XXXXXX"

//MACROS FOR PING
#define MAX_LIST_SIZE  5000

#define	IF_NAME		16	/* same as IFNAMSIZ    in <net/if.h> */
#define	IF_HADDR	 6	/* same as IFHWADDRLEN in <net/if.h> */
#define	IP_ALIAS  	 1	/* hwa_addr is an alias */

#define BUFSIZE          	1500
#define IP_HEADER_SIZE 	    20  // IPv4 header length
#define ICMP_HEADER_SIZE    8  // ICMP header length for echo request, excludes data
#define PF_PACKET_HEADER	14
 
#define	PROTOCOL_NO_PF		51000
#define	PROTOCOL_NO			200
#define PAYLOAD_SIZE        5000
/////MACROS FOR PING END ABOVE

//GLOBALS FOR PING
 /* globals */
char    sendbuf[BUFSIZE];
int     datalen;
char   *host;
int     nsent;
pid_t   pid;
int     sockfd;
long    ping_source_ip;
long    ping_dest_ip;
int     current_vm_ptr;
//GLOBALS FOR PING END HERE


int     datalen = 56;

/* function prototypes */
struct hwa_info	*get_hw_addrs();
struct hwa_info	*Get_hw_addrs();
void   free_hwa_info(struct hwa_info *);

///THIS WAS LAST DECLARATION OF PING FUNCTION

int key = 0;
int m_sock, m_readsock;
int already_pinged[12]={1,0,0,0,0,0,0,0,0,0,0,0};
int un_sockfd;
struct sockaddr_un un_arpaddr, un_cliaddr;


//for ping
long    ping_source_ip;
long    ping_dest_ip;
int 	pf_sock;
int 	multicast_flag=0;
char name[5];
int counter=0;
int total_tour_length=0;

struct hwa_info {
	char    if_name[IF_NAME];		/* interface name, null terminated */
	char    if_haddr[IF_HADDR];		/* hardware address */
	int     if_index;				/* interface index */
	short   ip_alias;				/* 1 if hwa_addr is an alias IP address */
	struct  sockaddr  *ip_addr;		/* IP address */
	struct  hwa_info  *hwa_next;	/* next of these structures */
};


struct hwaddr_list{
	char hw_addr[6];
	in_addr_t ip;
	struct list_head list;
}hwl;


struct ip_adrs_list{
	in_addr_t ip_adrs[100];
	struct ip_adrs_list*next;
};


struct tour_payload{
	int curr_index;
	int port;
	int tour_length;
	in_addr_t multicast_address;
	in_addr_t tour_ip[20];
	int already_visited[20];
	struct{
		char vm_name[8];
	}vm_list[20];
};


struct hwa_info *
get_hw_addrs()	{
	struct hwa_info	*hwa, *hwahead, **hwapnext;
	int		sockfd, len, lastlen, alias, nInterfaces, i;
	char	*buf, lastname[IF_NAME], *cptr;
	struct ifconf	ifc;
	struct ifreq	*ifr, *item, ifrcopy;
	struct sockaddr	*sinptr;

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

	lastlen = 0;
	len = 100 * sizeof(struct ifreq);	/* initial buffer size guess */
	for ( ; ; ) {
		buf = (char*) Malloc(len);
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
			if (errno != EINVAL || lastlen != 0)
				err_sys("ioctl error");
		} else {
			if (ifc.ifc_len == lastlen)
				break;		/* success, len has not changed */
			lastlen = ifc.ifc_len;
		}
		len += 10 * sizeof(struct ifreq);	/* increment */
		free(buf);
	}

	hwahead = NULL;
	hwapnext = &hwahead;
	lastname[0] = 0;
    
	ifr = ifc.ifc_req;
 	nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
for(i = 0; i < nInterfaces; i++)  {
		item = &ifr[i];
 		alias = 0; 
		hwa = (struct hwa_info *)Calloc(1, sizeof(struct hwa_info));
		memcpy(hwa->if_name, item->ifr_name, IF_NAME);		/* interface name */
		hwa->if_name[IF_NAME-1] = '\0';
				/* start to check if alias address */
		if ( (cptr = (char *) strchr(item->ifr_name, ':')) != NULL)
			*cptr = 0;		/* replace colon will null */
		if (strncmp(lastname, item->ifr_name, IF_NAME) == 0) {
			alias = IP_ALIAS;
		}
		memcpy(lastname, item->ifr_name, IF_NAME);
		ifrcopy = *item;
		*hwapnext = hwa;		/* prev points to this new one */
		hwapnext = &hwa->hwa_next;	/* pointer to next one goes here */

		hwa->ip_alias = alias;		/* alias IP address flag: 0 if no; 1 if yes */
                sinptr = &item->ifr_addr;
		hwa->ip_addr = (struct sockaddr *) Calloc(1, sizeof(struct sockaddr));
	        memcpy(hwa->ip_addr, sinptr, sizeof(struct sockaddr));	/* IP address */
		if (ioctl(sockfd, SIOCGIFHWADDR, &ifrcopy) < 0)
                          perror("SIOCGIFHWADDR");	/* get hw address */
		memcpy(hwa->if_haddr, ifrcopy.ifr_hwaddr.sa_data, IF_HADDR);
		if (ioctl(sockfd, SIOCGIFINDEX, &ifrcopy) < 0)
                          perror("SIOCGIFINDEX");	/* get interface index */
		memcpy(&hwa->if_index, &ifrcopy.ifr_ifindex, sizeof(int));
	}
	free(buf);
	return(hwahead);	/* pointer to first structure in linked list */
}

void
free_hwa_info(struct hwa_info *hwahead)	{
	struct hwa_info	*hwa, *hwanext;

	for (hwa = hwahead; hwa != NULL; hwa = hwanext) {
		free(hwa->ip_addr);
		hwanext = hwa->hwa_next;	/* can't fetch hwa_next after free() */
		free(hwa);			/* the hwa_info{} itself */
	}
}
/* end free_hwa_info */

struct hwa_info *
Get_hw_addrs()	{
	struct hwa_info	*hwa;

	if ( (hwa = get_hw_addrs()) == NULL)
		err_quit("get_hw_addrs error");
	return(hwa);
}

int
mac_filler(char	*dest,
				int		if_index)	{
	struct hwa_info		*hwa;
	int					status = 1;
	
	
	hwa = Get_hw_addrs();
	for (; hwa != NULL; hwa = hwa->hwa_next) {
		if(hwa->if_index == if_index) {
			memcpy((void*)dest, (void*)hwa->if_haddr, ETH_ALEN);
			status = 0;
			break;
		}
	}
//	printf("\nI have added mac\n ");
	return status;
}


void 
get_vmname(long ip,
			char	*dest) {
			
	struct hostent 		*hesrc = NULL;
	struct in_addr 		ipsrc;
	socklen_t			len = 0;
	
	ipsrc.s_addr = ip;
	len = sizeof(ipsrc);
    hesrc = gethostbyaddr((const char *)&ipsrc, len, AF_INET);
	strncpy(dest, hesrc->h_name, strlen(hesrc->h_name));
}



void print_hwaddr( char* hw_addr){
	int i;
	
	i = 0;
	while(i<6){
		printf("%.2x:", hw_addr[i] & 0xff);
		if(i == 5)
		   break;
		i++;
	}
	printf("\n");
}


uint16_t
in_cksum (uint16_t * addr, int len)
  {
      int     nleft = len;
      uint32_t sum = 0;
      uint16_t *w = addr;
      uint16_t answer = 0;

      /*
       * Our algorithm is simple, using a 32 bit accumulator (sum), we add
      * sequential 16 bit words to it, and at the end, fold back all the
      * carry bits from the top 16 bits into the lower 16 bits.
      */
     while (nleft > 1) {
         sum += *w++;
         nleft -= 2;
     }
         /* mop up an odd byte, if necessary */
     if (nleft == 1) {
         * (unsigned char *) (&answer) = * (unsigned char *) w;
         sum += answer;
     }

         /* add back carry outs from top 16 bits to low 16 bits */
     sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
     sum += (sum >> 16);     /* add carry */
     answer = ~sum;     /* truncate to 16 bits */
     return (answer);
 }

void
  tv_sub (struct timeval *out, struct timeval *in)
  {
      if ((out->tv_usec -= in->tv_usec) < 0) {     /* out -= in */
          --out->tv_sec;
          out->tv_usec += 1000000;
      }
     out->tv_sec -= in->tv_sec;
 }

//------------------------------------------------------------------------------------HEADERS



in_addr_t get_ip(char *vmname){
	struct hostent *he;
	struct in_addr **addr_list;
	char* ip_addr;
	in_addr_t ipaddress;

	he = gethostbyname(vmname);
	addr_list = (struct in_addr **)he->h_addr_list;
	//printf("%s \n", inet_ntoa(*addr_list[0]));
	ip_addr=(inet_ntoa(*addr_list[0]));
	ipaddress = inet_addr(ip_addr);
return ipaddress;
}


int 
get_max_sock(int a, int b, int c){
	int max;
	//printf("\n rt:%d pg:%d m_read:%d",a,b,c);
	if (a>=b)
      {
          if(a>=c)
				max=a;
          else
				max=c;
      }
    else
      {
          if(b>=c)
				max=b;
          else
				max=c;
      }
return(max);	  
}

 
  
void join_multicast(int sockfd, struct tour_payload* mynode)
{	
	struct sockaddr_in join;
	int ret = 0;
	join.sin_family=AF_INET;
	join.sin_addr.s_addr=mynode->multicast_address;
	
	if((mynode->already_visited[key])==0  ){                         //|| (mynode->curr_index)==0
		mynode->already_visited[key]++;
		//printf("\n printing the value for join %s\n",(inet_ntoa(join.sin_addr)));
	
		if((ret = mcast_join(sockfd,((struct sockaddr *)&join),sizeof(join),NULL,0))<0){
			printf("Error in adding multicast address %d\n", ret);
		}
		
			//printf("\n  it means i am added to the multigroup\n")	;
	}
	
	//printf("\n curr index is %d",(mynode->curr_index));
}



void mcast_send(char msg[])
{
	char *msg_str;
    struct sockaddr_in mcast_addr;
	int n, mcast_send,retval;
	char name[5];
	
	//printf("\n\t inside mcast_send");
	
	
	msg_str = malloc(100);
	strncpy(msg_str, msg, 100);
	//printf("msg_str is %s\n", msg_str);
	memset(&mcast_addr, 0, sizeof(mcast_addr));
	mcast_addr.sin_family = AF_INET;
	mcast_addr.sin_port = htons(MULTICAST_PORT);
	mcast_addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
	//printf("\n the address for last node of tour is %s",(inet_ntoa(mcast_addr.sin_addr)));
	gethostname(name,5);
	printf("Node %s  \t ",name);
	printf("Sending <%s> \n",msg_str);
	sleep(5);
	retval=sendto(m_sock,msg_str,(strlen(msg_str)), 0, (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
	//printf("i have sent  %d",retval);
	
	
	// if(n < 0){
		// perror("\nsend");
		// printf("\nMulticast send error\n");
	// }
	// else
		// printf("\n\t %d msggg sent bytes",n);

}


int 
tour(int rt_sock, struct tour_payload* mynode){
	struct iphdr *ip_hdr;
	//struct tour_payload* mynode;
	unsigned char*buf;
	int ret;
	struct sockaddr_in dest_addr;
	int node_len,hdr_len,total_len;

	node_len=sizeof(struct tour_payload);
	hdr_len=sizeof(struct iphdr);
	total_len=node_len+hdr_len;
	//printf("\n\t inside tour func");
	ip_hdr = (struct iphdr *)malloc(hdr_len);
	memset(ip_hdr,0,hdr_len);
	buf=(unsigned char*)malloc(total_len);
	memset(buf,0,total_len);

    ip_hdr->ihl = 5; 
	ip_hdr->version = 4;
	ip_hdr->tos = 0;
	ip_hdr->tot_len = htons(total_len);
	ip_hdr->id = htons(LOCAL_ID);
	ip_hdr->ttl = htons(TTL_OUT);
	ip_hdr->protocol = IPPROTO_TOUR;
	
	ip_hdr->saddr = mynode->tour_ip[mynode->curr_index];
	ip_hdr->daddr = mynode->tour_ip[(mynode->curr_index)+1];
	    	
	ip_hdr->check = 0;
	ip_hdr->check = in_cksum((unsigned short *)ip_hdr, total_len);
	
	memcpy(buf,ip_hdr,hdr_len);
	memcpy(buf+hdr_len,mynode,node_len);
	
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = mynode->tour_ip[(mynode->curr_index)+1];
	
	//printf("buf len is %ld\n", strlen(buf));
	//printf("tourlen in tour is %d\n", mynode->tour_length);
	//printf("curr idx is %d\n", mynode->curr_index);
	if(mynode->curr_index < mynode->tour_length){
		printf("\n");
		//printf("rt send to %d \tlen  %d\n", rt_sock, total_len);
		//printf("dest addr is %s\n", Sock_ntop((SA*)&dest_addr, sizeof(dest_addr)));
		ret = sendto(rt_sock, buf, total_len, 0, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
		
		//printf("ret of sendto tour is %d\n" ,ret);
		if (ret < 0) {
			printf("Error sending tour packet\n");
			return -1;
		}

	}
	
return 0;
}



int 
muticast_first_recieve(int m_readsock,struct tour_payload* mynode){
	char msgbuf[MAXLINE],msgbuf2[MAXLINE];
	struct sockaddr_in addr;
	int addrlen,retval;
	char message[64];
	char name[5];
	
	gethostname(name,5);
	memset(msgbuf, 0, MAXLINE);
	addrlen = sizeof(addr);
	if ((retval=recvfrom(m_readsock,msgbuf,MAXLINE,0,(struct sockaddr *) &addr,&addrlen)) < 0) {
		perror("recvfrom");
		return -1;
	}
	//printf("ret val is %d\n",retval);
	//printf("\n----------------------------actual got \n",msgbuf);
	if(multicast_flag==0){
		printf("------------------------------------------------------------------------\n");
		printf("\n \t Node %s Received <FIRST MULTICAST MESSAGE> \n",name);
		printf("\n  %s\n",msgbuf);
		printf("------------------------------------------------------------------------\n");
		
		memset(msgbuf2, 0, MAXLINE);
		sprintf(msgbuf2,"<<<<<Node %s. I am a member of the group>>>>>>.",name);
		//printf("\nNode %s.Sending: %s",myname,msgbuf2);
		mcast_send(msgbuf2);
		multicast_flag=1;
		return 0;
	}

	
	if(multicast_flag==1){
		printf("------------------------------------------------------------------------\n");
		printf("\n \t Node %s  Received <SECOND MULTICAST MESSAGE> \n",name);
		printf("\n  %s\n",msgbuf);
		printf("------------------------------------------------------------------------\n");
		// counter++;
		// printf("\n counter value is %d \n",counter);
		 // printf("\n the tour length is %d\n",total_tour_length);
		// if(counter>=(total_tour_length)-1)
			// return (2);
			
		return 0;
	}
	
	
	
	return (2);
}


 void
 send_v4 (){
 
 char 				ppayload[PAYLOAD_SIZE]={0};
 char 				ip_hddr[IP_HEADER_SIZE]={0};
 char               icmp_hddr[100]={0};    
 char               eth_hdr[PF_PACKET_HEADER]={0};  
     
 int                len,status,packet_soc_fd = 0;
 

 //unsigned char 		dest_mac[6] = {0x00, 0x0c,0x29,0xd9,0x08,0xec};// this i will need from arp
 struct 	sockaddr_ll 	pack_addr;
 struct  	iphdr 	*ip_h = (struct iphdr *) ip_hddr;
 struct  	icmp   *icmp_h = (struct icmp *) icmp_hddr;
 struct  	ethhdr *eh =   (struct ethhdr *) eth_hdr;
 struct 	hwaddr_list *hwtemp; 
 //printf("\n inside send v4 \n");
 //printf("source ip %ld and dest is %ld",ping_source_ip,ping_dest_ip);
	if(multicast_flag==1)
	return ;
   list_for_each_entry(hwtemp, &(hwl.list), list){
   
	status = mac_filler(eth_hdr+ETH_ALEN,2);
    if (status != 0) {
		printf("\nStatus = %d, Unable to fill source mac of Destination !!! Exiting ...",status);
		exit(0);
	}
	eh->h_proto = htons(ETH_P_IP);
    len = 8 + datalen;           /* checksum ICMP header and data */
	ip_h->ihl = 5;
    ip_h->version = 4;
    ip_h->tos = 0;
	ip_h->tot_len =htons(20+len);
    ip_h->ttl = 255;
	ip_h->frag_off=0;
    ip_h->protocol = IPPROTO_ICMP;
    ip_h->id = htons(0);
    ip_h->saddr = ping_source_ip;
    ip_h->daddr = hwtemp->ip ;
    ip_h->check = 0;
	ip_h->check = in_cksum ((uint16_t *) ip_h, IP_HEADER_SIZE);
	
	
	icmp_h->icmp_type = ICMP_ECHO;
	icmp_h->icmp_code = 0;
	icmp_h->icmp_id = pid;
	icmp_h->icmp_seq = nsent++;
  
   
    
    memset (icmp_h->icmp_data, 0xa5, datalen); /* fill with pattern */
	Gettimeofday ((struct timeval *) icmp_h->icmp_data, NULL);
    icmp_h->icmp_cksum = 0;
	icmp_h->icmp_cksum = in_cksum ((u_short *) icmp_h, len);
	
	memcpy((void*)ppayload, (void*)eth_hdr, sizeof(struct ethhdr));
   	memcpy((void*)ppayload+PF_PACKET_HEADER, (void*)ip_hddr, sizeof(struct iphdr));
	memcpy((void*)ppayload+PF_PACKET_HEADER+IP_HEADER_SIZE, (void*) icmp_hddr,len);
	//ppayload[strlen(ppayload)]='\0';
	/* Create packet socket */
	// packet_soc_fd = socket(AF_PACKET, SOCK_RAW, PROTOCOL_NO_PF);
	// if (packet_soc_fd < 0) {
		// printf("\nError in creating socket !!!");
        // exit(0);	
	// }
	
	pack_addr.sll_family   = PF_PACKET;
	pack_addr.sll_protocol = htons(ETH_P_IP);
	pack_addr.sll_ifindex  = 2;
	pack_addr.sll_hatype   = ARPHRD_ETHER;
	pack_addr.sll_pkttype  = PACKET_OTHERHOST;
	pack_addr.sll_halen    = ETH_ALEN;

		memcpy(pack_addr.sll_addr,hwtemp->hw_addr,6);
		pack_addr.sll_addr[6]  = 0x00;
		pack_addr.sll_addr[7]  = 0x00;
		//printf("\n sending to : ");
		//print_hwaddr(pack_addr.sll_addr);
		
		 memcpy((void*)ppayload, hwtemp->hw_addr, 6);
		status = sendto(pf_sock,ppayload , 200, 0,
						(struct sockaddr *)&pack_addr, sizeof(pack_addr));
		if (status <= 0) {
			printf("\nError in sending payload !!!\n");
			exit(0);
		}
		//printf("\n sent data %d bytes\n",status);
		fflush(stdout);
 	}
 
 }
//------------------------------------------------------------------------------------------------------------------- 
 void
 sig_alrm (int signo)
 {    
 
   send_v4();

    alarm(1);
    return;
 }

 
  void
  proc_v4 (char *ptr, ssize_t len,struct timeval *tvrecv)
  {
      int     hlenl, icmplen;
      double  rtt;
      struct iphdr *ip;
      struct icmp *icmp;
	  char   iprec[50];
    struct timeval *tvsend;
	//char str[50];

     ip = (struct iphdr *) ptr;      /* start of IP header */
     hlenl = ip->ihl << 2;      /* length of IP header */
     if (ip->protocol != IPPROTO_ICMP)
         return;                  /* not ICMP */

     icmp = (struct icmp *) (ptr + hlenl);   /* start of ICMP header */
     if ( (icmplen = len - hlenl) < 8)
         return;                  /* malformed packet */

     if (icmp->icmp_type == ICMP_ECHOREPLY) {
         if (icmp->icmp_id != pid)
             return;                /* not a response to our ECHO_REQUEST */
         if (icmplen < 16)
             return;                /* not enough data to use */
         printf("\n ping recived ");
			 
			 
         tvsend = (struct  timeval  *) icmp->icmp_data;
         tv_sub (tvrecv, tvsend);
         rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;
          
		
         inet_ntop(AF_INET, &(ip->saddr), ( iprec), INET_ADDRSTRLEN);
         printf ("%d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n",
                 icmplen,iprec,icmp->icmp_seq, ip->ttl, rtt);
     
	 } 
 
 }
  
 
int 
 receive_ping(int sockfd){
   int     size;
   //char    recvbuf[BUFSIZE];
   char    payload[BUFSIZE];
   struct sockaddr_in	recvaddr;
   socklen_t           len;
   int   n;
   struct timeval tval;

   
   fflush(stdout);
   size = 60 * 1024;           /* OK if setsockopt fails */
   setsockopt (sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size));

  
 
       	n = recvfrom(sockfd, payload, 150, 0, (struct sockaddr *)&recvaddr, &len);
			if (n < 0) {
				printf("\nError in receiving packet from UNIX domain socket !!!\n");
	            return(-1);		
			}
			
			//printf("\n Received Payload");	
			
			
         Gettimeofday (&tval, NULL);
        proc_v4(payload, n, &tval);

  return(0);		

 }

int ping_start(struct tour_payload* mynode,char *hwaddrptr){


    //long 				dest_ip;
//	long                source_ip; 
   // int  				current_vm;
    struct hostent 		*host_IP = NULL;
	struct in_addr 		**addr_list = NULL;
	char                 ip[50];
//	int                  status;
	char 	dest_name[100] = {0};
	ping_source_ip = 0;
    //ping_dest_ip   = 0;
	char prev_name[10];
	int prev_key;
	char hw_addr_sendto[6];
	
	strncpy(hw_addr_sendto,hwaddrptr,6);
	//printf("\n inside star ping \n");

    
	ping_source_ip = mynode->tour_ip[(mynode->curr_index)];
	//printf("\n curr index is %d\n",mynode->curr_index);
	//printf("\n the name is %s\n",mynode->vm_list[(mynode->curr_index)].vm_name);                     //(mynode->curr_index)-1
	strncpy(prev_name,mynode->vm_list[(mynode->curr_index)-1].vm_name,10);
	//prev_name=mynode->vm_list[(mynode->curr_index)-1].vm_name;
	
	if(prev_name[3] == '0' && prev_name[2] == '1')
		prev_key = 10;
	else
		prev_key = prev_name[2] - '0';
	//printf("\ntill here okay\n");
	if(multicast_flag==0)	{
		if(already_pinged[prev_key]==0){	
	
			already_pinged[prev_key]++;
   
			sig_alrm (SIGALRM);         /* send first packet */
			//printf("after sig alarm \n");
			get_vmname( ping_dest_ip, dest_name);
			printf("%s \n",dest_name);
		
			fflush(stdout);

			host_IP = gethostbyname(dest_name);
			if (host_IP == NULL) { 
				printf("\nNo IP address associated with %s\n",dest_name);
				return(-1);			
			} 
			else {
				addr_list = (struct in_addr **)host_IP->h_addr_list;
			}
			pid = getpid() & 0Xffff;
			inet_ntop(AF_INET, & ((**addr_list).s_addr), (ip), INET_ADDRSTRLEN);
			printf("\n PING %s (%s): %d data bytes ",host_IP->h_name,ip,datalen);	
			//printf("source ip %ld and dest is %ld",ping_source_ip,ping_dest_ip);
				
			 send_v4 ();
		}
		else{
			printf("\n************* already pinging previous node which is %s*********** \n",mynode->vm_list[(mynode->curr_index)-1].vm_name);
		}
		
	}	
	else{
			printf("\n ||||||||||||||||||First Multicast received so stopping pinging|||||||||||||||\n");
			
	}
		return 0;   	

		
	

	
}




int 
route_packet_handler(int rt_sock, int m_sock, struct sockaddr_in *rt_src_addr)
{
	int retval, i, len,status;
	char buf[MAXLINE];
	void* bufptr ;
	//struct sockaddr_in rt_src_addr;
	struct iphdr *ip_hdr;
	time_t t;
	struct tour_payload *mynode;
	struct hostent *hptr;
	//unsigned int previp;
	char msg[MAXLINE];
	//pthread_t mythread[MAXLINE];
	//in_addr_t peeripaddr;
	struct hostent *hp = NULL;
	//char array1[32];
	char msg_send[MAXLINE];
	struct in_addr ptr;
	char *hw_addr_arp;
	int hw_addr_arp_len;
	char *ip_rcvd_from;
	struct hwaddr_list *hwlistele;
	int size;
	
	
	hw_addr_arp_len=6;
	hw_addr_arp=malloc(hw_addr_arp_len);
	ip_rcvd_from=malloc(14);
	mynode = (struct tour_payload *) malloc(sizeof(struct tour_payload));
	ip_hdr = (struct iphdr *) malloc(sizeof(struct iphdr));
	
	memset(buf,0,sizeof(struct tour_payload)+sizeof(struct iphdr));
	
	len = sizeof(struct sockaddr_in);
	retval = recvfrom(rt_sock, buf, (sizeof(struct tour_payload)+sizeof(struct iphdr)), 0, (struct sockaddr *)rt_src_addr, &len); 

	if (retval < 0) {
		perror("\nrt recv");
		return -1;
	}
	
	strncpy(ip_rcvd_from,(inet_ntoa(rt_src_addr->sin_addr)),14);
	//printf("printing from string received from %s\n",ip_rcvd_from);

	bufptr=(unsigned char*)buf;
	

	memcpy(mynode,bufptr+sizeof(struct iphdr), sizeof(struct tour_payload));
	printf("\n the tour length in handler%d\n",mynode->tour_length);
	total_tour_length=mynode->tour_length;
	ip_hdr = (struct iphdr *)buf;
	mynode->curr_index++;
	retval=sendto(un_sockfd, ip_rcvd_from, 14, 0, (SA*)&un_arpaddr, sizeof(un_arpaddr) );
	if(retval<0){
		printf("Error:arp process is probably not running\n");
		
	}
	Recvfrom(un_sockfd, hw_addr_arp, 6, 0 ,(SA*)&un_arpaddr, &hw_addr_arp_len);
	printf("\n Hardware addr recvd from arp is - \n");
	print_hwaddr(hw_addr_arp);
	hwlistele = (struct hwaddr_list*)malloc(sizeof(struct hwaddr_list));
	memcpy(hwlistele->hw_addr, hw_addr_arp, 6);
	ping_dest_ip= mynode->tour_ip[(mynode->curr_index)-1];
	//printf("\n mynode current index is :: %d\n",mynode->curr_index);
	
	memcpy(&hwlistele->ip, &ping_dest_ip, sizeof(in_addr_t));
	list_add(&(hwlistele->list), &(hwl.list));
	//add hw addr

	

	if (LOCAL_ID == ntohs(ip_hdr->id) ){
		time(&t);
		printf("\n \t SOURCE ROUTING PACKET RECEIVED \n");
		printf("--------------------------------------------------------------------------------\n");
		printf("\n%s \n", ctime(&t));
		ptr=rt_src_addr->sin_addr;
		hp = gethostbyaddr((const void*)&ptr, sizeof(ptr), AF_INET);
		
		printf(" : received source routing packet from %s\n", hp->h_name);
		printf("---------------------------------------------------------------------------------\n");
		//printf("\n entering ping_start\n");
			status = ping_start(mynode,hw_addr_arp);
			if (status < 0) {
				printf("\nError in Pinging !!!\n");
				return 0;
				}
		
		if(mynode->tour_length==(mynode->curr_index)){                                       //send multicast
			//strncpy(msg_send, array1, 32);
			//printf("\n\t the string is %s\n",msg_send);
			join_multicast(m_readsock, mynode);
			gethostname(name,5);
			sleep(5);
			// printf("<<<<< This is node %s",name); 
			// printf(". Tour has ended . Group members please identify yourselves. >>>>>\n");
			// printf("Node %s",name);
			
			printf("\n Node %s is sending first multicast message \n Sending: <FIRST MULTICAST MESSAGE>.\n",name);
			printf("Node %s ",name);
			sprintf(msg_send,"<<<<< This is node %s.Tour has ended .Group members please identify yourselves. >>>>>\n",name);
			size=(strlen(msg_send));
			//printf("the value for multicast is %d \n",size);
			mcast_send(msg_send);
			//multicast_flag=1;
		}
        else{
			join_multicast(m_readsock,mynode);
			tour(rt_sock,mynode);
		}
		
	}
}		
	















int 
main(int argc, char **argv){
	//struct ip_adrs_list* node;
	struct tour_payload * mynode;
	int ret,ret_mkstemp;
	char my_name[5];
	int i=0,k=0;
	in_addr_t x;
	int rt_sock,pg_sock;
	const int on=1;
	int optval,array[5];
	int maxfd,maxfdp;
	fd_set  rset;
	struct sockaddr_in multi_addr;
	struct sockaddr_in *rt_src_addr;
	char ip_rcvd_from[14];
	
	char cli_un_path[100];
	int status,p;
	struct timeval tv;
	tv.tv_sec = 1000;
	tv.tv_usec = 0;
	
	INIT_LIST_HEAD(&hwl.list);
	un_sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);
	bzero(&un_arpaddr, sizeof(un_arpaddr));
	bzero(&un_cliaddr, sizeof(un_arpaddr));
 
	un_arpaddr.sun_family = AF_LOCAL;
	un_cliaddr.sun_family = AF_LOCAL;
 
	strcpy(cli_un_path,CLI_UN_PATH);
	strcpy(un_arpaddr.sun_path, ARP_UN_PATH);
	ret_mkstemp=mkstemp(cli_un_path);
	strcpy(un_cliaddr.sun_path,cli_un_path);
	unlink(un_cliaddr.sun_path);
	
	
	Bind(un_sockfd, (SA *) &un_cliaddr, sizeof(un_cliaddr));
	//printf("\n ***********unix sock addr is %s ******* \n \n",un_cliaddr.sun_path);
	
	rt_src_addr=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
	//ip_rcvd_from=(char*)malloc(14);
	mynode=(struct tour_payload *)malloc(sizeof(struct tour_payload));
	//node = (struct ip_adrs_list *)malloc(sizeof(struct ip_adrs_list));
	
	Signal(SIGALRM, sig_alrm);   //for ping working

	ret = gethostname(my_name,sizeof(my_name));
	if ( ret == -1)
	{
		printf("\n Error in hostname \n");
		return -1;
	}
	
	if(my_name[3] == '0' && my_name[2] == '1')
		key = 10;
	else
		key = my_name[2] - '0';

	//printf("\n my key is %d\n", key);
	rt_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TOUR);
    if(rt_sock < 0){
        perror("\nError creating the route socket");
        return -1;
    }
		
	//printf("rt sock is %d\n", rt_sock);
    if( ret = setsockopt(rt_sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("\nError in setsockopt");
		return -1;
    }
	
	pg_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if(pg_sock < 0){
		perror("\nError creating ping RAW socket");
		return -1;
	}
	setsockopt(pg_sock, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int));
	//printf("entering select");
	pf_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	if (pf_sock < 0) {
		printf("\nError in creating pf socket \n");
		return -1;
	}	
	
	
	m_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(m_sock < 0) {
        printf("\nError in creating multicast sock \n");
        return -1;
        }

	m_readsock= socket(AF_INET, SOCK_DGRAM, 0);
	if(m_readsock < 0) {
		 printf("\nError in creating multicast read sock \n");
         return -1;
        }
		
	if (setsockopt(m_readsock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0) {
		perror("Reusing ADDR failed \n");
		return -1;
	}	

	if (setsockopt(m_readsock,SOL_SOCKET,IP_MULTICAST_LOOP,&on,sizeof(on)) < 0) {
		perror("Reusing ADDR failed \n");
		return -1;
	}	
	if (setsockopt(m_sock,SOL_SOCKET,IP_MULTICAST_LOOP,&on,sizeof(on)) < 0) {
		perror("Reusing ADDR failed \n");
		return -1;
	}	

	multi_addr.sin_family=AF_INET;
	multi_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	multi_addr.sin_port=htons(MULTICAST_PORT);

	if (bind(m_readsock,(struct sockaddr *) &multi_addr,sizeof(multi_addr)) < 0) {
		perror("Multicast read bind");
		return -1;
	}	
	for(i=0;i<=20;i++)
		mynode->already_visited[i]=0;
	
	if(argc>1){
	
		mynode->tour_ip[0]=get_ip(my_name);
		strncpy(mynode->vm_list[0].vm_name,my_name,8);
		//mynode->vm_list[0].vm_name=my_name;
		x=mynode->tour_ip[0];
		//printf("\n self added %s \n",(inet_ntoa(*(struct in_addr *)&x)));
		for (i = 1; i < argc; i++){	
			mynode->tour_ip[i] = (get_ip(argv[i]));
			strncpy(mynode->vm_list[i].vm_name,argv[i],8);
			//mynode->vm_list[i].vm_name=argv[i];
			//printf("\n host added to list is %s \n",mynode->vm_list[i].vm_name);
			x=mynode->tour_ip[i];
			//printf("\n added %s \n",(inet_ntoa(*(struct in_addr *)&x)));
					}
		
		for	(i=1;i<argc;i++){
			if( mynode->tour_ip[i-1] == mynode->tour_ip[i] ){
				printf("\n Invalid tour... Two consecutive nodes are equal \n");
				return -1;
			}
		}
		printf("\n The IP aaddress in the tour are\n");
		for (k=0;k<argc;k++){
			x=mynode->tour_ip[k];
			printf("\n %d) 		%s  \n	",(k+1),(inet_ntoa(*(struct in_addr *)&x)));
			}		
		
		

		total_tour_length=mynode->tour_length;
		mynode->curr_index = 0;
		mynode->tour_length= argc-1;
		mynode->port = MULTICAST_PORT;
		mynode->multicast_address = inet_addr(MULTICAST_ADDR);
		printf("\n\t  Starting tour\n");
		join_multicast(m_readsock, mynode);
		
		//printf("\n \t added to multicast");
		ret= tour(rt_sock, mynode);
		//printf("\n\t sent something");
	}

//printf("my  addr is %s\n", Sock_ntop((SA*)&dest_addr, sizeof(dest_addr)));

	maxfd=get_max_sock(rt_sock,pg_sock,m_readsock);
	FD_ZERO(&rset);

	while(1){
		FD_SET(rt_sock,&rset);
		FD_SET(pg_sock,&rset);
		FD_SET(m_readsock,&rset);


		ret = select(maxfd+1, &rset, NULL, NULL, &tv);
		if(ret < 0 && (errno==EINTR) )
			continue;
			
		if (ret<0){
			printf("\nError occured in select \n"); 
			exit(0);
		}
		if(ret==0){
				printf("\n Exiting after completion \n");
				exit (0);
		}

			
		
		if (FD_ISSET(rt_sock, &rset)) {
			//---recvfrom-----
			route_packet_handler(rt_sock,m_sock,rt_src_addr);
			//printf("\n received from %s \n",(inet_ntoa(rt_src_addr->sin_addr)));
			

		}	
		if (FD_ISSET(pg_sock, &rset)) {
			//printf("\n\t ping socket detected something \n");
			{
			status = receive_ping(pg_sock);
			if(status<0) {
				printf("could not receive");
				exit(0);
			}
		}
		}
		if (FD_ISSET(m_readsock, &rset)) {
			
			p=muticast_first_recieve(m_readsock,mynode);
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			if(p==2){
				printf("\n Exiting after completion \n");
				goto end;
				exit (0);
				}
		}
		// if(multicast_flag==2){
			// printf("<<<<<<<<<Node %s ",name);
			// printf("Received:<MULTICAST MESSAGE 2>\n");
			// printf("\n Exiting after completion \n");
			// sleep(1);
			// return 0;
		// }

	}		
end: 
close(rt_sock);
close(pg_sock);
close(m_sock);
close(m_readsock);
close(un_sockfd);
return 0;
}

