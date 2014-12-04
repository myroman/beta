#include "unp.h"
#include "misc.h"
#include "debug.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>



int areq (struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr){
	//TODO: Create the unix domain socket of type sock stream and connect to ARP 
	//		Well known file name. Send the IP address and the other three parameters
	//		to ARP
	int sd, t, len;
    struct sockaddr_un remote;
    char str[100];
    //Setup ARP Unix domain socket
    if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }


    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, UNIX_FILE);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
   	//Connect to ARP Unix socket 
    if (connect(sd, (struct sockaddr *)&remote, len) == -1) {
        
        perror("connect");
        exit(1);
    }


    //Construct Buffer for messages to unix socket file 
	int bufSize = sizeof(struct arpdto);
	uint8_t *buffer = (uint8_t*)malloc(bufSize);
	struct sockaddr_in * s_in = (struct sockaddr_in *) IPaddr;
	struct arpdto dto;
	bzero(&dto, bufSize);
	dto.ipaddr = htonl(s_in->sin_addr.s_addr);
	memcpy(buffer, &dto, bufSize);
	

	printf("AREQ: Sent message to ARP to get Hardware address for IP : %s\n", inet_ntoa(s_in->sin_addr));
	//Send to Unix socke file
	int bytesSent;
	if( (bytesSent = send(sd, buffer, bufSize, 0)) == -1){
		printf("Error sending message to ARP Unix Domain Socket.\n");
		free(buffer);
		return;		
	}

	//TODO: after sending we should block on receive. Wait for a timeout on that socket
	fd_set rset;
	int maxfd1;

	//Setup select wait for 5 seconds for response from API
	FD_ZERO(&rset);
	struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0; 
	FD_ZERO(&rset);
	FD_SET(sd, &rset);
	maxfd1 = sd + 1;
	if((select(maxfd1, &rset, NULL, NULL, &tv)) < 0){
		printf("Error setting up select that waits for API response");
	}
	if(FD_ISSET(sd, &rset)){
		debug("API responded");
		//TODO:	recv the contents and display them
		int r;
		char h[MAXLINE];
		if((r= recv(sd, h, MAXLINE, 0 ))  > 0){
			
			printHardware(h);
			debug("Received");
		}
		else{
			debug("received less than zero.");
		}
	}
	else{
		debug("API didnt respond");
		close(sd);
	}



	//TODO: free buffer;
	free(buffer);
}

int main(){
	struct sockaddr_in s;
	s.sin_addr.s_addr = inet_addr("10.255.5.149");
	struct hwaddr h;

	areq((SA*)&s, sizeof(s), &h);
}
