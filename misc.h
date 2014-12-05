#ifndef __MISC_H
#define __MISC_H

void rmnl(char* s);
void printOK();
void printFailed();
char * printIPHuman(in_addr_t ip);
int getmax(int a, int b);

//#define MY_IP_PROTO 12424
#define MYPROTO 85

#define MY_IP_ID 241
#define ETH_PROTO 0x2457

#define RT_ICMPID 1257
#define PING_ICMPID 1258
#define	IF_HADDR	 6	/* same as IFHWADDRLEN in <net/if.h> */
#define ROMAN_IP_TEST "10.0.2.15\0"
#define IP4_HDRLEN 20         // IPv4 header length

#define ECHO_R_ICMP_DATALEN 56

#define OUR_PROTO 1925
#define UNIX_FILE "ARPUnixFile"


//hwaddr structure
struct hwaddr {
	int             sll_ifindex;	 /* Interface number */
	unsigned short  sll_hatype;	 /* Hardware type */
	unsigned char   sll_halen;		 /* Length of address */
	unsigned char   sll_addr[8];	 /* Physical layer address */
};

struct arpdto {
	in_addr_t ipaddr;
	int ifindex;
	unsigned short hatype;
	unsigned char  halen;
};


void printHardware(char * hw);

typedef struct sockaddr_un SockAddrUn;
typedef struct sockaddr_ll SockAddrLl;
typedef struct sockaddr_in SockAddrIn;
#endif
