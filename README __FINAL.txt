CSE 533: Network Programming
Assignment 4
--------------------------------------
Anish Ahmed 
SBU ID: 110560809
Netid: aniahmed
Pratik Shrivastav
SBU ID: 110385785
Netid: pshrivastav cse533-6
--------------------------------------

ABOUT
-----
ARP and Ping functionalities are implemented in the assignment.
This implementation involves use of PF_PACKET sockets, unix socket and IP raw sockets.
The PING functionality requires ethernet address to send ICMP ECHO REQUEST.
The API areq is used to achieve the communication of ethernet address from ARP module to Tour by means of UNIX sockets.
The ARP itself uses PF_PACKET socket for the ethernet address lookup.
The reply from ARP is conveyed via UNIX socket.
Once the ethernet address of previous VM is obtined then the ICMP ECHO REQUEST is sent in a Ethernet packet.
The ICMP ECHO REPLY is obtained on the IP raw socket.
While travelling every VM in the our it is being added to the Multicast Group.
Once the tour is over the last node sends a MULTICAST message to all the members in the group.
As soon as the members receive the MULTICAST message they stop pinging and themselves send a MULTICAST message.



FILES INCLUDED
--------------
1. Makefile
2. arp.h
3. arp.c
4. tour.c
5. list.h
6. hw_addrs.h
7. get_hw_addrs_aa19.c
8. prhwaddrs_aa19.c 


COMPILATION and RUNNING
-----------------------
Run - 
	make
To compile the required files.


Run-
	./deploy_app tour arp
To deploy the apps on all vms.


1. Start arp on all VM's by ./arp
2. Start tour on all VM's except the starting node by entering ./tour
3. Start tour from the VM where thre tour is to begin by entering ./tour (followed by sequence of VM's example vm2 vm3 vm4 ....)
   Example: on VM1 start tour by entering ./tour vm2 vm4 vm3 vm7 vm2


Run- 
	make clean to remove all the .o and excecutables.


MESSAGES PRINTED DURING MULTICAST:

	When the tour reaches the last node it starts pinging the previous node and waits for some reply and then 
	it sends the multicast message to all the group members . The message is
	
	<<<<< This is node vmi . Tour has ended . Group members please identify yourselves. >>>>>
	
	It also prints this while sending
	
	----Node vmi . Sending: <then print out the message sent>.
	
	This message is received by all the nodes who print it as 

	----Node vmj . Received <then print out the message received>.	
	
	Each node stops its pinging and prints the received multicast message .
	
	----Node vmk . Received: <then print out the message received>.
	



SYSTEM DOCUMENTATION
--------------------

ARP module - 
	The ARP process recieves a request from tour process running on the vm. The tour process requests hardware address for the node to which it wants to send ping. The tour process establishes a SOCK_STREAM connection with the arp process and sends the ip address of the node whose address it wants over the UNIX domain socket. 
	When ARP recieves the request from the tour process, it first queries its cache table to see if an entry is available. If an entry is available then it responds to the tour process request with the hardware address entry from the cache.
	If a cache entry is not present then the ARP module broadcasts on eth0 a PF_PACKET. Every node on the network revieves this broadcast. The node whose IP address is same as the requested adress by the tour process responds to the broadcast. Every other node just adds the entry for the source node to it's cache.
	After the response is recieved by the source node, it adds the hardware address and ip address mapping in its cache.


	DATA STRUCTURES:
	The structure of the elements in the ARP cache are of following type - 
		struct arp_cache {
			    char ip_str[14];
			    char hw_addr[6];
			    time_t ts;
			    struct list_head list;
			}cache;

	The structure of the PF_PACKET request is - 
			struct arp_packet {
			    char src_ip_str[14];
			    char src_hw_addr_str[6];
			    char req_ip_str[14];
			    char req_hw_addr_str[6];
			}arp_pack;

	list.h is used to create list based on the list provided by Linux Kernel.
	(list.h has been picked up from http://www.mcs.anl.gov/~kazutomo/list/list.h)


TOUR-
	
	Tour process recieves a packet form the previous node in the tour and starts sending ping back to the previous node.
	This ping is replied to by the previous node and is displayed at STDOUT. All the tour nodes add themselves to a multicast group.
	When the tour reaches the last node it sends a message to the multicast group to which all the tour nodes were added.
	When all the nodes in the multicast gourp recieve this multicast message they send out a multicast message to the multicast gourp and stop sending ping to the previous nodes.


	DATA STRUCTURES:

	The information regarding tour is stored in the following struct. An object of this struct is passed in a packet - 
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
		

	





