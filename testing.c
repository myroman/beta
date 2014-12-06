#include "unp.h"
#include "debug.h"
#include "misc.h"

int main(){
	debug("Hello World");

	struct tourdata test;
	
	test.index = 0;
	test.nodes_in_tour = 2;
	test.mult_ip = 1;
	test.mult_port = 50000;

	debug("port_t size: %u", sizeof(in_addr_t));


	debug("Size of tourData %u", sizeof(struct tourdata));
	debug("Size of ip %u", sizeof(in_addr_t));
	void * buffer = malloc(1000);
	bzero(buffer, 1000);
	memcpy(buffer, &test, sizeof(struct tourdata));
	void * ptr = buffer;
	ptr = ptr + sizeof(struct tourdata);
	int i;
	for(i = 0; i < 11; i++){
		in_addr_t temp = i;
		memcpy(ptr, &temp, 4);
		in_addr_t* t = (in_addr_t *) ptr;
		printf("%u: IP %u\n", temp , *t);
		ptr = ptr + 4;//(i*4);
	}

	debug("here");

	struct tourdata * unpack = (struct tourdata *)buffer;


	printf("index: %u, nodes_in_tour: %u mult_ip: %u mult_port: %u\n", unpack->index, unpack->nodes_in_tour, unpack->mult_ip, unpack->mult_port);
	unpack->index++;
	memcpy(buffer, unpack, sizeof(struct tourdata));
	unpack = (struct tourdata *)buffer;
	printf("index: %u, nodes_in_tour: %u mult_ip: %u mult_port: %u\n", unpack->index, unpack->nodes_in_tour, unpack->mult_ip, unpack->mult_port);
	void * ptr2 = buffer + sizeof(struct tourdata);
	for(i = 0; i < 11; i++){
		
		in_addr_t * temp2 = (in_addr_t *) ptr2;
		printf("%d: IP : %u\n", i, *temp2);
		//memcpy(ptr, &temp, sizeof(in_addr_t));
		ptr2 = ptr2 + 4;//(i*4);
	}

	free(buffer);

}