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
#include <linux/if_ether.h>
typedef struct Set Set;
struct Set{
	in_addr_t ip;
	//struct in_addr ip; //the in_addr struct houses the in_addr_t 32 bit ip
	char    hw_addr[IF_HADDR];
	Set * next;
};

void sendArpRequest(int pfSocket);

struct myarphdr {
	uint16_t ar_id; // our unique ID
	uint16_t ar_hrd; /* format of hardware address   */
	uint16_t ar_pro; /* format of protocol address   */
	unsigned char ar_hln; /* length of hardware address   */
	unsigned char ar_pln; /* length of protocol address   */
	uint16_t ar_op; /* ARP opcode (command)         */

	unsigned char ar_sha[ETH_ALEN];       /* sender hardware address      */
	unsigned char ar_sip[4];              /* sender IP address            */
	unsigned char ar_tha[ETH_ALEN];       /* target hardware address      */
	unsigned char ar_tip[4];              /* target IP address            */
};
#endif