#ifndef _check_sum_h
#define _check_sum_h
#include <netinet/ip_icmp.h>

uint16_t checksum (uint16_t *, int);
uint16_t icmp4_checksum (struct icmp, uint8_t *, int);
uint16_t in_cksum(uint16_t *addr, int len);
char *allocate_strmem (int);
uint8_t *allocate_ustrmem (int);
int *allocate_intmem (int);

#endif
