/* KallistiOS ##version##

   kernel/net/net_ipv4.c

   Copyright (C) 2005, 2006, 2007, 2008 Lawrence Sebald

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
uint16 net_ipv4_checksum(const uint8 *data, int bytes) {
    uint32 sum = 0;
    int i;

    /* Make sure we don't do any unaligned memory accesses */
    if(((uint32)data) & 0x01) {
        for(i = 0; i < bytes; i += 2) {
            sum += (data[i]) | (data[i + 1] << 8);

            if(sum & 0xFFFF0000) {
                sum &= 0xFFFF;
                ++sum;
            }
        }
    }
    else {
        uint16 *ptr = (uint16 *)data;

        for(i = 0; i < (bytes >> 1); ++i) {
            sum += ptr[i];

            if(sum & 0xFFFF0000) {
                sum &= 0xFFFF;
                ++sum;
            }
        }
    }

    /* Handle the last byte, if we have an odd byte count */
    if(bytes & 0x01) {
        sum += data[bytes - 1];

        if(sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            ++sum;
        }
    }

    return sum ^ 0xFFFF;
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

/* Send a packet on the specified network adapter */
int net_ipv4_send_packet(netif_t *net, ip_hdr_t *hdr, const uint8 *data,
                         int size) {
    uint8 dest_ip[4];
    uint8 dest_mac[6];
    uint8 pkt[size + sizeof(ip_hdr_t) + sizeof(eth_hdr_t)];
    eth_hdr_t *ehdr;
    int err;

    if(net == NULL) {
        net = net_default_dev;
    }

    net_ipv4_parse_address(ntohl(hdr->dest), dest_ip);

    /* Is this the loopback address (127.0.0.1)? */
    if(ntohl(hdr->dest) == 0x7F000001) {
        /* Put the IP header / data into our packet */
        memcpy(pkt, hdr, 4 * (hdr->version_ihl & 0x0f));
        memcpy(pkt + 4 * (hdr->version_ihl & 0x0f), data, size);
        
        /* Send it "away" */
        net_ipv4_input(NULL, pkt, 4 * (hdr->version_ihl & 0x0f) + size);

        return 0;
    }
        
    /* Is it in our network? */
    if(!is_in_network(net->ip_addr, dest_ip, net->netmask)) {
        memcpy(dest_ip, net->gateway, 4);
    }

    /* Get our destination's MAC address. If we do not have the MAC address
       cached, return a distinguished error to the upper-level protocol so
       that it can decide what to do. */
    err = net_arp_lookup(net, dest_ip, dest_mac);
    if(err == -1) {
        errno = ENETUNREACH;
        return -1;
    }
    else if(err == -2) {
        return -2;
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

int net_ipv4_send(netif_t *net, const uint8 *data, int size, int id, int ttl,
                  int proto, uint32 src, uint32 dst)    {
    ip_hdr_t hdr;

    /* Fill in the IPv4 Header */
    hdr.version_ihl = 0x45;
    hdr.tos = 0;
    hdr.length = htons(size + 20);
    hdr.packet_id = id;
    hdr.flags_frag_offs = htons(0x4000);
    hdr.ttl = ttl;
    hdr.protocol = proto;
    hdr.checksum = 0;
    hdr.src = src;
    hdr.dest = dst;

    hdr.checksum = net_ipv4_checksum((uint8 *)&hdr, sizeof(ip_hdr_t));

    return net_ipv4_send_packet(net, &hdr, data, size);
}

int net_ipv4_input(netif_t *src, const uint8 *pkt, int pktsize) {
    ip_hdr_t	*ip;
    int		i;
    uint8 *data;
    int hdrlen;

    if(pktsize < sizeof(ip_hdr_t))
        /* This is obviously a bad packet, drop it */
        return -1;

    ip = (ip_hdr_t*) pkt;
    hdrlen = (ip->version_ihl & 0x0F) << 2;

    if(pktsize < hdrlen)
        /* The packet is smaller than the listed header length, bail */
        return -1;

    data = (uint8 *) (pkt + hdrlen);

    /* Check ip header checksum */
    i = ip->checksum;
    ip->checksum = 0;
    ip->checksum = net_ipv4_checksum((uint8 *)ip, hdrlen);

    if(i != ip->checksum) {
        /* The checksums don't match, bail */
        return -1;
    }

    switch(ip->protocol) {
        case IPPROTO_ICMP:
            return net_icmp_input(src, ip, data, ntohs(ip->length) - hdrlen);           

        case IPPROTO_UDP:
            return net_udp_input(src, ip, data, ntohs(ip->length) - hdrlen);
    }

    /* There's no handler for this packet type, bail out */
    return -1;
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
