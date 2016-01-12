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