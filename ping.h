#ifndef _ping_h
#define _ping_h
#include "unp.h"
#define	BUFSIZE		1500


struct paramsToPing{
	struct in_addr srcIp;
	struct sockaddr *destAddr;
	socklen_t sockaddrlen;
};
//void sendPing(struct in_addr srcIp, struct sockaddr *destAddr, socklen_t sockaddrlen);
void sendPing(void * args);
void replyToPing(int socket, SockAddrIn senderAddr, int recvAddrLen);
#endif