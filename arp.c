#include "arp.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#define OUR_PROTO 1925
#define UNIX_FILE "ARPUnixFile"
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
	strcpy(local.sun_path,UNIX_FILE);
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

	//TODO: Select on unix_fd and pf_fd 

	

	//TODO: remove the funciton call below it is for testing purposes only
	testCacheEntry();

	freeSet();
	debug("Sets freed from interface exploration.");
	return 0;
}