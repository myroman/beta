#include "unp.h"
#include "misc.h"

void rmnl(char* s) {
	int ln = strlen(s) - 1;
	if (s[ln] == '\n')
	    s[ln] = '\0';
	printf("%c\n",s[ln]);	
}


void printOK() {
	printf("OK\n");
}
void printFailed() {
	printf("FAILED: %s\n", strerror(errno));
}

//it comes in host order
char * printIPHuman(in_addr_t ip){
	struct in_addr ipIa;
	ipIa.s_addr = htonl(ip);
	//ipIa.s_addr = ip;
	return inet_ntoa(ipIa);
}