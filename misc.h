#ifndef __MISC_H
#define __MISC_H

void rmnl(char* s);
void printOK();
void printFailed();
char * printIPHuman(in_addr_t ip);

#define MY_IP_PROTO 12424

#define ICMPID 1258
#define	IF_HADDR	 6	/* same as IFHWADDRLEN in <net/if.h> */
#define ROMAN_IP_TEST "10.0.2.15\0"

void printHardware(char * hw);
typedef struct sockaddr_un SockAddrUn;
typedef struct sockaddr_ll SockAddrLl;
typedef struct sockaddr_in SockAddrIn;
#endif
