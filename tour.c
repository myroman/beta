#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include "unp.h"
#include "hw_addrs.h"
#include "misc.h"
#include "checksum.h"
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()

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
void processRtResponse(char *ptr, ssize_t len, SockAddrIn senderAddr, int pfpSocket);
void processPgResponse(char *buf, ssize_t len, SockAddrIn senderAddr, int addrlen);
void sendPing();
void tv_sub(struct timeval *out, struct timeval *in);
struct in_addr * ip_list;
char* myNodeIP;
char* myNodeName;

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

	// Interface to send packet through.
	strcpy (src_ip, "130.245.156.21\0");
	strcpy (target, "130.245.156.22\0");

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

	// ICMP data
	datalen = 4;
	data[0] = 'T';
	data[1] = 'e';
	data[2] = 's';
	data[3] = 't';

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
	memcpy ((packet + IP4_HDRLEN), &icmphdr, ICMP_HDRLEN);

	// The kernel is going to prepare layer 2 information (ethernet frame header) for us.
	// For that, we need to specify a destination for the kernel in order for it
	// to decide where to send the raw datagram. We fill in a struct in_addr with
	// the desired destination IP address, and pass this structure to the sendto() function.
	memset (&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;  

	printf("Gonna send...");
	// Send packet.
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
	//myNodeName = findNameofVM(myNodeIP);
	if(inet_aton(myNodeIP, &ip_list[0]) != 0){
            //if((hptr = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET)) == NULL)
			if((hptr = gethostbyaddr(&ip_list[0], sizeof(struct in_addr), AF_INET)) == NULL){
                    err_quit ( "gethostbyaddress error for IP address: %s: %s" , myNodeIP, hstrerror(h_errno));
            		debug("errno: %s", strerror(errno));
            }
            printf("The server host name is %s.\n", hptr->h_name);
            myNodeName =(char*) malloc(strlen(hptr->h_name));
            strcpy(myNodeName,hptr->h_name);
            //strcpy(myNodeName, myNodeIP);//copy the string into string
    }
    //TODO: ntohl the 32 bit ip
    ip_list[0].s_addr = ntohl(ip_list[0].s_addr);
    debug("%s %s", myNodeName, myNodeIP);
    debug("%d: %s", 0, inet_ntoa((ip_list[0])));
}

void fillIpList(int argc, char **argv){
	int i, VMcount;
	VMcount = argc - 1;
	info("Number of VMs passed: %d", (VMcount));
	for(i = 1; i < argc; i++){
		struct hostent *hptr;
		hptr = gethostbyname(argv[i]);
		char str[INET_ADDRSTRLEN];
		inet_ntop(hptr->h_addrtype, hptr-> h_addr_list[0],str, sizeof(str));
		debug("%s", str);
		//TODO: store as network byte order
		ip_list[i].s_addr =  inet_addr(str);
		ip_list[i].s_addr = htonl(ip_list[i].s_addr);
		info("%d: %s", i, argv[i]);
	}
	for(i = 1; i < argc; i++){
		debug("%d: %s", i, inet_ntoa(*(ip_list+i)));
	}
}

int main(int argc, char ** argv){
	
	int i, VMcount;
	VMcount = argc - 1;
	//0th index will house the IP of the current node that it is running on
	ip_list= (struct in_addr *) malloc(sizeof(struct in_addr) * (VMcount + 1));
	
	getNodeName();
	fillIpList(argc, argv);
	
	int pgSocket = createPgSocket();
	int rtSocket = createRtSocket();
	int pfpSocket = createPfPacketSocket();
	if (VMcount > 0) {			
		sendRtMsg(rtSocket);
		//if we set some input args, let's send PF_PACKET ICMP msg to vm1
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
	char* buf = malloc(MAXLINE);
	SockAddrIn senderAddr;	
	int addrLen = sizeof(senderAddr),
	res = 0;

	debug("Gonna listen to rtsocket: %d", rtSocket);
	for(;;){
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(rtSocket, &set);
		FD_SET(pgSocket, &set);

		maxfd = getmax(rtSocket, pgSocket) + 1;
		printf("Waiting for incoming packets...\n");
		res = select(maxfd, &set, NULL, NULL, &tv);
		if (res < 0) {
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
			processRtResponse(buf, length, senderAddr, pfpSocket);			
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
		
	}
	free(buf);
}

void processRtResponse(char *ptr, ssize_t len, SockAddrIn senderAddr, int pfpSocket)
{
	int	icmplen;
	struct timeval	*tvsend;
	char buff[MAXLINE];	
	struct ip *ip = (struct ip *) ptr;		/* start of IP header */	
	if (ip->ip_p != RT_PROTO)
		return;				/* not RT */

	int hlen1 = ip->ip_hl << 2;		/* length of IP header */	
	struct icmp	*icmp = (struct icmp *) (ptr + hlen1);	/* start of ICMP header */
	if ( (icmplen = len - hlen1) < 8)
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
	
	sendPing((struct sockaddr*)&senderAddr, sizeof(senderAddr));
}
void processPgResponse(char *ptr, ssize_t len, SockAddrIn senderAddr, int addrlen) {
	int icmplen;
	struct ip *ip = (struct ip *) ptr;		/* start of IP header */	
	
	if (ip->ip_p != IPPROTO_ICMP)
		return;				/* not ICMP */
	int hlen1 = ip->ip_hl << 2;		/* length of IP header */	
	struct icmp	*icmp = (struct icmp *) (ptr + hlen1);	/* start of ICMP header */
	if ( (icmplen = len - hlen1) < 8)
		return;				/* malformed packet */
	if (icmp->icmp_type != 0) {
		return;
	}
	if (ntohs(icmp->icmp_id) != RT_ICMPID) {
		return;
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
