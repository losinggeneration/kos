/* KallistiOS ##version##

   kernel/net/net_udp.h
   Copyright (C) 2005 Lawrence Sebald

*/

#ifndef __LOCAL_NET_UDP_H
#define __LOCAL_NET_UDP_H

#define packed __attribute__((packed))
typedef struct {
	uint16 src_port    packed;
	uint16 dst_port    packed;
	uint16 length      packed;
	uint16 checksum    packed;
} udp_hdr_t;
#undef packed

int net_udp_input(netif_t *src, eth_hdr_t *eh, ip_hdr_t *ih, const uint8 *data, int size);

#endif /* __LOCAL_NET_UDP_H */
