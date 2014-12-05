#include "unp.h"
#include "debug.h"


struct tourdata {
	int index;
	int nodes_in_tour;
	in_addr_t mult_ip;
	in_port_t mult_port;
};
int main(){
	debug("Hello World");

	struct tourdata test;
	
	test.index = 0;
	test.nodes_in_tour = 2;
	test.mult_ip = 1;
	test.mult_port = 50000;


	debug("Size of tourData %u", sizeof(struct tourdata));

	void * buffer = malloc(1000);
	memcpy(&test, buffer, sizeof(struct tourdata));
	debug("here");




}