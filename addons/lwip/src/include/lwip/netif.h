/*
 * Copyright (c) 2001, Swedish Institute of Computer Science.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: netif.h,v 1.1 2002/03/24 00:24:52 bardtx Exp $
 */
#ifndef __LWIP_NETIF_H__
#define __LWIP_NETIF_H__

#include "lwip/opt.h"

#include "lwip/err.h"

#include "lwip/ip_addr.h"

#include "lwip/inet.h"
#include "lwip/pbuf.h"


struct lwipif {
  struct lwipif *next;
  u8_t num;
  struct ip_addr ip_addr;
  struct ip_addr netmask;  /* netmask in network byte order */
  struct ip_addr gw;
  char hwaddr[6];

  /* This function is called by the network device driver
     when it wants to pass a packet to the TCP/IP stack. */
  err_t (* input)(struct pbuf *p, struct lwipif *inp);

  /* The following two fields should be filled in by the
     initialization function for the device driver. */

  char name[2];
  /* This function is called by the IP module when it wants
     to send a packet on the interface. */
  err_t (* output)(struct lwipif *netif, struct pbuf *p,
		   struct ip_addr *ipaddr);
  err_t (* linkoutput)(struct lwipif *netif, struct pbuf *p);

  /* This field can be set bu the device driver and could point
     to state information for the device. */
  void *state;
};

/* The list of network interfaces. */
extern struct lwipif *netif_list;
extern struct lwipif *netif_default;


/* netif_init() must be called first. */
void netif_init();

struct lwipif *netif_add(struct ip_addr *ipaddr, struct ip_addr *netmask,
			struct ip_addr *gw,
			void (* init)(struct lwipif *netif),
			err_t (* input)(struct pbuf *p, struct lwipif *netif));

/* Returns a network interface given its name. The name is of the form
   "et0", where the first two letters are the "name" field in the
   netif structure, and the digit is in the num field in the same
   structure. */
struct lwipif *netif_find(char *name);

void netif_set_default(struct lwipif *netif);

void netif_set_ipaddr(struct lwipif *netif, struct ip_addr *ipaddr);
void netif_set_netmask(struct lwipif *netif, struct ip_addr *netmast);
void netif_set_gw(struct lwipif *netif, struct ip_addr *gw);

#endif /* __LWIP_NETIF_H__ */
