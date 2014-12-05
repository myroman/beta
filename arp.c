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
in_addr_t myIpAddr = 0;

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
				myIpAddr = toInsert->ip = inet_addr(ip);								

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
	if((sd = socket(PF_PACKET, SOCK_RAW, htons(ARP_PROTO))) < 0){
		printf("Creating PF_PACKET, SOCK RAW, %d, failed.\n", ARP_PROTO);
		return -1;
	}
	debug("Successfuly created PF_PACKET Socket. Socket Descriptor: %d, Protocol Number: %d", sd, ARP_PROTO);
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
	int s2 = -1;
	void* buff = malloc(MAXLINE);	
	
	for( ; ; ){
		FD_SET(unix_sd, &rset);
		FD_SET(pf_sd, &rset);
		maxfd1 = max(unix_sd, pf_sd);			

		if((select(maxfd1 + 1, &rset, NULL, NULL, NULL)) < 0) {
            printf("Error setting up select.\n");
        }

        if(FD_ISSET(pf_sd, &rset)) {    	
		    FD_CLR(pf_sd, &rset);

    		handleIncomingArpMsg(pf_sd, buff);
        }
        if(FD_ISSET(unix_sd, &rset)) {
        	FD_CLR(unix_sd, &rset);

            debug("Message on unix domain socket.");
            
            if((s2 = accept(unix_sd, (struct sockaddr*)&remote, &t)) == -1){
            	debug("ACCEPT error");
            	exit(1);
            }
            bzero(buff, MAXLINE);
            int n = recv(s2, buff, MAXLINE, 0);
            struct arpdto * msgr = (struct arpdto *)buff;
            struct in_addr ina;
            ina.s_addr = msgr->ipaddr;
            printf("Message IP: %s \n", inet_ntoa(ina));
           	CacheEntry * lookup  = findHwByIP(msgr->ipaddr, headCache);
           	if(lookup == NULL){
           		sendArp(pf_sd, myIpAddr, msgr->ipaddr, ARP_REQ);           		
           		
           		debug("Cache entry not found.");
           		
           		//TODO: Add partial entry to cache
           		insertCacheEntry(msgr->ipaddr, NULL, msgr->ifindex, msgr->hatype, s2, &headCache, &tailCache);
           		debug("Inserted partial cache entry.");
           		printCacheEntries(headCache);
           		//TODO: Setup up select on sd to see if it becomes readable
           		//		if it does then we know that the other end closed the socket
           		//FD_ZERO(&rset2);
           		//FD_SET(s2, &rset2);
           		//FD_SET(pf_sd, &rset2);
           		//int maxdf2 = max(s2, pf_sd) + 1;
           		//printf("Waiting for replies or client termination\n");
           		/*if(select(maxdf2, &rset2, NULL, NULL, NULL) < 0){
           			printf("Error setting up select when Broadcast send.\n");
           			exit(1);
           		}
           		debug("Received something!");
           		if(FD_ISSET(s2, &rset2)){
           			//Became readable. Means we got a FIN from the other end. 
           			//TODO:	Remove Partial entry from Cache
           			debug("AREQ timed out. Removing partial entry and returning");
           			deletePartialCacheEntry(msgr->ipaddr, &headCache, &tailCache);
           			printCacheEntries(headCache);
           		}*/
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
        }        
	}

	free(buff);  
}
//No command line arguments are passed into this executable
//runs on every VM [0-10]
int main(){
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

void sendArp(int pfSocket, in_addr_t srcIp, in_addr_t destIp, int op) {
	ArpHdr ahdr;
	bzero(&ahdr, sizeof(ahdr));
	ahdr.ar_hrd = htons(1);
	ahdr.ar_pro = htons(0x0800);// map to IP
	ahdr.ar_hln = IF_HADDR;
	ahdr.ar_pln = IP_ADDR_LEN;
	ahdr.ar_op = htons(op);
	ahdr.ar_id = htons(ARP_ID);
	unsigned char src_mac[IF_HADDR];
	fillMyMac(pfSocket, src_mac);
	memcpy(ahdr.ar_sha, src_mac, ETH_ALEN);
	memcpy(ahdr.ar_sip, &srcIp, IP_ADDR_LEN);
	memcpy(ahdr.ar_tip, &destIp, IP_ADDR_LEN);
	
	sendArpPacket(pfSocket, ahdr);
}


void sendArpPacket(int pfSocket, ArpHdr ahdr) {
	char *interface = "eth0\0";
	void *ether_frame;
	unsigned char dst_mac[IF_HADDR];

	makeMacBroadcast(dst_mac);
	
	printHardware(ahdr.ar_sha);
	printf("to\n");
	printHardware(dst_mac);
	
	struct sockaddr_ll device;	
	memset (&device, 0, sizeof (device));
	
	if ((device.sll_ifindex = if_nametoindex (interface)) == 0) {
		perror ("if_nametoindex() failed to obtain interface index ");
		exit (EXIT_FAILURE);
	}
	printf("Device ifindex:%d\n", device.sll_ifindex);
	device.sll_family = htons(AF_PACKET);
	device.sll_halen = htons (IF_HADDR);
	device.sll_hatype = htons(ARPHRD_ETHER);	
	memcpy (device.sll_addr, ahdr.ar_sha, IF_HADDR);//try set dest mac, but in ping.c we set src mac
	
	//Building Ethernet frame
	ether_frame = malloc(ETH_HDRLEN + ARP_HDRLEN);
	memcpy (ether_frame, dst_mac, IF_HADDR);
	memcpy ((void*)(ether_frame + IF_HADDR), ahdr.ar_sha, IF_HADDR);
	struct ethhdr *eh = (struct ethhdr *)ether_frame;/*another pointer to ethernet header*/	 
	eh->h_proto = htons(ARP_PROTO);

	memcpy (ether_frame + ETH_HDRLEN, &ahdr, ARP_HDRLEN);
	
	int bytesSent;
	printf("Gonna send ARP\n");
	if ((bytesSent = sendto(pfSocket, ether_frame, ETH_HDRLEN + ARP_HDRLEN, 0, (struct sockaddr *) &device, sizeof (device))) <= 0) {
		perror ("sendto() failed");
		return;
	}

	free(ether_frame);
	printf("Sent %d bytes in ARP request\n", bytesSent);
}

int handleIncomingArpMsg(int pfSocket, void* buf) {
	int n;
	struct sockaddr_ll senderAddr;
	debug("Received on PF_PACKET");
	int sz = sizeof(senderAddr);
	if ((n = recvfrom(pfSocket, buf, MAXLINE, 0, (SA *)&senderAddr, &sz)) < 0) {
		printFailed();
		return 0;
	}
	
	// Extract encapsulated ARP packet
	ArpHdr arph;
	memcpy(&arph, buf + ETH_HDRLEN, ARP_HDRLEN);
	if (ntohs(arph.ar_id) != ARP_ID) {
		printf("Id of ARP is not ours: %d\n", ntohs(arph.ar_id));
		return 0;
	}
	
	in_addr_t destIp, srcIp;
	memcpy(&destIp, arph.ar_tip, IP_ADDR_LEN);
	memcpy(&srcIp, arph.ar_sip, IP_ADDR_LEN);
	
	printEthPacketWithArp(buf);

	CacheEntry *lookup = findHwByIP(destIp, headCache);
	
	// REPLY
	if (ntohs(arph.ar_op) == ARP_REP) { 
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
	else if (ntohs(arph.ar_op) == ARP_REQ) {				
		if (destIp == myIpAddr) {
			debug("insert&send");
			//insertCacheEntry(destIp, arph.ar_tha, senderAddr.sll_ifindex, arph.ar_hrd, -1, &headCache, &tailCache);		
			// For the destination node there will always be a lookup in the cache.
			updateCacheEntry(lookup, senderAddr.sll_ifindex, arph.ar_hrd, -1);

			sendArp(pfSocket, myIpAddr, srcIp, ARP_REP);
		}
		else {
			if (lookup != NULL) {
				debug("update");
				updateCacheEntry(lookup, senderAddr.sll_ifindex, arph.ar_hrd, -1);
			}
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
	ArpHdr *ah = (ArpHdr *)ptr;
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

void makeMacBroadcast(unsigned char addr[IF_HADDR]) {
	int i = 0;
	for(i = 0;i < IF_HADDR;++i) {
		addr[i] = 0xff;
	}
}


void fillMyMac(int pfSocket, unsigned char macAddr[IF_HADDR]) {
	struct ifreq ifr;	
	// Use ioctl() to look up interface name and get its MAC address.
	memset (&ifr, 0, sizeof (ifr));	
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", "eth0\0");
	if (ioctl (pfSocket, SIOCGIFHWADDR, &ifr) < 0) {
		perror ("ioctl() failed to get source MAC address ");
		return;
	}
	// Copy source MAC address.
	memcpy (macAddr, ifr.ifr_hwaddr.sa_data, IF_HADDR);
}