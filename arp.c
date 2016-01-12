#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_ether.h>
//#include <linux/if_arp.h>
#include <netpacket/packet.h>
#include <sys/time.h>
#include "unp.h"
#include "hw_addrs.h"
#include "list.h"
#include "arp.h"


int init_ll_var(){
		
	struct if_info *tmp;
	struct hwa_info *hwahead, *hwa;
	char hostname[5];

	gethostname(hostname, sizeof(hostname));

	hwahead = (struct hwa_info*) malloc(sizeof(struct hwa_info));
	hwa = (struct hwa_info*) malloc(sizeof(struct hwa_info));
	printf("-------------------------\n");
	printf("\t  %s\n", hostname);
	printf("-------------------------\n");
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next ){

		if( strncmp(hwa->if_name, "eth0", 4) == 0){

			printf("	Interface name is: %s\n", hwa->if_name);
			printf("	Hardware address is: " ) ;
			print_hwaddr (hwa->if_haddr);
			printf("	Interface index is: %d\n", hwa->if_index);
			printf("	Interface ip_alias is: %d\n", hwa->ip_alias);
			printf("	Interface IP addr is: %s\n", Sock_ntop(hwa->ip_addr, sizeof(SA*)) );
			printf("\n");
			self_if_idx = hwa->if_index;
			memcpy(self_hw_addr, hwa->if_haddr, 6);
			memcpy(self_ip_str, Sock_ntop(hwa->ip_addr, sizeof(SA*)), 14);
		}
	}
	printf("-------------------------\n");

	return 1;

}

int main(){
	
	int maxfd = -1;
	fd_set rset;
	
	char *req_ip_str;
	char *reply_hw_addr_str;
	int req_len = 14;
	void *buffer;
	
	char *src_hw_addr;
	struct request *reqtemp, *reqiter;
	int ret = 0, i = 0;
	struct sockaddr_ll recv_ll_addr;
	int recv_ll_addr_len = 0;
	struct arp_packet *arp_pack_rcvd;
	struct sockaddr_ll *sendto_addr;
	struct ethhdr* eh;
	reply_hw_addr_str = (char*)malloc(6);
	req_ip_str = malloc(14);
	//Unix socket
	//un_sockfd = Socket(AF_LOCAL, SOCK_STREAM, 0);
	un_sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);
	arp_un_addr.sun_family = AF_LOCAL;
	strcpy(arp_un_addr.sun_path, ARP_UN_PATH);
	unlink(ARP_UN_PATH);
	Bind(un_sockfd, (SA*) &arp_un_addr, sizeof(arp_un_addr));


	//Packet socket
	ll_sockfd = Socket(PF_PACKET, SOCK_RAW, htons(AA19_PROTO));
	self_hw_addr = malloc(6);
	src_hw_addr = malloc(6);
	
	//initialization
	INIT_LIST_HEAD(&cache.list);
	INIT_LIST_HEAD(&req.list);
	ret = init_ll_var();
	buffer = malloc(14 + sizeof(struct arp_packet));
	
	if(ll_sockfd > un_sockfd)
		maxfd = ll_sockfd;
	else
		maxfd = un_sockfd;

	FD_ZERO(&rset);
	while(1){
		FD_SET( un_sockfd, &rset);
		FD_SET( ll_sockfd, &rset);
		printf("Waiting for request...\n");
		Select(maxfd+1, &rset, NULL, NULL, NULL);


		if(FD_ISSET(ll_sockfd, &rset)){
			printf("Recieved link layer packet\n");
			ret = Recvfrom(ll_sockfd, buffer, 14 + sizeof(struct arp_packet), 0, 
							(SA*)&recv_ll_addr, &recv_ll_addr_len );
			memcpy(src_hw_addr, buffer+6, 6);
			//printf("ret is %d\n", ret);
			arp_pack_rcvd = (struct arp_packet*) malloc(sizeof(struct arp_packet));
			memcpy(arp_pack_rcvd, buffer+14, sizeof(struct arp_packet));
			//print_hwaddr(self_hw_addr);
			//print_hwaddr(arp_pack_rcvd->src_hw_addr_str);
			if(memcmp(self_hw_addr, arp_pack_rcvd->src_hw_addr_str, 6) != 0){
				printf("Query for hardware address recieved from \n");
				print_hwaddr(arp_pack_rcvd->src_hw_addr_str);
				printf("Hardware address will be sent as reply.\n");
				update_cache( arp_pack_rcvd->src_ip_str, arp_pack_rcvd->src_hw_addr_str);
				print_cache();
				if( memcmp(self_ip_str, arp_pack_rcvd->req_ip_str, 14) == 0){
					sendto_addr = (struct sockaddr_ll*)malloc(sizeof(struct sockaddr_ll));
					sendto_addr->sll_family = AF_PACKET;
					sendto_addr->sll_ifindex = self_if_idx;
					sendto_addr->sll_halen = IF_HADDR;
					sendto_addr->sll_protocol = htons(AA19_PROTO);
					memcpy(sendto_addr->sll_addr, src_hw_addr, IF_HADDR);
					sendto_addr->sll_addr[6] = 0x00;	
					sendto_addr->sll_addr[7] = 0x00;	
	
					//filling ethernet layer packet
					memcpy(buffer, src_hw_addr, 6);
					memcpy(buffer+6, self_hw_addr, 6);
					memcpy(arp_pack_rcvd->req_hw_addr_str, self_hw_addr, 6);
					memcpy(buffer+14, (void*)arp_pack_rcvd, sizeof(struct arp_packet));
					eh = buffer;
					eh->h_proto = htons(AA19_PROTO);
					Sendto(ll_sockfd, buffer, 14 + sizeof(struct arp_packet), 0, 
							(SA*)sendto_addr, sizeof(struct sockaddr_ll) );
					printf("Hardware addr reply sent successfully\n");
				}
			}
			else{
				//printf("at src\n");
				printf("Recieved reply to  hardware reply is: ");
				print_hwaddr(arp_pack_rcvd->req_hw_addr_str);
				update_cache( arp_pack_rcvd->req_ip_str, arp_pack_rcvd->req_hw_addr_str );	
				print_cache();
			}	
			list_for_each_entry_reverse(reqiter, &(req.list), list){
				if(reqiter->request_status == 1){
					ret = cache_hw_addr_lookup(reqiter->requested_ip_addr, reply_hw_addr_str);
					print_hwaddr(reply_hw_addr_str);
					ret = sendto( un_sockfd, reply_hw_addr_str, 6, 0, 
							(SA*)&tour_un_addr, sizeof(struct sockaddr_un));
					
				}
			}
		}

		if(FD_ISSET(un_sockfd, &rset)){
			printf("Recieved request from tour process\n");
			memset(&tour_un_addr, 0, sizeof(struct sockaddr_un));
			Recvfrom(un_sockfd, req_ip_str, req_len, 0, (SA*)&tour_un_addr, &tour_un_addr_len);
			running_request_idx++;
			
			printf("Requested ip str is %s\n", req_ip_str);
			ret = cache_hw_addr_lookup(req_ip_str, reply_hw_addr_str);
			print_cache();
			printf("Cache lookup performed\n");
			//printf("ret is %d\n", ret);
			if(ret == -1){
				printf("Entry not FOUND on cache lookup\n");
				reqtemp = (struct request*)malloc(sizeof(struct request));
				reqtemp->request_idx = running_request_idx;
				reqtemp->request_status = 1;
				memcpy(reqtemp->requested_ip_addr, req_ip_str, 14);
				memcpy(&reqtemp->request_un_addr, &tour_un_addr, sizeof(struct sockaddr_un));
				list_add(&(reqtemp->list), &(req.list));
			}
			else{
				printf("Entry found in cache table\n");
				printf("Sending tour application the hardware address\n");
				reqtemp = (struct request*)malloc(sizeof(struct request));
				reqtemp->request_idx = running_request_idx;
				reqtemp->request_status = 2;
				memcpy(reqtemp->requested_ip_addr, req_ip_str, 14);
				memcpy(&reqtemp->request_un_addr, &tour_un_addr, sizeof(struct sockaddr_un));
				list_add(&(reqtemp->list), &(req.list));
				//send entry to tour	
				ret = sendto( un_sockfd, reply_hw_addr_str, 6, 0, (SA*)&tour_un_addr, sizeof(struct sockaddr_un));
			}
		}

	}
	
	close(un_sockfd);
	close(ll_sockfd);
	return 0;
}
