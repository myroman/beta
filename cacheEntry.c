#include "cacheEntry.h"
#include "misc.h"


int insertCacheEntry(in_addr_t ip, char *hw, int ifindex, unsigned int hatype, int unix_fd, CacheEntry **headCache, CacheEntry **tailCache){
	int ret = 0;

	CacheEntry * newEntry = (CacheEntry * )malloc(sizeof(struct CacheEntry));

	if(newEntry == NULL){
		ret = -1;
		return ret;
	}

	newEntry->ip = ip;
	newEntry->sll_ifindex = ifindex;
	newEntry->sll_hatype = hatype;
	newEntry->unix_fd = unix_fd;
	memcpy((newEntry->if_haddr), hw, IF_HADDR); 

	if(*headCache == NULL && *tailCache == NULL){
		*headCache = newEntry;
		*tailCache = newEntry;
		newEntry->left = NULL;
		newEntry->right = NULL;
	}
	else {
		(*tailCache)->right = newEntry;
		newEntry->left = *tailCache;
		*tailCache = newEntry;
	}
	ret = 1;
	return ret;
}
CacheEntry * findCacheEntry(in_addr_t ip, char* hw, CacheEntry *headCache){
	CacheEntry * ptr = headCache;
	while(ptr != NULL){
		if(ptr->ip == ip){
			//debug("IP match");
			int val = memcmp(hw, ptr->if_haddr, IF_HADDR);
			if( val == 0){
				//debug("Str match");
				return ptr;
			}
		}
		ptr = ptr->right;
	}
	return NULL;
}

void printCacheEntries(CacheEntry *headCache){
	CacheEntry *ptr = headCache;
	printf("*** Cache Entries ***\n");
	while(ptr != NULL){
		struct in_addr toPrint;
		toPrint.s_addr = ptr->ip;
		printf("IP: %s , ", inet_ntoa(toPrint));
		printf("HW: ");
		printHardware(ptr->if_haddr);
		printf(", IF Ind: %d, HW Type: %u, Unix FD: %d\n", ptr->sll_ifindex, ptr->sll_hatype, ptr->unix_fd );

		ptr = ptr->right;
	}
	printf("*********************\n");
}

void updateCacheEntry(CacheEntry * entry, int ifind, unsigned short hatype, int ufd){
	entry->sll_ifindex = ifind;
	entry->sll_hatype = hatype;
	entry->unix_fd = ufd;
}