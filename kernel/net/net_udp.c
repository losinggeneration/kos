/* KallistiOS ##version##

   kernel/net/net_udp.c
   Copyright (C) 2005 Lawrence Sebald

*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <kos/net.h>
#include <kos/mutex.h>
#include <sys/queue.h>
#include "net_ipv4.h"
#include "net_udp.h"

typedef struct udp_pkt {
	TAILQ_ENTRY(udp_pkt) pkt_queue;
	udp_hdr_t hdr;
	uint8 *data;
	uint16 datasize;
} udp_pkt_t;

TAILQ_HEAD(udp_pkt_queue, udp_pkt);

typedef struct udp_sock {
	LIST_ENTRY(udp_sock) sock_list;
	int sock_num;
	uint16 loc_port;
	uint16 rem_port;
	uint32 loc_addr;
	uint32 rem_addr;
	struct udp_pkt_queue packets;
} udp_sock_t;

LIST_HEAD(udp_sock_list, udp_sock);

static struct udp_sock_list net_udp_socks = LIST_HEAD_INITIALIZER(0);
static int cur_udp_sock = 0;
static uint8 udp_buf[1514];

int net_udp_sock_open(uint16 loc_port, uint32 loc_addr, uint16 rem_port, uint32 rem_addr)	{
	udp_sock_t *ns;

	LIST_FOREACH(ns, &net_udp_socks, sock_list)	{
		if(ns->loc_port == loc_port && ns->loc_addr == loc_addr && 
           ns->rem_port == rem_port && ns->rem_addr == rem_addr)	{
			dbglog(DBG_KDEBUG, "net_udp: Attempt to create already existing socket\n");
			return 0;
		}
	}

	ns = (udp_sock_t *)malloc(sizeof(udp_sock_t));

	ns->sock_num = ++cur_udp_sock;
	ns->loc_port = loc_port;
	ns->loc_addr = loc_addr;
	ns->rem_port = rem_port;
	ns->rem_addr = rem_addr;

	TAILQ_INIT(&ns->packets);
	LIST_INSERT_HEAD(&net_udp_socks, ns, sock_list);

	return cur_udp_sock;
}

int net_udp_sock_close(int socknum)	{
	udp_sock_t *ns;
	udp_pkt_t *pkt;

	LIST_FOREACH(ns, &net_udp_socks, sock_list)	{
		if(ns->sock_num == socknum)	{
			LIST_REMOVE(ns, sock_list);
			TAILQ_FOREACH(pkt, &ns->packets, pkt_queue) {
				TAILQ_REMOVE(&ns->packets, pkt, pkt_queue);
				free(pkt->data);
				free(pkt);
			}
			free(ns);

			return 0;
		}
	}

	dbglog(DBG_KDEBUG, "net_udp: Attempt to close unopened socket");
	return -1;
}

int net_udp_recv(int sock, uint8 *buf, int size)	{
	udp_sock_t *ns;
	udp_pkt_t *pkt;

	LIST_FOREACH(ns, &net_udp_socks, sock_list)	{
		if(ns->sock_num == sock)	{
			pkt = TAILQ_FIRST(&ns->packets);
			if(pkt)	{
				if(size >= pkt->datasize)	{
					memcpy(buf, pkt->data, pkt->datasize);
					TAILQ_REMOVE(&ns->packets, pkt, pkt_queue);
					size = pkt->datasize;
					free(pkt->data);
					free(pkt);
					return size;
				}
				else	{
					uint8 tbuf[pkt->datasize - size];
					memcpy(buf, pkt->data, size);
					memcpy(tbuf, pkt->data + size, pkt->datasize - size);
					free(pkt->data);
					pkt->data = (uint8 *)malloc(pkt->datasize - size);
					memcpy(pkt->data, tbuf, pkt->datasize - size);
					pkt->datasize -= size;
					return size;
				}
			}
			else	{
				return 0;
			}
		}
	}

	dbglog(DBG_KDEBUG, "net_udp: attempt to recv on unopened socket\n");

	return -1;
}

int net_udp_send(int sock, const uint8 *data, int size)	{
	udp_sock_t *ns;
	
	LIST_FOREACH(ns, &net_udp_socks, sock_list)	{
		if(ns->sock_num == sock)	{
			ip_pseudo_hdr_t *ps = (ip_pseudo_hdr_t *)udp_buf;
			ip_hdr_t ip;
			int internsize;

			/* Fill in the UDP Header */
			ps->src_addr = ns->loc_addr;
			ps->dst_addr = ns->rem_addr;
			ps->zero = 0;
			ps->proto = 17;
			ps->length = net_ntohs(size + sizeof(udp_hdr_t));
			ps->src_port = ns->loc_port;
			ps->dst_port = ns->rem_port;
			ps->hdrlength = ps->length;
			ps->checksum = 0;
			memcpy(ps->data, data, size);

			if(size % 2)	{
				internsize = size + 1;
				ps->data[internsize] = 0;
			}
			else
				internsize = size;

			/* Compute the UDP Checksum */
			ps->checksum = net_ipv4_checksum((uint16 *)udp_buf, (internsize + sizeof(udp_hdr_t) + 12) / 2);

			/* Fill in the IP Header */
			ip.version_ihl = 0x45; /* 20 byte header, ipv4 */
			ip.tos = 0;
			ip.length = net_ntohs(sizeof(udp_hdr_t) + size + 20);
			ip.packet_id = 0;
			ip.flags_frag_offs = net_ntohs(0x4000);
			ip.ttl = 64;
			ip.protocol = 17; /* UDP */
			ip.checksum = 0;
			ip.src = ps->src_addr;
			ip.dest = ps->dst_addr;

			/* Compute the IP Checksum */
			ip.checksum = net_ipv4_checksum((uint16*)&ip, sizeof(ip_hdr_t) / 2);

			net_ipv4_send_packet(net_default_dev, &ip, (uint8 *)(udp_buf + 12), sizeof(udp_hdr_t) + size);

			return size;
		}
	}

	dbglog(DBG_KDEBUG, "net_udp: attempt to send on unopened socket\n");
	
	return -1;	
}

int net_udp_input(netif_t *src, eth_hdr_t *eh, ip_hdr_t *ip, const uint8 *data, int size) {
	ip_pseudo_hdr_t *ps = (ip_pseudo_hdr_t*)udp_buf;
	uint16 i;
	udp_sock_t *sock;
	udp_pkt_t *pkt;

	ps->src_addr = ip->src;
	ps->dst_addr = ip->dest;
	ps->zero = 0;
	ps->proto = ip->protocol;
	memcpy(&ps->src_port, data, size);
	ps->length = net_ntohs(size);

	if(size % 2)	{
		ps->data[size - sizeof(udp_hdr_t)] = 0;
		++size;
	}

	i = ps->checksum;
	ps->checksum = 0;
	ps->checksum = net_ipv4_checksum((uint16 *)udp_buf, (size + 12) / 2);

#ifdef DEBUG_UDP
	printf("UDP Header Dump:\n");
	printf("  Pseudo-Header:\n");
	printf("src_addr  = 0x%08x\n", ps->src_addr);
	printf("dst_addr  = 0x%08x\n", ps->dst_addr);
	printf("zero      = 0x%02x\n", ps->zero);
	printf("proto     = 0x%02x\n", ps->proto);
	printf("length    = 0x%04x\n", ps->length);
	printf("src_port  = 0x%04x\n", ps->src_port);
	printf("dst_port  = 0x%04x\n", ps->dst_port);
	printf("hdrlength = 0x%04x\n", ps->hdrlength);
	printf("checksum  = 0x%04x\n", ps->checksum);
#endif

	if(i != ps->checksum)	{
		dbglog(DBG_KDEBUG, "net_udp: discarding UDP packet with invalid checksum\n");
		dbglog(DBG_KDEBUG, "net_udp:   was %x, expected %x\n", i, ps->checksum);
		return -1;
	}

	LIST_FOREACH(sock, &net_udp_socks, sock_list) {
		if(sock->loc_port == ps->dst_port && sock->loc_addr == ps->dst_addr && 
		   ((sock->rem_port == ps->src_port && sock->rem_addr == ps->src_addr) ||
		   (sock->rem_port == 0 && sock->rem_addr == 0))) {
			sock->rem_port = ps->src_port;
			sock->rem_addr = ps->src_addr;
			pkt = (udp_pkt_t *) malloc(sizeof(udp_pkt_t));
			pkt->datasize = size - sizeof(udp_hdr_t);
			pkt->data = (uint8 *) malloc(pkt->datasize);
			memcpy(&pkt->hdr, &ps->src_port, sizeof(udp_hdr_t));
			memcpy(pkt->data, ps->data, pkt->datasize);
			TAILQ_INSERT_TAIL(&sock->packets, pkt, pkt_queue);
			return 0;			
		}
	}

	dbglog(DBG_KDEBUG, "net_udp: discarding UDP packet for unopened socket\n");

	return 0;
}

int net_udp_send_raw(netif_t *net, uint16 src_port, uint16 dst_port, const uint8 ipa[4],
                     const uint8 *data, int size) {
	ip_pseudo_hdr_t *ps = (ip_pseudo_hdr_t *)udp_buf;
	ip_hdr_t ip;
	int internsize;

	/* Fill in the UDP Header */
	if(net == NULL) /* Sending from the loopback device? */
		ps->src_addr = 0x0100007f; /* 127.0.0.1 */
	else
		ps->src_addr = net_ntohl(net_ipv4_address(net->ip_addr));
	ps->dst_addr = net_ntohl(net_ipv4_address(ipa));
	ps->zero = 0;
	ps->proto = 17;
	ps->length = net_ntohs(size + sizeof(udp_hdr_t));
	ps->src_port = net_ntohs(src_port);
	ps->dst_port = net_ntohs(dst_port);
	ps->hdrlength = ps->length;
	ps->checksum = 0;
	memcpy(ps->data, data, size);

	if(size % 2)	{
		internsize = size + 1;
		ps->data[internsize] = 0;
	}
	else
		internsize = size;
	

	/* Compute the UDP Checksum */
	ps->checksum = net_ipv4_checksum((uint16 *)udp_buf, (internsize + sizeof(udp_hdr_t) + 12) / 2);

	/* Fill in the IP Header */
	ip.version_ihl = 0x45; /* 20 byte header, ipv4 */
	ip.tos = 0;
	ip.length = net_ntohs(sizeof(udp_hdr_t) + size + 20);
	ip.packet_id = 0;
	ip.flags_frag_offs = net_ntohs(0x4000);
	ip.ttl = 64;
	ip.protocol = 17; /* UDP */
	ip.checksum = 0;
	ip.src = ps->src_addr;
	ip.dest = ps->dst_addr;

	/* Compute the IP Checksum */
	ip.checksum = net_ipv4_checksum((uint16*)&ip, sizeof(ip_hdr_t) / 2);

	return net_ipv4_send_packet(net, &ip, (uint8 *)(udp_buf + 12), sizeof(udp_hdr_t) + size);
}
