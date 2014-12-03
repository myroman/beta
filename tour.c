#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include "unp.h"
#include "debug.h"
#include "hw_addrs.h"

struct in_addr * ip_list;
char* myNodeIP;
char* myNodeName;

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

void getNodeName(){
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	struct hostent *hptr;
	for (hwahead = hwa = get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

		if(strcmp(hwa->if_name, "eth0") == 0 || strcmp(hwa->if_name, "wlan0") == 0 ){
			if ( (sa = hwa->ip_addr) != NULL)
				myNodeIP = Sock_ntop_host(sa, sizeof(*sa));
		}
	}
	myNodeIP = Sock_ntop_host(sa, sizeof(*sa));

	//myNodeName = findNameofVM(myNodeIP);
	if(inet_aton(myNodeIP, &ip_list[0]) != 0){
            //if((hptr = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET)) == NULL)
			if((hptr = gethostbyaddr(&ip_list[0], sizeof(struct in_addr), AF_INET)) == NULL){
                    err_quit ( "gethostbyaddress error for IP address: %s: %s" , myNodeIP, hstrerror(h_errno));
            		debug("errno: %s", strerror(errno));
            }
            printf("The server host name is %s.\n", hptr->h_name);
            myNodeName =(char*) malloc(strlen(hptr->h_name));
            strcpy(myNodeName,hptr->h_name);
            //strcpy(myNodeName, myNodeIP);//copy the string into string
    }
    debug("%s %s", myNodeName, myNodeIP);
    debug("%d: %s", 0, inet_ntoa((ip_list[0])));
}

void fillIpList(int argc, char **argv){
	int i, VMcount;
	VMcount = argc - 1;
	info("Number of VMs passed: %d", (VMcount));
	for(i = 1; i < argc; i++){
		struct hostent *hptr;
		hptr = gethostbyname(argv[i]);
		char str[INET_ADDRSTRLEN];
		inet_ntop(hptr->h_addrtype, hptr-> h_addr_list[0],str, sizeof(str));
		debug("%s", str);
		ip_list[i].s_addr =  inet_addr(str);
		info("%d: %s", i, argv[i]);
	}
	for(i = 1; i < argc; i++){
		debug("%d: %s", i, inet_ntoa(*(ip_list+i)));
	}
}

int main(int argc, char ** argv){
	
	int i, VMcount;
	VMcount = argc - 1;
	//0th index will house the IP of the current node that it is running on
	ip_list= (struct in_addr *) malloc(sizeof(struct in_addr) * (VMcount + 1));
	
	getNodeName();
	fillIpList(argc, argv);
	
	

	createRawSockets();
	createPfPacket();
	createUDPSocket();





	return 0;
}