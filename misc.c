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

void printHardware(char * hw){
	int    i, prflag;
	char   *ptr;
	prflag = 0;
	i = 0;
	do {
		if (hw != '\0') {
			prflag = 1;
			break;
		}
	} while (++i < IF_HADDR);

	if (prflag) {
		ptr = hw;
		i = IF_HADDR;
		do {
			printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
		} while (--i > 0);
	}
}

int getmax(int a, int b) {
	if (a >= b)
		return a;
	return b;	
}