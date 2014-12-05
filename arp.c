#include "unp.h"
#include "arp.h"
#include "misc.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO
#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <netinet/in.h>       // IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/udp.h>      // struct udphdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.

#include <errno.h>            // errno, perror()

//This is the pointer to the set IP to hardware address pairs of eth0
Set *headSet = NULL;
Set *tailSet = NULL;

//This is the pointer to the cache entries 
CacheEntry *headCache = NULL;
CacheEntry *tailCache = NULL;

Set * allocateSet(){
	Set * toRet = (Set*) malloc(sizeof(Set));
	toRet->next = NULL;
	//Empty set
	if(headSet == NULL && tailSet == NULL){
		headSet = toRet;
		tailSet = toRet;
	}
	else if(tailSet != NULL){
		tailSet = toRet;
	}
	else{
		debug("Code shouldn't execute");
		return NULL;
	}
	return toRet;
}
void freeSet(){
	Set * ptr = headSet;
	while(ptr!= NULL){
		free(ptr);
		ptr= ptr->next;
	}
}
void printAddressPair(Set *pair){
	struct in_addr toPrint;
	int    i, prflag;
	char   *ptr;

	toPrint.s_addr = pair->ip;
	printf("*** Address Pair Found ***\n");
	//Print IP
	printf("< %s , ", inet_ntoa(toPrint));
	//Print HW address

	//Code modified from prhwaddrs.c file
	/*prflag = 0;
	i = 0;
	do {
		if (pair->hw_addr[i] != '\0') {
			prflag = 1;
			break;
		}
	} while (++i < IF_HADDR);

	if (prflag) {
		ptr = pair->hw_addr;
		i = IF_HADDR;
		do {
			printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
		} while (--i > 0);
	}*/

	printHardware(pair->hw_addr);

	printf(" >\n");
}

int exploreInterfaces(){
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	struct hostent *hptr;
	for (hwahead = hwa = get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

		if(strcmp(hwa->if_name, "eth0") == 0 || strcmp(hwa->if_name, "wlan0") == 0 ){
			if ( (sa = hwa->ip_addr) != NULL && hwa->if_haddr != NULL){
				char * ip = Sock_ntop_host(sa, sizeof(*sa));
				//Set test;
				Set * toInsert = allocateSet();
				//assign the ip of the set
				toInsert->ip = inet_addr(ip);

				//assign the hw_Addr of the set
				memcpy((toInsert->hw_addr), hwa->if_haddr, IF_HADDR); 
				
				//Print out to stdout according to specs
				printAddressPair(toInsert);

			}
		}
	}
}

void testCacheEntry(){
	insertCacheEntry(headSet->ip, headSet->hw_addr, 1, 2, 3, &headCache, &tailCache);
	printCacheEntries(headCache);
	CacheEntry * test = findCacheEntry(headSet->ip, headSet->hw_addr,headCache);
	if(test == NULL){
		debug("NULL");
	}
	else{
		debug("FOUND Entry");
		updateCacheEntry(test, 4,5,6);
	}
	printCacheEntries(headCache);

}

int createPFPacket(){
	int sd;
	if((sd = socket(PF_PACKET, SOCK_RAW, htons(OUR_PROTO))) < 0){
		printf("Creating PF_PACKET, SOCK RAW, %d, failed.\n", OUR_PROTO);
		return -1;
	}
	debug("Successfuly created PF_PACKET Socket. Socket Descriptor: %d, Protocol Number: %d", sd, OUR_PROTO);
	return sd;
}

int createUnixSocket(){
	unsigned int s;
	struct sockaddr_un local;
	int len;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if(s == -1){
		printf("Error when creating Unix Domain socket. Errno: %s\n", strerror(errno));
		return -1;		
	}

	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, UNIX_FILE);
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);
	int ret = bind(s, (struct sockaddr *)&local, len);
	if(ret == -1){
		printf("Error when creating Unix Domain socket. Errno: %s\n", strerror(errno));
		return -1;
	}
	debug("Successfully created Unix Socket File. Socket Descriptor: %d\n", s);
	return s;
}


void infiniteSelect(int unix_sd, int pf_sd){
	listen(unix_sd, 5); //Call listen on the socket 
	fd_set rset, rset2;
	int maxfd1;
	struct sockaddr_un remote;
	int t;
	FD_ZERO(&rset);
	int s2;
	//char str[100];
	
	for( ; ; ){
		FD_SET(unix_sd, &rset);
		FD_SET(pf_sd, &rset);
		maxfd1 = max(unix_sd, pf_sd) + 1;
		if((select(maxfd1, &rset, NULL, NULL, NULL)) < 0)
        {
            printf("Error setting up select.\n");
        }
        if(FD_ISSET(unix_sd, &rset)) {
        	FD_CLR(unix_sd, &rset);

            debug("Message on unix domain socket.");
            
            if((s2 = accept(unix_sd, (struct sockaddr*)&remote, &t)) == -1){
            	debug("ACCEPT error");
            	exit(1);
            }
            void * buff = malloc(MAXLINE);
            int n = recv(s2, buff, MAXLINE, 0);
            struct arpdto * msgr = (struct arpdto *)buff;
            struct in_addr ina;
            ina.s_addr = ntohl(msgr->ipaddr);
            debug("%u", msgr->ipaddr);
            printf("Message IP: %s\n IFIndex: %d, HW Type: %u , HW Len: %u\n", inet_ntoa(ina), ntohl(msgr->ifindex), ntohs(msgr->hatype), msgr->halen);
           	
           	CacheEntry * lookup  = findHwByIP(htonl(msgr->ipaddr), headCache);
           	if(lookup == NULL){
           		sendArpRequest(pf_sd);
           		
           		debug("Cache entry not found.");
           		
           		//TODO: Add partial entry to cache
           		insertCacheEntry(msgr->ipaddr, NULL, msgr->ifindex, msgr->hatype, s2, &headCache, &tailCache);
           		debug("Inserted partial cache entry.");
           		printCacheEntries(headCache);
           		//TODO: Setup up select on sd to see if it becomes readable
           		//		if it does then we know that the other end closed the socket
           		FD_ZERO(&rset2);
           		FD_SET(s2, &rset2);
           		FD_SET(pf_sd, &rset2);
           		int maxdf2 = max(s2, pf_sd) + 1;
           		if(select(maxdf2, &rset2, NULL, NULL, NULL) < 0){
           			printf("Error setting up select when Broadcast send.\n");
           			exit(1);
           		}
           		if(FD_ISSET(s2, &rset2)){
           			//Became readable. Means we got a FIN from the other end. 
           			//TODO:	Remove Partial entry from Cache
           			debug("AREQ timed out. Removing partial entry and returning");
           			deletePartialCacheEntry(msgr->ipaddr, &headCache, &tailCache);
           			printCacheEntries(headCache);
           		}
           		if (FD_ISSET(pf_sd, &rset2)) {
           			FD_CLR(pf_sd, &rset2);
           			goto pfRecv; // dont' handle it in inner select to avoid duplicate logic
           		}
           	}
           	else{
           		//TODO: return the hardware address to areq
           		debug("Cache entry found.");
           		if(send(s2, lookup->if_haddr, IF_HADDR, 0) < 0){
           			perror("Sending the found Cache Entry");
           		}
           	}
            
            //sleep(7);
            close(s2);    
            free(buff);                              
        }

        if(FD_ISSET(pf_sd, &rset)) {    	
    pfRecv:	debug("Message on pf_packet socket");
    		FD_CLR(pf_sd, &rset);

    		handleIncomingArpMsg(pf_sd);
        }
	}
}
//No command line arguments are passed into this executable
//runs on every VM [0-10]
int main (){
	int ret;
	int pf_fd, unix_fd;
	//Explore interfaces and build a set of eth0 pairs found
	exploreInterfaces();

	// Create two sockets one PF_PACKET and one Unix Domain socket 
	// PF_PACKET of type SOCK_RAW
	// Unix domain of type sock stream listening socket bound to a well 
	// sunpath file 
	pf_fd = createPFPacket();
	if(pf_fd == -1){
		freeSet();
		return -1;
	}
	unix_fd = createUnixSocket();
	if(unix_fd == -1){
		freeSet();
		return -1;
	}
	

	testCacheEntry();

	//TODO: Select on unix_fd and pf_fd 
	infiniteSelect(unix_fd, pf_fd);


	//TODO: remove the funciton call below it is for testing purposes only
	testCacheEntry();

	freeSet();
	debug("Sets freed from interface exploration.");
	return 0;
}

void sendArpRequest(int pfSocket) {
	char *interface = "eth0\0";
	int sd, status;
	struct ifreq ifr;
	struct sockaddr_ll device;
	uint8_t *src_mac, *dst_mac, *ether_frame;
	char *src_ip = malloc(INET_ADDRSTRLEN), 
		*dst_ip = malloc(INET_ADDRSTRLEN);
	strcpy (src_ip, "130.245.156.22\0");
	strcpy (dst_ip, "130.245.156.21\0");
	struct in_addr srcAddr, dstAddr;	

	if ((status = inet_pton (AF_INET, src_ip, &srcAddr)) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}

	if ((status = inet_pton (AF_INET, dst_ip, &dstAddr)) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}

	// Submit request for a socket descriptor to look up interface.
	if ((sd = socket(PF_PACKET, SOCK_RAW, ARP_PROTO)) < 0) {
		perror ("socket() failed to get socket descriptor for using ioctl() ");
		return;
	}

	// Use ioctl() to look up interface name and get its MAC address.
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	if (ioctl (sd, SIOCGIFHWADDR, &ifr) < 0) {
		perror ("ioctl() failed to get source MAC address ");
		return;
	}
	close (sd);

	// Copy source MAC address.
	memcpy (src_mac, ifr.ifr_hwaddr.sa_data, 6 * sizeof (uint8_t));

	// Find interface index from interface name and store index in
	// struct sockaddr_ll device, which will be used as an argument of sendto().
	memset (&device, 0, sizeof (device));
	if ((device.sll_ifindex = if_nametoindex (interface)) == 0) {
		perror ("if_nametoindex() failed to obtain interface index ");
		exit (EXIT_FAILURE);
	}
	// Fill out sockaddr_ll.
	device.sll_family = AF_PACKET;
	memcpy (device.sll_addr, src_mac, 6 * sizeof (uint8_t));
	device.sll_halen = htons (6);

	//Building Ethernet frame
	ether_frame = malloc(IP_MAXPACKET);
	memcpy (ether_frame, dst_mac, IF_HADDR);
	memcpy (ether_frame + IF_HADDR, src_mac, IF_HADDR);
	struct ethhdr *eh = (struct ethhdr *)ether_frame;/*another pointer to ethernet header*/	 
	eh->h_proto = htons(ARP_PROTO);
	
	// ARP header
	struct myarphdr ahdr;
	bzero(&ahdr, sizeof(ahdr));
	ahdr.ar_hrd = htons(1);
	ahdr.ar_pro = htons(0x0800);
	ahdr.ar_hln = IF_HADDR;
	ahdr.ar_pln = IP_ADDR_LEN;
	ahdr.ar_op = htons(1);
	ahdr.ar_id = htons(ARP_ID);
	memcpy(ahdr.ar_sha, src_mac, ETH_ALEN);
	memcpy(ahdr.ar_sip, &srcAddr.s_addr, IP_ADDR_LEN);
	memcpy(ahdr.ar_tip, &dstAddr.s_addr, IP_ADDR_LEN);

	//lookup after ar_tha
	if ((sd = socket(PF_PACKET, SOCK_RAW, ARP_PROTO)) < 0) {
		perror ("socket() failed ");
		exit (EXIT_FAILURE);
	}
	memcpy (ether_frame + ETH_HDRLEN, &ahdr, ARP_HDRLEN);
	int bytesSent;
	printf("Gonna send ARP\n");
	if ((bytesSent = sendto(pfSocket, ether_frame, ETH_HDRLEN + ARP_HDRLEN, 0, (struct sockaddr *) &device, sizeof (device))) <= 0) {
		perror ("sendto() failed");
		return;
	}

	free(ether_frame);
	free(src_ip);
	free(dst_ip);
	printf("Sent %d bytes in ARP request\n", bytesSent);
}

int handleIncomingArpMsg(int pfSocket, in_addr_t myIpAddr, void* buf) {
	int n;
	struct sockaddr_ll senderAddr;
	int sz = sizeof(senderAddr);
	if ((n = recvfrom(pfSocket, buf, MAXLINE, 0, (SA *)&senderAddr, &sz)) < 0) {
		printFailed();
		return 0;
	}
	printf("We received from PF_PACKET possibly ARP packet, length=%d\n", n);

	// Extract encapsulated ARP packet
	struct myarphdr arph;
	memcpy(&arph, buf + ETH_HDRLEN, ARP_HDRLEN);
	if (ntohs(arph.ar_id) != ARP_ID) {
		printf("Id of ARP is not ours: %d\n", ntohs(arph.ar_id));
		return 0;
	}

	in_addr_t destIp;
	memcpy(&destIp, arph.ar_tip, IP_ADDR_LEN);
	CacheEntry *lookup = findHwByIP(destIp, headCache);
	
	// REPLY
	if (ntohs(arph.ar_op) == 0) { 
		if (lookup == NULL) {
			return 0;
		}
		//TODO: create needed function

		if(send(lookup->unix_fd, lookup->if_haddr, IF_HADDR, 0) < 0){
   			perror("Error when responding to unix domain socket\n");
   		}
   		close(lookup->unix_fd);
   		lookup->unix_fd = -1;
	} 
	// REQUEST
	else if (ntohs(arph.ar_op) == 1) {		
		if (lookup != NULL) {
			updateCacheEntry(lookup, senderAddr.sll_ifindex, arph.ar_hrd, -1);
		}
		else if (destIp == myIpAddr) {
			insertCacheEntry(destIp, arph.ar_tha, senderAddr.sll_ifindex, arph.ar_hrd, -1, &headCache, &tailCache);
		}
	}

	return 1;
}

void printEthPacketWithArp(void* buf) {
	printf("*** Ethernet packet contents ***\n");
	unsigned char *ptr = buf;
	printf("Destination MAC: ");
	printHardware(ptr); // dest
	ptr += IF_HADDR;
	printf("Source MAC: ");
	printHardware(ptr);
	ptr += IF_HADDR;
	struct ethhdr *eh = (struct ethhdr *)buf;
	printf("Frame type: %d\n", (int)ntohs(eh->h_proto));

	printf("*** ARP packet contents ***\n");
	ptr = buf + ETH_HDRLEN;
	struct myarphdr *ah = (struct myarphdr *)ptr;
	printf("Id:%d, HW type:%d, Protocol:%d, HW size:%d, OP:%d", (int)ntohs(ah->ar_id), (int)ntohs(ah->ar_hrd), 
		(int)ntohs(ah->ar_pro), (int)ntohs(ah->ar_hln), (int)ntohs(ah->ar_op));
	
	printf("Sender HW address:");
	printHardware(ptr + IF_HADDR);
	
	in_addr_t *pIpAddr = (in_addr_t *)ah->ar_sip;
	printf("Sender IP address: %s\n", printIPHuman(ntohs(*pIpAddr)));
	
	printf("Target HW address:");
	printHardware(ptr + 2*IF_HADDR + IP_ADDR_LEN);

	pIpAddr = (in_addr_t *)ah->ar_tip;
	printf("Target IP address: %s\n", printIPHuman(ntohs(*pIpAddr)));
}