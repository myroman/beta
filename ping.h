#ifndef _ping_h
#define _ping_h

#define	BUFSIZE		1500
void sendPing(struct in_addr srcIp, struct sockaddr *destAddr, socklen_t sockaddrlen);
void replyToPing(int socket, SockAddrIn senderAddr, int recvAddrLen);
#endif