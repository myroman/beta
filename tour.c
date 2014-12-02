#include "debug.h"
#include <stdio.h>
#include <stdlib.h>



int createRawSockets(){
	//Create two IP raw sockets
	//one called rt for route traversal

	//TODO: create rt raw socket

	//one called pg for pinging

	//TODO: create pg raw socket


	return 0;
}

int createPfPacket(){
	//Create a PF_SPACKET socket that should be of type SOCK_RAW
	//Protocol value of ETH_P_IP 0x800
	
	//TODO: look at above note

	return 0;
}

int createUDPSocket(){
	//Create a UDP socket fro multicasting 
	//May need two UDP sockets look at page 576
	return 0;
}

int main(int argc, char ** argv){
	

	//


	createRawSockets();
	createPfPacket();
	createUDPSocket();





	return 0;
}