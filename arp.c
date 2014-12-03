
#include "arp.h"

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
}

//No command line arguments are passed into this executable
//runs on every VM [0-10]
int main (){
	
	//Explore interfaces and build a set of eth0 pairs found
	exploreInterfaces();

	//TODO: Create two sockets one PF_PACKET and one Unix Domain socket 
	//		PF_PACKET of type SOCK_RAW
	//		Unix domain of type sock stream listening socket bound to a well 
	// 		sunpath file 

	testCacheEntry();
	freeSet();
	debug("Sets freed from interface exploration.");
	return 0;
}