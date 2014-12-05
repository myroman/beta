#include "unp.h"
#include "misc.h"
#include "debug.h"
#include "ping.h"
#include "checksum.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO
#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <netinet/in.h>       // IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/udp.h>      // struct udphdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.

#include <errno.h>            // errno, perror()

#define IP4_HDRLEN 20  // IPv4 header length
#define ICMP_HDRLEN 8  // ICMP header length for echo request, excludes data

void sendPing(struct sockaddr *destIp, socklen_t sockaddrlen)
{
	int i, status, datalen, frame_length, sd, bytes, *ip_flags;
	char *interface, *target, *src_ip, *dst_ip;
	struct ip iphdr;
	struct icmp send_icmphdr;
	uint8_t *src_mac, *dst_mac, *ether_frame;
	struct addrinfo hints, *res;
	struct sockaddr_in *ipv4;
	struct sockaddr_ll device;
	struct ifreq ifr;
	void *tmp;

	// Allocate memory for various arrays.
	src_mac = allocate_ustrmem (6);
	dst_mac = allocate_ustrmem (6);
	ether_frame = allocate_ustrmem (IP_MAXPACKET);
	interface = allocate_strmem (40);
	target = allocate_strmem (40);
	src_ip = allocate_strmem (INET_ADDRSTRLEN);
	dst_ip = allocate_strmem (INET_ADDRSTRLEN);
	ip_flags = allocate_intmem (4);

	// Interface to send packet through.
	strcpy (interface, "eth0");

	// Submit request for a socket descriptor to look up interface.
	if ((sd = socket(PF_PACKET, SOCK_RAW, htons (ETH_P_IP))) < 0) {
		perror ("socket() failed to get socket descriptor for using ioctl() ");
		exit (EXIT_FAILURE);
	}

	// Use ioctl() to look up interface name and get its MAC address.
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	if (ioctl (sd, SIOCGIFHWADDR, &ifr) < 0) {
		perror ("ioctl() failed to get source MAC address ");
		return;
	}
	close (sd);

	// Copy source MAC address.
	memcpy (src_mac, ifr.ifr_hwaddr.sa_data, 6 * sizeof (uint8_t));

	// Find interface index from interface name and store index in
	// struct sockaddr_ll device, which will be used as an argument of sendto().
	memset (&device, 0, sizeof (device));
	if ((device.sll_ifindex = if_nametoindex (interface)) == 0) {
		perror ("if_nametoindex() failed to obtain interface index ");
		exit (EXIT_FAILURE);
	}

	struct hwaddr destHwAddr;
	int res2 = areq(destIp, sockaddrlen, &destHwAddr);
	if (res2 == -1) {
		printf("ARP haven't responded. Return\n");
		return;
	}
	if (res2 == 0) {
		return; // already printed
	}
	
	memcpy(dst_mac, &destHwAddr.sll_addr, 6);
	printf("ARP gave us MAC:\n");
	printHardware(dst_mac);
	/*dst_mac[0] = 0x00;
	dst_mac[1] = 0x0c;
	dst_mac[2] = 0x29;
	dst_mac[3] = 0x49;
	dst_mac[4] = 0x3f;
	dst_mac[5] = 0x5b;
	*/
	strcpy (src_ip, "130.245.156.22");
	strcpy (target, "130.245.156.21");

	// Fill out hints for getaddrinfo().
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = hints.ai_flags | AI_CANONNAME;

	// Resolve target using getaddrinfo().
	if ((status = getaddrinfo (target, NULL, &hints, &res)) != 0) {
		fprintf (stderr, "getaddrinfo() failed: %s\n", gai_strerror (status));
		exit (EXIT_FAILURE);
	}
	ipv4 = (struct sockaddr_in *) res->ai_addr;
	tmp = &(ipv4->sin_addr);
	if (inet_ntop (AF_INET, tmp, dst_ip, INET_ADDRSTRLEN) == NULL) {
		status = errno;
		fprintf (stderr, "inet_ntop() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}
	freeaddrinfo (res);

	// Fill out sockaddr_ll.
	device.sll_family = AF_PACKET;
	device.sll_halen = htons (6);
	memcpy (device.sll_addr, src_mac, 6);
	
	// IPv4 header
	datalen = ECHO_R_ICMP_DATALEN;
	// IPv4 header length (4 bits): Number of 32-bit words in header = 5
	iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);	
	iphdr.ip_v = 4;// Internet Protocol version (4 bits): IPv4	
	iphdr.ip_tos = 0;// Type of service (8 bits)	
	iphdr.ip_len = htons (IP4_HDRLEN + ICMP_HDRLEN + datalen);// Total length of datagram (16 bits): IP header + UDP header + datalen	
	iphdr.ip_id = htons (MY_IP_ID);// ID sequence number (16 bits): unused, since single datagram

	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram
	ip_flags[0] = 0;  
	ip_flags[1] = 0;// Do not fragment flag (1 bit)  
	ip_flags[2] = 0;// More fragments following flag (1 bit)  
	ip_flags[3] = 0;// Fragmentation offset (13 bits)

	iphdr.ip_off = htons ((ip_flags[0] << 15)
	                  + (ip_flags[1] << 14)
	                  + (ip_flags[2] << 13)
	                  +  ip_flags[3]);

	// Time-to-Live (8 bits): default to maximum value
	iphdr.ip_ttl = 255;  
	iphdr.ip_p = IPPROTO_ICMP;// Transport layer protocol (8 bits)

	// Source IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}

	// Destination IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}
	
	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
	iphdr.ip_sum = 0;
	iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);

	// ICMP header	
	send_icmphdr.icmp_type = ICMP_ECHO;// Message Type (8 bits): echo request	
	send_icmphdr.icmp_code = 0;	// Message Code (8 bits): echo request
	send_icmphdr.icmp_id = htons (PING_ICMPID);// Identifier (16 bits): usually pid of sending process - pick a number
	send_icmphdr.icmp_seq = htons (0);		
	//attach a timestamp
	Gettimeofday((struct timeval *) &send_icmphdr.icmp_data, NULL);
	
	send_icmphdr.icmp_cksum = 0;
	int icmplen = 8 + datalen;
	send_icmphdr.icmp_cksum = in_cksum((u_short*)&send_icmphdr, icmplen);

	// Fill out ethernet frame header.
	// Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + ICMP header + ICMP data)
	frame_length = 6 + 6 + 2 + IP4_HDRLEN + ICMP_HDRLEN + datalen;

	// Destination and Source MAC addresses
	memcpy (ether_frame, dst_mac, 6 * sizeof (uint8_t));
	memcpy (ether_frame + 6, src_mac, 6 * sizeof (uint8_t));

	// Next is ethernet type code (ETH_P_IP for IPv4).
	struct ethhdr *eh = (struct ethhdr *)ether_frame;/*another pointer to ethernet header*/	 
	eh->h_proto = htons(ETH_P_IP);//htons();
	// IPv4 header and IMCP
	memcpy (ether_frame + ETH_HDRLEN, &iphdr, IP4_HDRLEN * sizeof (uint8_t));
	memcpy (ether_frame + ETH_HDRLEN + IP4_HDRLEN, &send_icmphdr, ICMP_HDRLEN);	
	memcpy (ether_frame + ETH_HDRLEN + IP4_HDRLEN + ICMP_HDRLEN, &send_icmphdr.icmp_data, datalen * sizeof (uint8_t));

	// Submit request for a raw socket descriptor.
	if ((sd = socket(PF_PACKET, SOCK_RAW, htons (ETH_P_IP))) < 0) {
		perror ("socket() failed ");
		exit (EXIT_FAILURE);
	}
	char* host = "vm1\0";
    struct addrinfo* ai = Host_serv(host, NULL, 0, 0);
	char* h = sock_ntop_host(ai->ai_addr, ai->ai_addrlen);
	printf("PING %s (%s): %d data bytes\n", ai->ai_canonname ? ai->ai_canonname : h, h, datalen);

	// Send ethernet frame to socket.
	if ((bytes = sendto (sd, ether_frame, frame_length, 0, (struct sockaddr *) &device, sizeof (device))) <= 0) {
		perror ("sendto() failed");
		exit (EXIT_FAILURE);
	}

	// Close socket descriptor.
	close (sd);

	// Free allocated memory.
	free (src_mac);
	free (dst_mac);
	free (ether_frame);
	free (interface);
	free (target);
	free (src_ip);
	free (dst_ip);
	free (ip_flags);

	return;
}