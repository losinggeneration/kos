/* KallistiOS ##version##

   modem.h
   Copyright (C)2002 Nick Kochakian

   Distributed under the terms of the KOS license.

   $Id: modem.h,v 1.1 2003/05/23 02:05:02 bardtx Exp $
*/

/*
   Dreamcast modem driver v1.1
   11/1/2002 by NickK
   http://www.boob.co.uk/

   Modem tech info can be found for a limited time at
   http://ev.dhs.org/dreamcast/
*/

#ifndef __MODEM_H
#define __MODEM_H

#include "mconst.h"

#define MAKE_DSP_ADDR(x) ((x) & 0xFF), (((x) >> 8) & 0xF)

#define MODEM_MODE_REMOTE 0   /* Dial a remote computer */
#define MODEM_MODE_DIRECT 1   /* Wait for a handshake signal */
#define MODEM_MODE_ANSWER 2   /* Answer a call when a ring is detected */
#define MODEM_MODE_NULL   255 /* Not doing anything. Don't give this to
                                 any function! */

/* V.22 bis modes */
#define MODEM_SPEED_V22BIS_1200 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V22BIS, MODEM_SPEED_1200)
#define MODEM_SPEED_V22BIS_2400 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V22BIS, MODEM_SPEED_2400)

/* V.22 modes */
#define MODEM_SPEED_V22_1200 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V22, MODEM_SPEED_1200)

/* V.32 modes */
#define MODEM_SPEED_V32_4800 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V32, MODEM_SPEED_4800)
#define MODEM_SPEED_V32_9600 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V32, MODEM_SPEED_9600)

/* V.32 bis modes */
#define MODEM_SPEED_V32BIS_7200  MODEM_MAKE_SPEED(MODEM_PROTOCOL_V32BIS, MODEM_SPEED_7200)
#define MODEM_SPEED_V32BIS_12000 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V32BIS, MODEM_SPEED_12000)
#define MODEM_SPEED_V32BIS_14400 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V32BIS, MODEM_SPEED_14400)

/* V.34 modes */
#define MODEM_SPEED_V34_2400  MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_2400)
#define MODEM_SPEED_V34_4800  MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_4800)
#define MODEM_SPEED_V34_7200  MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_7200)
#define MODEM_SPEED_V34_9600  MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_9600)
#define MODEM_SPEED_V34_12000 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_12000)
#define MODEM_SPEED_V34_14400 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_14400)
#define MODEM_SPEED_V34_16800 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_16800)
#define MODEM_SPEED_V34_19200 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_19200)
#define MODEM_SPEED_V34_21600 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_21600)
#define MODEM_SPEED_V34_24000 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_24000)
#define MODEM_SPEED_V34_26400 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_26400)
#define MODEM_SPEED_V34_28000 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_28000)
#define MODEM_SPEED_V34_31200 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_31200)
#define MODEM_SPEED_V34_33600 MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_33600)
#define MODEM_SPEED_V34_AUTO  MODEM_MAKE_SPEED(MODEM_PROTOCOL_V34, MODEM_SPEED_1200)

//From modem.c:
int           modem_init(void);
void          modem_shutdown(void);
int           modem_set_mode(int mode, mspeed_t speed);
void          modem_disconnect(void);
int           modem_is_connecting(void);
int           modem_is_connected(void);
unsigned long modem_get_connection_rate(void);

//From mdata.c:
int modem_read_data(unsigned char *data, int size);
int modem_write_data(unsigned char *data, int size);
int modem_has_data(void);

#endif
