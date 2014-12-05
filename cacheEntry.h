#ifndef __CACHE_ENTRY_H
#define __CACHE_ENTRY_H
#include "unp.h"
#include "misc.h"
#include "debug.h"

typedef struct CacheEntry CacheEntry;
struct CacheEntry{ //Key is ip and if_haddr together
	in_addr_t 		ip;
	char      		if_haddr[IF_HADDR];
	int 	  		sll_ifindex;
	unsigned short	sll_hatype;
	int 			unix_fd;
	CacheEntry *left;
	CacheEntry *right;
};

int insertCacheEntry(in_addr_t ip, char *hw, int ifindex, unsigned int hatype, int unix_fd, CacheEntry **headCache, CacheEntry **tailCache);
void deletePartialCacheEntry(in_addr_t ip, CacheEntry **headCache, CacheEntry **tailCache);
CacheEntry * findCacheEntry(in_addr_t ip, char* hw, CacheEntry *headCache);
CacheEntry * findHwByIP(in_addr_t, CacheEntry * headCache);
void printCacheEntries(CacheEntry *headCache);
void updateCacheEntry(CacheEntry * entry, int ifind, unsigned short hatype, int ufd);
void updatePartialCacheEntry(CacheEntry *entry, char *hw, int ifind, unsigned short hatype);

#endif