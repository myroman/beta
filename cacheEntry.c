#include "cacheEntry.h"
#include "misc.h"

int insertCacheEntry(in_addr_t ip, char *hw, int ifindex, unsigned int hatype, int unix_fd, CacheEntry **headCache, CacheEntry **tailCache){
	int ret = 0;
	CacheEntry * newEntry = (CacheEntry * )malloc(sizeof(struct CacheEntry));

	if(newEntry == NULL){
		ret = -1;
		return ret;
	}
	newEntry->ip = htonl(ip);
	newEntry->sll_ifindex = ifindex;
	newEntry->sll_hatype = hatype;
	newEntry->unix_fd = unix_fd;
	if(hw != NULL){
		memcpy((newEntry->if_haddr), hw, IF_HADDR);
	}
	else{
		newEntry->if_haddr[0] = 0;
		newEntry->if_haddr[1] = 0;
		newEntry->if_haddr[2] = 0;
		newEntry->if_haddr[3] = 0;
		newEntry->if_haddr[4] = 0;
		newEntry->if_haddr[5] = 0;
	} 
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

void deletePartialCacheEntry(in_addr_t ip, CacheEntry ** headCache, CacheEntry ** tailCache){
	
	ip = htonl(ip);
	CacheEntry *ptr = *headCache;
	while(ptr != NULL){
		if(ptr->ip == ip){ //May need to make sure the MAC address field is not filled in here as well
			int i = 0;

			for(i =0; i < IF_HADDR ; i++){
				if(ptr->if_haddr[i] != 0){
					//debug("NOT PARTIAL ENTRY");
					return;
				}
			}

			//HEAD
			if(ptr->left == NULL){
				//debug("head Delete");
				if(ptr->right == NULL){
					*headCache = NULL;
					*tailCache = NULL;
				}
				else{
					//ptr->left = NULL;
					*headCache = ptr->right;
					(*headCache)->left = NULL;
					
				}
			}	
			//TAIL
			if(ptr->right == NULL){
				//debug("tail Delete");
				if(ptr->left == NULL){
					*headCache = NULL;
					*tailCache = NULL;
				}
				else{
					ptr->left->right = NULL;
					*tailCache = ptr->left;
				}
			}
			//MIDDLE
			if(ptr->right != NULL && ptr->left != NULL){
				//debug("Middle delete");
				ptr->left->right = ptr->right;
				ptr->right->left = ptr->left;
			}
			free(ptr);
			return;
		}
		ptr = ptr->right;
	}
	return;
}
CacheEntry * findCacheEntry(in_addr_t ip, char* hw, CacheEntry *headCache){
	ip = htonl(ip);
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


CacheEntry * findHwByIP(in_addr_t ip, CacheEntry * headCache){
	ip = htonl(ip);
	CacheEntry *ptr = headCache;
	while(ptr != NULL){
		if(ptr->ip == ip){
			return ptr;
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
		toPrint.s_addr = ntohl(ptr->ip);
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