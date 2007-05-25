/* KallistiOS ##version##

   kernel/net/net_ipv4.c

   Copyright (C) 2005, 2006 Lawrence Sebald

   Portions adapted from KOS' old net_icmp.c file:
   Copyright (c) 2002 Dan Potter

*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <kos/net.h>
#include "net_ipv4.h"
#include "net_icmp.h"
#include "net_udp.h"

/* Perform an IP-style checksum on a block of data */
uint16 net_ipv4_checksum(const uint16 *data, int words) {
	uint32 sum;
	int i;

	sum = 0;
	for (i=0; i<words; i++) {
		sum += data[i];
		if (sum & 0xffff0000) {
			sum &= 0xffff;
			sum++;
		}
	}

	return ~(sum & 0xffff);
}

/* Determine if a given IP is in the current network */
static int is_in_network(const uint8 src[4], const uint8 dest[4],
                         const uint8 netmask[4]) {
	int i;

	for(i = 0; i < 4; i++) {
		if((dest[i] & netmask[i]) != (src[i] & netmask[i]))
			return 0;
	}

	return 1;
}

/* Send a packet on the specified network adaptor */
int net_ipv4_send_packet(netif_t *net, ip_hdr_t *hdr, const uint8 *data,
                         int size) {
	uint8 dest_ip[4];
	uint8 dest_mac[6];
	uint8 pkt[size + sizeof(ip_hdr_t) + sizeof(eth_hdr_t)];
	eth_hdr_t *ehdr;

	if(net == NULL) {
		net = net_default_dev;
	}

	net_ipv4_parse_address(ntohl(hdr->dest), dest_ip);

	/* Is this the loopback address (127.0.0.1)? */
	if(ntohl(hdr->dest) == 0x7F000001) {
		/* Fill in the ethernet header */
		ehdr = (eth_hdr_t *)pkt;
		memset(ehdr->dest, 0, 6);
		memset(ehdr->src, 0, 6);

		ehdr->dest[0] = 0xCF;
		ehdr->src[0] = 0xCF;

		ehdr->type[0] = 0x08;
		ehdr->type[1] = 0x00;

		/* Put the IP header / data into our ethernet packet */
		memcpy(pkt + sizeof(eth_hdr_t), hdr,
		       4 * (hdr->version_ihl & 0x0f));
		memcpy(pkt + sizeof(eth_hdr_t) + 4 * (hdr->version_ihl & 0x0f),
		       data, size);
		
		/* Send it away */
		net_input(NULL, pkt, sizeof(eth_hdr_t) + sizeof(ip_hdr_t) +
		                     size);

		return 0;
	}
		
	/* Is it in our network? */
	if(!is_in_network(net->ip_addr, dest_ip, net->netmask)) {
		memcpy(dest_ip, net->gateway, 4);
	}

	/* Get our destination's MAC address */
	if(net_arp_lookup(net, dest_ip, dest_mac) == -1) {
		errno = ENETUNREACH;
		return -1;
	}

	/* Fill in the ethernet header */
	ehdr = (eth_hdr_t *)pkt;
	memcpy(ehdr->dest, dest_mac, 6);
	memcpy(ehdr->src, net->mac_addr, 6);
	ehdr->type[0] = 0x08;
	ehdr->type[1] = 0x00;

	/* Put the IP header / data into our ethernet packet */
	memcpy(pkt + sizeof(eth_hdr_t), hdr, 4 * (hdr->version_ihl & 0x0f));
	memcpy(pkt + sizeof(eth_hdr_t) + 4 * (hdr->version_ihl & 0x0f), data,
	       size);

	/* Send it away */
	net->if_tx(net, pkt, sizeof(ip_hdr_t) + size + sizeof(eth_hdr_t),
		   NETIF_BLOCK);

	return 0;
}

int net_ipv4_input(netif_t *src, const uint8 *pkt, int pktsize) {
	eth_hdr_t	*eth;
	ip_hdr_t	*ip;
	int		i;
	uint8 *data;
	uint16 *ethproto;

	/* Get pointers */
	eth = (eth_hdr_t*) (pkt);
	ip = (ip_hdr_t*) (pkt + sizeof(eth_hdr_t));
	data = (uint8 *) (pkt + sizeof(eth_hdr_t) + 4 *
	                  (ip->version_ihl & 0x0f));
	ethproto = (uint16 *) (pkt + 12);

	/* Make sure this packet is actually marked as an IP packet */
	if(ntohs(*ethproto) != 0x0800) {
		dbglog(DBG_KDEBUG, "net_ipv4: Discarding non IP packet\n");
		return 0;
	}

	/* Check ip header checksum */
	i = ip->checksum;
	ip->checksum = 0;
	ip->checksum = net_ipv4_checksum((uint16*)ip, 2 *
	                                 (ip->version_ihl & 0x0f));

	if(i != ip->checksum) {
		dbglog(DBG_KDEBUG, "net_ipv4: Discarding recieved IP packet "
		                   "with invalid checksum\n");
		return 0;
	}

	switch(ip->protocol) {
		case 1:
			net_icmp_input(src, eth, ip, data, ntohs(ip->length) -
			               (ip->version_ihl & 0x0f) * 4);
			break;
		case 17:
			net_udp_input(src, eth, ip, data, ntohs(ip->length) -
			              (ip->version_ihl & 0x0f) * 4);
			break;
		default:
			dbglog(DBG_KDEBUG, "net_ipv4: Discarding recieved IP "
			                   "packet with unkown protocol: %d\n",
			       ip->protocol);
	}

	return 0;
}

uint32 net_ipv4_address(const uint8 addr[4]) {
	return (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | (addr[3]);
}

void net_ipv4_parse_address(uint32 addr, uint8 out[4]) {
	out[0] = (uint8) ((addr >> 24) & 0xFF);
	out[1] = (uint8) ((addr >> 16) & 0xFF);
	out[2] = (uint8) ((addr >> 8) & 0xFF);
	out[3] = (uint8) (addr & 0xFF);
}
