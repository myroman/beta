#include "arp.h"
#include "misc.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>

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
		FD_ZERO(&rset);
		FD_SET(unix_sd, &rset);
		FD_SET(pf_sd, &rset);
		maxfd1 = max(unix_sd, pf_sd) + 1;
		if((select(maxfd1, &rset, NULL, NULL, NULL)) < 0)
        {
            printf("Error setting up select.\n");
        }
        if(FD_ISSET(unix_sd, &rset)){
            debug("Message on unix domain socket.");
            
            if((s2 = accept(unix_sd, (struct sockaddr*)&remote, &t)) == -1){
            	debug("ACCEPT error");
            	exit(1);
            }
            void * buff = malloc(MAXLINE);
            int n = recv(s2, buff, MAXLINE, 0);
            debug("here");
            struct arpdto * msgr = (struct arpdto *)buff;
            struct in_addr ina;
            ina.s_addr = ntohl(msgr->ipaddr);
            debug("%u", msgr->ipaddr);
            printf("Message IP: %s\n IFIndex: %d, HW Type: %u , HW Len: %u\n", inet_ntoa(ina), ntohl(msgr->ifindex), ntohs(msgr->hatype), msgr->halen);
           	
           	CacheEntry * lookup  = findHwByIP(htonl(msgr->ipaddr), headCache);
           	if(lookup == NULL){
           		//TODO: Broadcast on all the interfaces
           		debug("Cache entry not found.");
           		
           		//TODO: Add partial entry to cache
           		insertCacheEntry(msgr->ipaddr, NULL, msgr->ifindex, msgr->hatype, s2, &headCache, &tailCache);
           		debug("Inserted partial cache entry.");
           		printCacheEntries(headCache);
           		//TODO: Setup up select on sd to see if it becomes readable
           		//		if it does then we know that the other end closed the socket
           		FD_ZERO(&rset2);
           		FD_SET(s2, &rset2);
           		if(select((s2+1), &rset2, NULL, NULL, NULL) < 0){
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
        if(FD_ISSET(pf_sd, &rset)){
        	debug("Message on pf_packet socket");
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

	//TODO: Create two sockets one PF_PACKET and one Unix Domain socket 
	//		PF_PACKET of type SOCK_RAW
	//		Unix domain of type sock stream listening socket bound to a well 
	// 		sunpath file 
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