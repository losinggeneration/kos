/* KallistiOS ##version##
 *
 * dc/net/broadband_adapter.h
 *
 * (c)2001-2002 Dan Potter
 *
 * $Id: broadband_adapter.h,v 1.1 2003/07/31 00:54:04 bardtx Exp $
 */

#ifndef __DC_NET_BROADBAND_ADAPTER_H
#define __DC_NET_BROADBAND_ADAPTER_H

#include <sys/cdefs.h>
__BEGIN_DECLS

/* RTL8139C register definitions */
#define RT_IDR0			0x00		/* Mac address */
#define RT_MAR0			0x08		/* Multicast filter */
#define RT_TXSTATUS0		0x10		/* Transmit status (4 32bit regs) */
#define RT_TXADDR0		0x20		/* Tx descriptors (also 4 32bit) */
#define RT_RXBUF		0x30		/* Receive buffer start address */
#define RT_RXEARLYCNT		0x34		/* Early Rx byte count */
#define RT_RXEARLYSTATUS	0x36		/* Early Rx status */
#define RT_CHIPCMD		0x37		/* Command register */
#define RT_RXBUFTAIL		0x38		/* Current address of packet read (queue tail) */
#define RT_RXBUFHEAD		0x3A		/* Current buffer address (queue head) */
#define RT_INTRMASK		0x3C		/* Interrupt mask */
#define RT_INTRSTATUS		0x3E		/* Interrupt status */
#define RT_TXCONFIG		0x40		/* Tx config */
#define RT_RXCONFIG		0x44		/* Rx config */
#define RT_TIMER		0x48		/* A general purpose counter */
#define RT_RXMISSED		0x4C		/* 24 bits valid, write clears */
#define RT_CFG9346		0x50		/* 93C46 command register */
#define RT_CONFIG0		0x51		/* Configuration reg 0 */
#define RT_CONFIG1		0x52		/* Configuration reg 1 */
#define RT_TIMERINT		0x54		/* Timer interrupt register (32 bits) */
#define RT_MEDIASTATUS		0x58		/* Media status register */
#define RT_CONFIG3		0x59		/* Config register 3 */
#define RT_CONFIG4		0x5A		/* Config register 4 */
#define RT_MULTIINTR		0x5C		/* Multiple interrupt select */
#define RT_MII_TSAD		0x60		/* Transmit status of all descriptors (16 bits) */
#define RT_MII_BMCR		0x62		/* Basic Mode Control Register (16 bits) */
#define RT_MII_BMSR		0x64		/* Basic Mode Status Register (16 bits) */
#define RT_AS_ADVERT		0x66		/* Auto-negotiation advertisement reg (16 bits) */
#define RT_AS_LPAR		0x68		/* Auto-negotiation link partner reg (16 bits) */
#define RT_AS_EXPANSION		0x6A		/* Auto-negotiation expansion reg (16 bits) */

/* RTL8193C command bits; or these together and write teh resulting value
   into CHIPCMD to execute it. */
#define RT_CMD_RESET		0x10
#define RT_CMD_RX_ENABLE	0x08
#define RT_CMD_TX_ENABLE	0x04
#define RT_CMD_RX_BUF_EMPTY	0x01

/* RTL8139C interrupt status bits */
#define RT_INT_PCIERR		0x8000		/* PCI Bus error */
#define RT_INT_TIMEOUT		0x4000		/* Set when TCTR reaches TimerInt value */
#define RT_INT_RXFIFO_OVERFLOW	0x0040		/* Rx FIFO overflow */
#define RT_INT_RXFIFO_UNDERRUN	0x0020		/* Packet underrun / link change */
#define RT_INT_LINK_CHANGE	0x0020		/* Packet underrun / link change */
#define RT_INT_RXBUF_OVERFLOW	0x0010		/* Rx BUFFER overflow */
#define RT_INT_TX_ERR		0x0008
#define RT_INT_TX_OK		0x0004
#define RT_INT_RX_ERR		0x0002
#define RT_INT_RX_OK		0x0001

/* Composite RX bits we check for while doing an RX interrupt */
#define RT_INT_RX_ACK (RT_INT_RXFIFO_OVERFLOW | RT_INT_RXBUF_OVERFLOW | RT_INT_RX_OK)

/* RTL8139C transmit status bits */
#define RT_TX_CARRIER_LOST	0x80000000	/* Carrier sense lost */
#define RT_TX_ABORTED		0x40000000	/* Transmission aborted */
#define RT_TX_OUT_OF_WINDOW	0x20000000	/* Out of window collision */
#define RT_TX_STATUS_OK		0x00008000	/* Status ok: a good packet was transmitted */
#define RT_TX_UNDERRUN		0x00004000	/* Transmit FIFO underrun */
#define RT_TX_HOST_OWNS		0x00002000	/* Set to 1 when DMA operation is completed */
#define RT_TX_SIZE_MASK		0x00001fff	/* Descriptor size mask */

/* RTL8139C receive status bits */
#define RT_RX_MULTICAST		0x00008000	/* Multicast packet */
#define RT_RX_PAM		0x00004000	/* Physical address matched */
#define RT_RX_BROADCAST		0x00002000	/* Broadcast address matched */
#define RT_RX_BAD_SYMBOL	0x00000020	/* Invalid symbol in 100TX packet */
#define RT_RX_RUNT		0x00000010	/* Packet size is <64 bytes */
#define RT_RX_TOO_LONG		0x00000008	/* Packet size is >4K bytes */
#define RT_RX_CRC_ERR		0x00000004	/* CRC error */
#define RT_RX_FRAME_ALIGN	0x00000002	/* Frame alignment error */
#define RT_RX_STATUS_OK		0x00000001	/* Status ok: a good packet was received */

/* Reads the MAC address of the BBA into the specified array */
void bba_get_mac(uint8 *arr);

/* Set an ethernet packet receive callback */
typedef void (*eth_rx_callback_t)(uint8 *pkt, int len);
void bba_set_rx_callback(eth_rx_callback_t cb);

/* Transmit a single packet */
#define BBA_TX_OK	0
#define BBA_TX_ERROR	-1
#define BBA_TX_AGAIN	-2
#define BBA_TX_NOWAIT	0
#define BBA_TX_WAIT	1
int bba_tx(const uint8 *pkt, int len, int wait);

/* Initialize */
int bba_init();

/* Shutdown */
int bba_shutdown();

__END_DECLS

#endif	/* __DC_NET_BROADBAND_ADAPTER_H */

