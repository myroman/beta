#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
//#include "unp.h"
#include "unpthread.h"
#include "hw_addrs.h"
#include "misc.h"
#include "checksum.h"
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include "ping.h"
#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_ICMP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
//#include <linux/if_packet.h>
#include <linux/if_ether.h>
//#include <linux/if_arp.h>

#include <errno.h>            // errno, perror()

// Define some constants.

#define ICMP_HDRLEN 6 //was 8         // ICMP header length for echo request, excludes data
#define RT_PROTO 89
void tv_sub(struct timeval *out, struct timeval *in);
void dispatch(int rtSocket, int pgSocket, int pfpSocket);
int createRtSocket();
int createPgSocket();
int createPfPacketSocket();
int sendRtMsg(int sd);
int sendRtMsgIntermediate(int sd, void *buf, ssize_t len);
void processRtResponse(void *ptr, ssize_t len, SockAddrIn senderAddr, int pfpSocket);
void processPgResponse(void *buf, ssize_t len, SockAddrIn senderAddr, int addrlen);
void processMulticastRecv(void *buf, ssize_t len, SockAddrIn senderAddr, int addrlen) ;
void subscribeToMulticast(struct tourdata *unpack);
void tv_sub(struct timeval *out, struct timeval *in);
int myudp_client(in_addr_t multicastIp, in_port_t multicastPort, SA **saptr, socklen_t *lenp);

int VMcount;
struct in_addr * ip_list;
char* myNodeIP;
char* myNodeName;

int mcastSendSd = -1;
int mcastRecvSd = -1;
socklen_t salen;	
struct sockaddr *sasend;
int endOfTour = 0;
int lastPings = 0;

//Hold the nodes we are already pinging 
int numPinging = 0;
in_addr_t pinging_ip [10];
pthread_t ping_thread_ids[10]; 

int createRtSocket(){
	const int on = 1;
	int sd;
	struct ifreq ifr;
	char* interface = allocate_strmem (40);
	strcpy (interface, "eth0");
	// Submit request for a socket descriptor to look up interface.
	if ((sd = socket (AF_INET, SOCK_RAW, RT_PROTO)) < 0) {
		perror ("socket() failed to get socket descriptor for using ioctl() ");
		exit (EXIT_FAILURE);
	}

	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	if (ioctl (sd, SIOCGIFINDEX, &ifr) < 0) {
		perror ("ioctl() failed to find interface ");
		return (EXIT_FAILURE);
	}
	close (sd);	

	// Submit request for a raw socket descriptor.
	if ((sd = socket (AF_INET, SOCK_RAW, RT_PROTO)) < 0) {
		perror ("socket() failed ");
		exit (EXIT_FAILURE);
	}
	// Set flag so socket expects us to provide IPv4 header.
	if (setsockopt (sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof (on)) < 0) {
		perror ("setsockopt() failed to set IP_HDRINCL ");
		exit (EXIT_FAILURE);
	}

	// Bind socket to interface index.
	if (setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof (ifr)) < 0) {
		perror ("setsockopt() failed to bind to interface ");
		exit (EXIT_FAILURE);
	}
	free(interface);	
	return sd;
}

int createPgSocket(){
	int sd;
	struct ifreq ifr;
	char* interface = allocate_strmem (40);
	strcpy (interface, "eth0");
	// Submit request for a socket descriptor to look up interface.
	if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror ("socket() failed to get socket descriptor for using ioctl() ");
		exit (EXIT_FAILURE);
	}

	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	if (ioctl (sd, SIOCGIFINDEX, &ifr) < 0) {
		perror ("ioctl() failed to find interface ");
		return (EXIT_FAILURE);
	}
	close (sd);
	
	// Submit request for a raw socket descriptor.
	if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror ("socket() failed ");
		exit (EXIT_FAILURE);
	}
	
	// Bind socket to interface index.
	if (setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof (ifr)) < 0) {
		perror ("setsockopt() failed to bind to interface ");
		exit (EXIT_FAILURE);
	}
	free(interface);	
	return sd;
}

int createPfPacketSocket() {
	int sd;
	if ((sd = socket (PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
	    printf("Creating PF_PACKET socket failed\n");
	    return -1;
	}
	return sd;
}

int sendRtMsg(int sd){
	
	int status, datalen, *ip_flags;
	char *target, *src_ip, *dst_ip;
	struct ip iphdr;
	uint8_t *data, *packet;
	struct icmp icmphdr;
	struct addrinfo hints, *res;
	struct sockaddr_in *ipv4, sin;	
	void *tmp;

	// Allocate memory for various arrays.
	data = allocate_ustrmem (IP_MAXPACKET);
	packet = allocate_ustrmem (IP_MAXPACKET);
	target = allocate_strmem (40);
	src_ip = allocate_strmem (INET_ADDRSTRLEN);
	dst_ip = allocate_strmem (INET_ADDRSTRLEN);
	ip_flags = allocate_intmem (4);

	strcpy(src_ip, inet_ntoa(ip_list[0]));

	strcpy(target, inet_ntoa(ip_list[1]));
	debug("Source IP %s, targe IP %s", src_ip, target);

	// Fill out hints for getaddrinfo().
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = hints.ai_flags | AI_CANONNAME;

	// Resolve target using getaddrinfo().
	if ((status = getaddrinfo (target, NULL, &hints, &res)) != 0) {
		fprintf (stderr, "getaddrinfo() failed: %s\n", gai_strerror (status));
		exit (EXIT_FAILURE);
	}
	ipv4 = (struct sockaddr_in *) res->ai_addr;
	tmp = &(ipv4->sin_addr);
	if (inet_ntop (AF_INET, tmp, dst_ip, INET_ADDRSTRLEN) == NULL) {
		status = errno;
		fprintf (stderr, "inet_ntop() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}
	freeaddrinfo (res);	
	datalen = sizeof(struct tourdata) + (VMcount * 4);
	
	struct tourdata td;
	td.index = htonl(0);
	td.nodes_in_tour = htonl(VMcount);
	td.mult_ip = inet_addr(MULTICAST_IP);
	td.mult_port = htons(MULTICAST_PORT);
	memcpy(data, &td, sizeof(struct tourdata));

	subscribeToMulticast(&td);

	void * ptr = data;
	ptr = ptr + sizeof(struct tourdata);
	int i;
	//Add the IPs of the VMs to visit
	for(i = 1; i <=VMcount; i++){
		memcpy(ptr, &ip_list[i], 4);
		struct in_addr * temp2 = (struct in_addr *) ptr;
		ptr = ptr + 4;
	}

	// IPv4 header

	// IPv4 header length (4 bits): Number of 32-bit words in header = 5
	iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);	
	iphdr.ip_v = 4;// Internet Protocol version (4 bits): IPv4	
	iphdr.ip_tos = 0;// Type of service (8 bits)	
	iphdr.ip_len = htons (IP4_HDRLEN + ICMP_HDRLEN + datalen);// Total length of datagram (16 bits): IP header + UDP header + datalen	
	iphdr.ip_id = htons (MY_IP_ID);// ID sequence number (16 bits): unused, since single datagram

	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram
	ip_flags[0] = 0;  
	ip_flags[1] = 0;// Do not fragment flag (1 bit)  
	ip_flags[2] = 0;// More fragments following flag (1 bit)  
	ip_flags[3] = 0;// Fragmentation offset (13 bits)

	iphdr.ip_off = htons ((ip_flags[0] << 15)
	                  + (ip_flags[1] << 14)
	                  + (ip_flags[2] << 13)
	                  +  ip_flags[3]);	
	iphdr.ip_ttl = 255;// Time-to-Live (8 bits): default to maximum value	
	iphdr.ip_p = RT_PROTO;// Transport layer protocol (8 bits): 1 for ICMP

	// Source IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1) {
	fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
	exit (EXIT_FAILURE);
	}

	// Destination IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1) {
	fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
	exit (EXIT_FAILURE);
	}

	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
	iphdr.ip_sum = 0;
	iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);

	// ICMP header	
	icmphdr.icmp_type = 0;// Message Type (8 bits): echo request	
	icmphdr.icmp_code = 0;// Message Code (8 bits): echo request	
	icmphdr.icmp_id = htons (RT_ICMPID);// Identifier (16 bits): usually pid of sending process - pick a number	
	icmphdr.icmp_seq = htons (0);// Sequence Number (16 bits): starts at 0	
	icmphdr.icmp_cksum = 0;
	icmphdr.icmp_cksum = icmp4_checksum(icmphdr, data, datalen);

	// Prepare packet.
	// First part is an IPv4 header.
	memcpy (packet, &iphdr, IP4_HDRLEN);
	// Next part of packet is upper layer protocol header.
	memcpy ((packet + IP4_HDRLEN), &icmphdr, ICMP_HDRLEN);
	// Finally, add the ICMP data.
	memcpy (packet + IP4_HDRLEN + ICMP_HDRLEN, data, datalen);
	// Calculate ICMP header checksum
	//icmphdr.icmp_cksum = 0;//checksum ((uint16_t *) (packet + IP4_HDRLEN), ICMP_HDRLEN + datalen);
	//memcpy ((packet + IP4_HDRLEN), &icmphdr, ICMP_HDRLEN);

	// The kernel is going to prepare layer 2 information (ethernet frame header) for us.
	// For that, we need to specify a destination for the kernel in order for it
	// to decide where to send the raw datagram. We fill in a struct in_addr with
	// the desired destination IP address, and pass this structure to the sendto() function.
	memset (&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;  

	if (sendto (sd, packet, IP4_HDRLEN + ICMP_HDRLEN + datalen, 0, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)  {
		perror ("sendto() failed ");
		exit (EXIT_FAILURE);
	}
	printOK();
	
	// Free allocated memory.
	free (data);
	free (packet);
	free (target);
	free (src_ip);
	free (dst_ip);
	free (ip_flags);

	return 0;
}

int sendRtMsgIntermediate(int sd, void * buf, ssize_t len){
	
	int status, *ip_flags;
	struct ip iphdr;
	uint8_t *data, *packet;
	struct icmp icmphdr;
	struct sockaddr_in *ipv4, sin;	
	void *tmp;

	// Allocate memory for various arrays.
	data = allocate_ustrmem (IP_MAXPACKET);
	packet = allocate_ustrmem (IP_MAXPACKET);
	ip_flags = allocate_intmem (4);
	//Unpack the tourdata 
	struct tourdata * unpack = (struct tourdata *)buf;

	subscribeToMulticast(unpack);

	//since we work and change index
	unpack->index = ntohl(unpack->index);
	printf("Index:%d, nodes in tour:%d\n", unpack->index, ntohl(unpack->nodes_in_tour));
	if((unpack->index + 1) == ntohl(unpack->nodes_in_tour)){
		//Tour has ended. Send multicast here to everyone and return actually 
		//you dont send anyting from here
		endOfTour = 1;		

		free (data);
		free (packet);
		free (ip_flags);
		return 0;		
	}			

	int itemsArrSizeBytes = ntohl(unpack->nodes_in_tour) * sizeof(struct in_addr);
	int datalen = sizeof(struct tourdata) + itemsArrSizeBytes;
	
	// IPv4 header

	// IPv4 header length (4 bits): Number of 32-bit words in header = 5
	iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);	
	iphdr.ip_v = 4;// Internet Protocol version (4 bits): IPv4	
	iphdr.ip_tos = 0;// Type of service (8 bits)	
	iphdr.ip_len = htons (IP4_HDRLEN + ICMP_HDRLEN + datalen);// Total length of datagram (16 bits): IP header + UDP header + datalen	
	iphdr.ip_id = htons (MY_IP_ID);// ID sequence number (16 bits): unused, since single datagram
debug("hey");
	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram
	ip_flags[0] = 0;  
	ip_flags[1] = 0;// Do not fragment flag (1 bit)  
	ip_flags[2] = 0;// More fragments following flag (1 bit)  
	ip_flags[3] = 0;// Fragmentation offset (13 bits)

	iphdr.ip_off = htons ((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) +  ip_flags[3]);	
	iphdr.ip_ttl = 255;// Time-to-Live (8 bits): default to maximum value	
	iphdr.ip_p = RT_PROTO;// Transport layer protocol (8 bits): 1 for ICMP
debug("hey");
	void * ptr = buf + sizeof(struct tourdata);
	ptr = ptr + (sizeof(struct in_addr) * unpack->index); //current node-item
	iphdr.ip_src = *(struct in_addr *)ptr;
	
	unpack->index++;
	ptr = ptr + sizeof(struct in_addr); //advance the pointer to the next in_addr_t
	iphdr.ip_dst  = *(struct in_addr *)ptr;
	debug("hey. Src IP :%s\n", printIPHuman(iphdr.ip_src.s_addr));
	debug("hey. Dest IP :%s\n", printIPHuman(iphdr.ip_dst.s_addr));
	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
	iphdr.ip_sum = 0;
	iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);

	// ICMP header	
	icmphdr.icmp_type = 0;// Message Type (8 bits): echo request	
	icmphdr.icmp_code = 0;// Message Code (8 bits): echo request	
	icmphdr.icmp_id = htons (RT_ICMPID);// Identifier (16 bits): usually pid of sending process - pick a number	
	icmphdr.icmp_seq = htons (0);// Sequence Number (16 bits): starts at 0	
	icmphdr.icmp_cksum = 0;
	icmphdr.icmp_cksum = icmp4_checksum(icmphdr, data, datalen);

	// Copy tour packet to the new ICMP data payload
	unpack->index = htonl(unpack->index);
	memcpy(data, unpack, sizeof(struct tourdata));

	memcpy((void*)(data + sizeof(struct tourdata)), (void*)(buf + sizeof(struct tourdata)), itemsArrSizeBytes);	
debug("hey itemarrsizebytes %d", itemsArrSizeBytes);
	// Prepare packet.
	// First part is an IPv4 header.
	memcpy (packet, &iphdr, IP4_HDRLEN);
	// Next part of packet is upper layer protocol header.
debug("hey");
	memcpy ((packet + IP4_HDRLEN), &icmphdr, ICMP_HDRLEN);
	// Finally, add the ICMP data.
	debug("hey");
	memcpy (packet + IP4_HDRLEN + ICMP_HDRLEN, data, datalen);
	debug("hey, datalen=%d", datalen);
	memset (&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;  
	debug("hey. Dest IP :%s\n", printIPHuman(ntohl(iphdr.ip_dst.s_addr)));
	printf("Gonna send...");
	// Send packet.
	if (sendto (sd, packet, IP4_HDRLEN + ICMP_HDRLEN + datalen, 0, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)  {
		printFailed();
		return 1;		
	}
	printOK();
	
	// Free allocated memory.
	free (data);
	free (packet);
	free (ip_flags);

	return 0;
}


int createUDPSocket(){
	//Create a UDP socket fro multicasting 
	//May need two UDP sockets look at page 576
	return 0;
}

void getNodeName(){
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	struct hostent *hptr;
	for (hwahead = hwa = get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

		if(strcmp(hwa->if_name, "eth0") == 0 || strcmp(hwa->if_name, "wlan0") == 0 ){
			if ( (sa = hwa->ip_addr) != NULL)
				myNodeIP = Sock_ntop_host(sa, sizeof(*sa));
		}
	}
	myNodeIP = Sock_ntop_host(sa, sizeof(*sa));
	if(inet_aton(myNodeIP, &ip_list[0]) != 0){
			if((hptr = gethostbyaddr(&ip_list[0], sizeof(struct in_addr), AF_INET)) == NULL){
                    err_quit ( "gethostbyaddress error for IP address: %s: %s" , myNodeIP, hstrerror(h_errno));
            		debug("errno: %s", strerror(errno));
            }
            printf("The server host name is %s.\n", hptr->h_name);
            myNodeName =(char*) malloc(strlen(hptr->h_name));
            strcpy(myNodeName,hptr->h_name);
    }    
}

void fillIpList(int argc, char **argv){
	int i, VMcount;
	VMcount = argc - 1;
	//info("Number of VMs passed: %d", (VMcount));
	for(i = 1; i < argc; i++){
		struct hostent *hptr;
		hptr = gethostbyname(argv[i]);
		char str[INET_ADDRSTRLEN];
		inet_ntop(hptr->h_addrtype, hptr-> h_addr_list[0],str, sizeof(str));
		ip_list[i].s_addr =  inet_addr(str);
		info("%d: %s", i, argv[i]);
	}
	for(i = 1; i < argc; i++){
		//debug("%d: %s", i, inet_ntoa(*(ip_list+i)));
	}
}

int main(int argc, char ** argv){
	int i;//, VMcount;
	VMcount = argc - 1;
	//0th index will house the IP of the current node that it is running on
	ip_list= (struct in_addr *) malloc(sizeof(struct in_addr) * (VMcount + 1));
	
	getNodeName();
	fillIpList(argc, argv);
	
	int pgSocket = createPgSocket();
	int rtSocket = createRtSocket();
	int pfpSocket = createPfPacketSocket();
	
	// create multicast sockets
	if (VMcount > 0) {			
		sendRtMsg(rtSocket);			
	}

	dispatch(rtSocket, pgSocket, pfpSocket);	

	free(ip_list);
	free(myNodeName);
	return 0;
}

void dispatch(int rtSocket, int pgSocket, int pfpSocket) {
	fd_set set;
	int maxfd;
	struct timeval tv;	
	void* buf = malloc(MAXLINE);
	SockAddrIn senderAddr;	
	int addrLen = sizeof(senderAddr),
	res = 0;

	debug("Gonna listen to rtsocket: %d", rtSocket);
	for(;;){
		if (endOfTour == 0) {
			tv.tv_sec = 20;
		} 
		else {
			tv.tv_sec = 5;			
		}
		
		tv.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(rtSocket, &set);
		FD_SET(pgSocket, &set);
		maxfd = max(rtSocket, pgSocket);
		if (mcastRecvSd > 0) {
			FD_SET(mcastRecvSd, &set);
			maxfd = max(maxfd, mcastRecvSd);
		}
		res = select(maxfd + 1, &set, NULL, NULL, &tv);
		if (res == 0) {
			if (endOfTour == 1) {
				printf("Auf Wiedersehen!\n");
				return;
			}
			debug("Nothing read. Timeout.");				
			continue;
		}
		
		// if received a rt
		if(FD_ISSET(rtSocket, &set)){
			// let's choose some smaller value than 1500 bytes.
			bzero(buf, MAXLINE);
			bzero(&senderAddr, addrLen);
		 	addrLen = sizeof(senderAddr);
			int length = recvfrom(rtSocket, buf, MAXLINE, 0, (SA* )&senderAddr, &addrLen);
			if (length == -1) { 
				printFailed();								
			}
			char* ipr = inet_ntoa(senderAddr.sin_addr);			
			debug("Got rt packet from %s, length = %d", ipr, length);									
			processRtResponse(buf, length, senderAddr, rtSocket);			
		}
		// if received a ping
		if (FD_ISSET(pgSocket, &set)){
			bzero(buf, MAXLINE);
			bzero(&senderAddr, addrLen);
		 	addrLen = sizeof(senderAddr);
			int length = recvfrom(pgSocket, buf, MAXLINE, 0, (SA* )&senderAddr, &addrLen);
			if (length == -1) { 
				printFailed();								
			}

			processPgResponse(buf, length, senderAddr, addrLen);
		}

		if (mcastRecvSd > 0 && FD_ISSET(mcastRecvSd, &set)){
			bzero(buf, MAXLINE);
			bzero(&senderAddr, addrLen);
		 	addrLen = sizeof(senderAddr);
			int length = recvfrom(mcastRecvSd, buf, MAXLINE, 0, (SA* )&senderAddr, &addrLen);
			if (length == -1) { 
				printFailed();								
			}
			processMulticastRecv(buf, length, senderAddr, addrLen);

		}	
	}
	free(buf);
}

void processRtResponse(void *ptr, ssize_t len, SockAddrIn senderAddr, int rtsd)
{
	int	icmplen;
	struct timeval	*tvsend;
	char buff[MAXLINE];	
	struct ip *ip = (struct ip *) ptr;		/* start of IP header */	
	if (ip->ip_p != RT_PROTO)
		return;				/* not RT */

	int ipheaderlen = ip->ip_hl << 2;		/* length of IP header */	
	struct icmp	*icmp = (struct icmp *) (ptr + ipheaderlen);	/* start of ICMP header */
	if ( (icmplen = len - ipheaderlen) < 8)
		return;				/* malformed packet */
	if (ntohs(icmp->icmp_id) != RT_ICMPID) {
		return;
	}

	time_t ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s", ctime(&ticks));
	struct hostent *hptr;
	//printf("%s received source routing packet from %s.icmp type=%d, id=%d\n", buff, inet_ntoa(senderAddr.sin_addr), ntohs(icmp->icmp_type), ntohs(icmp->icmp_id));
	if((hptr = gethostbyaddr(&(senderAddr.sin_addr), sizeof (senderAddr.sin_addr), AF_INET)) == NULL)
        err_quit ( "gethostbyaddress error");            
	printf("%s received source routing packet from %s. icmp type=%d, id=%d,imcplen=%d\n", buff, hptr->h_name, icmp->icmp_type, ntohs(icmp->icmp_id), icmplen);	
	
	//TODO:  Send down the chain OR MULTICAST here
	sendRtMsgIntermediate(rtsd, ptr + ipheaderlen + ICMP_HDRLEN, len);

	//TODO:	check if we are already pinging the node
	//If we are then just return

	int i;
	for(i = 0; i < 10; i++){
		if(senderAddr.sin_addr.s_addr == pinging_ip[i]){
			debug("Match Found. Dont ping again");
			return;
		}
	}
	pinging_ip[numPinging] = senderAddr.sin_addr.s_addr;
	

	//TODO: pthread create this bad boy
	pthread_t tid;
	struct paramsToPing p;
    p.srcIp = ip_list[0];
    p.destAddr= (struct sockaddr *)&senderAddr;
    p.sockaddrlen = sizeof(senderAddr);
    //struct paramsToPing * p_ptr = &p;
    if( (pthread_create(&tid, NULL, &sendPing, (void*) &p)) != 0)
    	err_quit("Error creating a thread for pinging");
    
    ping_thread_ids[numPinging] = tid;
    numPinging++;
    //pthread_create(&tid, NULL, &sendPing, &p);
	//sendPing(ip_list[0], (struct sockaddr *)&senderAddr, sizeof(senderAddr));
}

void processPgResponse(void *ptr, ssize_t len, SockAddrIn senderAddr, int addrlen) {
	int icmplen;
	struct ip *ip = (struct ip *) ptr;		/* start of IP header */	
	
	if (ip->ip_p != IPPROTO_ICMP)
		return;				/* not ICMP */
	int ipheaderlen = ip->ip_hl << 2;		/* length of IP header */	
	struct icmp	*icmp = (struct icmp *) (ptr + ipheaderlen);	/* start of ICMP header */
	if ( (icmplen = len - ipheaderlen) < 8)
		return;				/* malformed packet */

	if (icmp->icmp_type != 0) {
		return;
	}
	if (ntohs(icmp->icmp_id) != PING_ICMPID) {
		return;
	}
	
	if (endOfTour == 1) {
		lastPings++;
		if (lastPings == 5) {
			char finalMsg[MAXLINE];
			int i = 0;
			for (i = 0; i < numPinging;++i) {
				pthread_cancel(ping_thread_ids[i]);	
			}

			sprintf(finalMsg, "<<<<< This is node %s . Tour has ended. Group members please identify yourselves. >>>>>", myNodeName);
			printf("Node %s. Sending: %s\n", myNodeName, finalMsg);
			sendto(mcastSendSd, finalMsg, MAXLINE, 0, sasend, salen);
		}
	}

	struct timeval	*tvsend;
	struct timeval tvrecv;
	Gettimeofday(&tvrecv, NULL);
	
	tvsend = (struct timeval *) icmp->icmp_data;
	tv_sub(&tvrecv, tvsend);
	double rtt = tvrecv.tv_sec * 1000.0 + tvrecv.tv_usec / 1000.0;

	printf("%d bytes from %s: seq=%u, ttl=%d, rtt=%.3f ms\n",
			icmplen, Sock_ntop_host((SA *)&senderAddr, addrlen),
			icmp->icmp_seq, ip->ip_ttl, rtt);
}

void tv_sub(struct timeval *out, struct timeval *in)
{
	if ( (out->tv_usec -= in->tv_usec) < 0) {	/* out -= in */
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

int myudp_client(in_addr_t multicastIp, in_port_t multicastPort, SA **saptr, socklen_t *lenp)
{
	int sockfd, n;
	struct addrinfo	hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	struct in_addr hostAddr;
	hostAddr.s_addr = multicastIp;
	char* ipAddr = inet_ntoa(hostAddr);
	char port[6];
	sprintf(port, "%d", ntohs(multicastPort));
	port[5] = '\0';
	
	if ( (n = getaddrinfo(ipAddr, port, &hints, &res)) != 0)
		err_quit("udp_client error for %s, %s: %s",
				 ipAddr, port, gai_strerror(n));
	ressave = res;
	do {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd >= 0)
			break;		/* success */
	} while ( (res = res->ai_next) != NULL);
	if (res == NULL)	/* errno set from final socket() */
		err_sys("udp_client error for %s, %s", ipAddr, port);
	*saptr = Malloc(res->ai_addrlen);
	memcpy(*saptr, res->ai_addr, res->ai_addrlen);
	*lenp = res->ai_addrlen;
	freeaddrinfo(ressave);
	return(sockfd);
}

void subscribeToMulticast(struct tourdata *unpack) {
	const int on = 1;	
	debug("hey");
	if (mcastRecvSd > 0) {
		return; // we've already subsc.
	}

	mcastSendSd = myudp_client(unpack->mult_ip, unpack->mult_port, (SA **)&sasend, &salen);
	debug("hey");
	mcastRecvSd = Socket(sasend->sa_family,  SOCK_DGRAM, 0);
	Setsockopt(mcastRecvSd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	struct sockaddr *sarecv = Malloc(salen);
	memcpy(sarecv, sasend, salen);
	Bind(mcastRecvSd, sarecv, salen);
	Mcast_join(mcastRecvSd, sasend, salen, NULL, 0);

	debug("I subscribed\n");
}

void processMulticastRecv(void *buf, ssize_t len, SockAddrIn senderAddr, int addrlen) {	
	printf("Node %s. Received %s\n", myNodeName, (char*)buf);
	
	if (endOfTour != 1) {
		// stop pinging
		char msg[MAXLINE];
		int i = 0;
		for (i = 0; i < numPinging;++i) {
			pthread_cancel(ping_thread_ids[i]);					
		}

		sprintf(msg, "<<<<< Node %s. I am a member of the group. >>>>>", myNodeName);
		printf("Node %s. Sending: %s\n", myNodeName, msg);	
		sendto(mcastSendSd, msg, MAXLINE, 0, sasend, salen);
	}
	endOfTour = 1;
}