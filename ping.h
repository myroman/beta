#ifndef _ping_h
#define _ping_h

#define	BUFSIZE		1500
void replyToPing(int socket, SockAddrIn senderAddr, int recvAddrLen);

#endif