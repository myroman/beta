#ifndef __MISC_H
#define __MISC_H

void rmnl(char* s);
void printOK();
void printFailed();
char * printIPHuman(in_addr_t ip);

#define MY_IP_PROTO 12424

#define ICMPID 1258

typedef struct sockaddr_in SockAddrIn;
#endif