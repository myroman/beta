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

typedef struct myarphdr ArpHdr;

void sendArp(int pfSocket, in_addr_t srcIp, in_addr_t destIp, int op, unsigned char dst_mac[IF_HADDR]);
void sendArpPacket(int pfSocket, ArpHdr ahdr, unsigned char dst_mac[IF_HADDR]);
void makeMacBroadcast(unsigned char addr[IF_HADDR]);
int handleIncomingArpMsg(int pfSocket, void* buf);
void fillMyMac(int pfSocket, unsigned char macAddr[IF_HADDR]);
void printEthPacketWithArp(void* buf);

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

#define ARP_REQ 1
#define ARP_REP 2
#endif