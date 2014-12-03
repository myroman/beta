#include "misc.h"

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