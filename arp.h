#ifndef __ARP_H
#define __ARP_H
#include "unp.h"
#include "misc.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "hw_addrs.h"
#include "cacheEntry.h"

typedef struct Set Set;
struct Set{
	in_addr_t ip;
	//struct in_addr ip; //the in_addr struct houses the in_addr_t 32 bit ip
	char    hw_addr[IF_HADDR];
	Set * next;
};
#endif