/* lwip/dhcp.h
		10-10-2000	LW	Created file.
		26-10-2000	LW	Added DHCP_OVERLOAD_*
*/

#ifndef __LWIP_DHCP_H__
#define __LWIP_DHCP_H__

#include "udp.h"
#include "lwip/netif.h"
#include "lwipopts.h"

// Two timer mechanisms:
// 0 if you would periodically call dhcp_coarse_tmr()
// and dhcp_fine_mtr() from your application.
// 1 if you want to use timers that can do function callbacks
#define DHCP_TIMER_CALLBACKS 0 
// period (in seconds) of the application calling dhcp_coarse_tmr()
#define DHCP_COARSE_TIMER_SECS 60 
// period (in milliseconds) of the application calling dhcp_fine_tmr()
#define DHCP_FINE_TIMER_MSECS 500 

struct dhcp_state
{
  struct dhcp_state *next; // for linked list purposes
  u8_t state; // current DHCP state (of DHCP state machine)
  u8_t tries; // retries of current request
  u32_t xid; // id of last sent request
	struct lwipif *netif; // interface to be configured
	struct udp_pcb *pcb; // our connection

  struct pbuf *p; // (first) pbuf of incoming msg
  struct dhcp_msg *msg_in; // incoming msg
  struct dhcp_msg *options_in; // incoming msg options
  u16_t options_in_len; // ingoing msg options length

  struct pbuf *p_out; // pbuf of outcoming msg
  struct dhcp_msg *msg_out; // outgoing msg
  u16_t options_out_len; // outgoing msg options length
#if DHCP_TIMER_CALLBACKS
	u8_t request_timer;	// timer identifier that handles the request timeout (msecs)
  u8_t t1_timer; // timer identifier that handles the renewal timeout
  u8_t t2_timer;// timer identifier that handles the rebind timeout
#else
	u16_t request_timeout; // #ticks with period DHCP_FINE_TIMER_SECS for request timeout
	u16_t t1_timeout;	// #ticks with period DHCP_COARSE_TIMER_SECS for renewal time
	u16_t t2_timeout;	// #ticks with period DHCP_COARSE_TIMER_SECS for rebind time
#endif
  struct ip_addr server_ip_addr; // dhcp server address that offered this lease 
  struct ip_addr offered_ip_addr;
  struct ip_addr offered_sn_mask;
  struct ip_addr offered_gw_addr;
  struct ip_addr offered_bc_addr;
  u32_t offered_t0_lease; // lease period (in seconds)
  u32_t offered_t1_renew; // recommended renew time (usually 50% of lease period)
  u32_t offered_t2_rebind; // recommended rebind time (usually 66% of lease period)
};

// always present fields of any DHCP message
struct dhcp_msg
{
  u8_t op;
  u8_t htype;
  u8_t hlen;
  u8_t hops;
  u32_t xid;
  u16_t secs;
  u16_t flags;
  u32_t ciaddr;
  u32_t yiaddr;
  u32_t siaddr;
  u32_t giaddr;
#define DHCP_CHADDR_LEN 16U
  u8_t chaddr[DHCP_CHADDR_LEN];
#define DHCP_SNAME_LEN 64U
  u8_t sname[DHCP_SNAME_LEN];
#define DHCP_FILE_LEN 128U
  u8_t file[DHCP_FILE_LEN];
  u32_t cookie;
#define DHCP_MIN_OPTIONS_LEN 68U
// allow this to be configured in lwipopts.h, but not too small
#if ((!defined(DHCP_OPTIONS_LEN)) || (DHCP_OPTIONS_LEN < DHCP_MIN_OPTIONS_LEN))
// set this to be sufficient for your options in outgoing DHCP msgs
#  define DHCP_OPTIONS_LEN DHCP_MIN_OPTIONS_LEN
#endif
	u8_t options[DHCP_OPTIONS_LEN];
};

// initialize DHCP client (pool)
void dhcp_init(void);
// start DHCP configuration
struct dhcp_state *dhcp_start(struct lwipif *netif);
void dhcp_stop(struct dhcp_state *state);
err_t dhcp_renew(struct dhcp_state *state);
err_t dhcp_inform(struct dhcp_state *state);

#if	DHCP_DOES_ARP_CHECK
void dhcp_arp_reply(struct ip_addr *addr);
#endif
#if (DHCP_TIMER_CALLBACKS == 0)
void dhcp_coarse_tmr(void);
void dhcp_fine_tmr(void);
#endif
 
// DHCP message item offsets and length 
#define DHCP_MSG_OFS (UDP_DATA_OFS)  
  #define DHCP_OP_OFS (DHCP_MSG_OFS + 0)
  #define DHCP_HTYPE_OFS (DHCP_MSG_OFS + 1)
  #define DHCP_HLEN_OFS (DHCP_MSG_OFS + 2)
  #define DHCP_HOPS_OFS (DHCP_MSG_OFS + 3)
  #define DHCP_XID_OFS (DHCP_MSG_OFS + 4)
  #define DHCP_SECS_OFS (DHCP_MSG_OFS + 8)
  #define DHCP_FLAGS_OFS (DHCP_MSG_OFS + 10)
  #define DHCP_CIADDR_OFS (DHCP_MSG_OFS + 12)
  #define DHCP_YIADDR_OFS (DHCP_MSG_OFS + 16)
  #define DHCP_SIADDR_OFS (DHCP_MSG_OFS + 20)
  #define DHCP_GIADDR_OFS (DHCP_MSG_OFS + 24)
  #define DHCP_CHADDR_OFS (DHCP_MSG_OFS + 28)
  #define DHCP_SNAME_OFS (DHCP_MSG_OFS + 44)
  #define DHCP_FILE_OFS (DHCP_MSG_OFS + 108)
#define DHCP_MSG_LEN 236

#define DHCP_COOKIE_OFS (DHCP_MSG_OFS + DHCP_MSG_LEN)
#define DHCP_OPTIONS_OFS (DHCP_MSG_OFS + DHCP_MSG_LEN + 4)

#define DHCP_CLIENT_PORT 68	
#define DHCP_SERVER_PORT 67

// DHCP client states
#define DHCP_REQUESTING 1
#define DHCP_INIT 2
#define DHCP_REBOOTING 3
#define DHCP_REBINDING 4
#define DHCP_RENEWING 5
#define DHCP_SELECTING 6
#define DHCP_INFORMING 7
#define DHCP_CHECKING 8
#define DHCP_PERMANENT 9
#define DHCP_BOUND 10
#define DHCP_BACKING_OFF 11
#define DHCP_OFF 12
 
#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY 2

#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK 5
#define DHCP_NAK 6
#define DHCP_RELEASE 7
#define DHCP_INFORM 8

#define DHCP_HTYPE_ETH 1

#define DHCP_HLEN_ETH 6

#define DHCP_BROADCAST_FLAG 15
#define DHCP_BROADCAST_MASK (1 << DHCP_FLAG_BROADCAST)

// BootP options
#define DHCP_OPTION_PAD 0
#define DHCP_OPTION_SUBNET_MASK 1 // RFC 2132 3.3
#define DHCP_OPTION_ROUTER 3 
#define DHCP_OPTION_HOSTNAME 12
#define DHCP_OPTION_IP_TTL 23
#define DHCP_OPTION_MTU 26
#define DHCP_OPTION_BROADCAST 28
#define DHCP_OPTION_TCP_TTL 37
#define DHCP_OPTION_END 255

// DHCP options
#define DHCP_OPTION_REQUESTED_IP 50 // RFC 2132 9.1, requested IP address
#define DHCP_OPTION_LEASE_TIME 51 // RFC 2132 9.2, time in seconds, in 4 bytes 
#define DHCP_OPTION_OVERLOAD 52 // RFC2132 9.3, use file and/or sname field for options

#define DHCP_OPTION_MESSAGE_TYPE 53 // RFC 2132 9.6, important for DHCP
#define DHCP_OPTION_MESSAGE_TYPE_LEN 1


#define DHCP_OPTION_SERVER_ID 54 // RFC 2131 9.7, server IP address
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55 // RFC 2131 9.8, requested option types

#define DHCP_OPTION_MAX_MSG_SIZE 57 // RFC 2131 9.10, message size accepted >= 576
#define DHCP_OPTION_MAX_MSG_SIZE_LEN 2

#define DHCP_OPTION_T1 58 // T1 renewal time
#define DHCP_OPTION_T2 59 // T2 rebinding time
#define DHCP_OPTION_CLIENT_ID 61
#define DHCP_OPTION_TFTP_SERVERNAME 66
#define DHCP_OPTION_BOOTFILE 67

// possible combinations of overloading	the file and sname fields with options
#define DHCP_OVERLOAD_NONE 0
#define DHCP_OVERLOAD_FILE 1
#define DHCP_OVERLOAD_SNAME	2
#define DHCP_OVERLOAD_SNAME_FILE 3

#endif //__LWIP_DHCP_H__
