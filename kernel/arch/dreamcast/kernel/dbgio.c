/* KallistiOS ##version##

   dbgio.c
   Copyright (C)2000,2001,2004 Dan Potter
*/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <arch/arch.h>
#include <arch/dbgio.h>
#include <arch/spinlock.h>
#include <arch/irq.h>

CVSID("$Id: dbgio.c,v 1.4 2002/02/18 02:58:02 bardtx Exp $");

/*

This module handles very basic serial I/O using the SH4's SCIF port. FIFO
mode is used by default; you can turn this off to avoid forcing a wait
when there is no serial device attached.

Unlike in KOS 1.x, this is not designed to be used as the normal I/O, but
simply as an early debugging device in case something goes wrong in the
kernel or for debugging it.

*/

/* SCIF registers */
#define SCIFREG08(x) *((volatile uint8 *)(x))
#define SCIFREG16(x) *((volatile uint16 *)(x))
#define SCSMR2	SCIFREG16(0xffeb0000)
#define SCBRR2	SCIFREG08(0xffe80004)
#define SCSCR2	SCIFREG16(0xffe80008)
#define SCFTDR2	SCIFREG08(0xffe8000C)
#define SCFSR2	SCIFREG16(0xffe80010)
#define SCFRDR2	SCIFREG08(0xffe80014)
#define SCFCR2	SCIFREG16(0xffe80018)
#define SCFDR2	SCIFREG16(0xffe8001C)
#define SCSPTR2	SCIFREG16(0xffe80020)
#define SCLSR2	SCIFREG16(0xffe80024)

/* This redirect is here to allow you to hook the debug output in your 
   program if you want to do that. */
dbgio_printk_func dbgio_printk = dbgio_null_write;

/* Is serial enabled? This is a failsafe for making CDRs. */
static int serial_enabled = 1;

/* Default serial parameters */
static int serial_baud = DEFAULT_SERIAL_BAUD,
	serial_fifo = DEFAULT_SERIAL_FIFO;

/* Set serial parameters; this is not platform independent like I want
   it to be, but it should be generic enough to be useful. */
void dbgio_set_parameters(int baud, int fifo) {
	serial_baud = baud;
	serial_fifo = fifo;
}

/* Enable serial support */
void dbgio_enable() { serial_enabled = 1; }

/* Disable serial support */
void dbgio_disable() { serial_enabled = 0; }

/* Set another function to capture all debug output */
dbgio_printk_func dbgio_set_printk(dbgio_printk_func func) {
	dbgio_printk_func rv = dbgio_printk;
	dbgio_printk = func;
	return rv;
}

// Receive ring buffer
#define BUFSIZE 1024
static uint8 recvbuf[BUFSIZE];
static int rb_head = 0, rb_tail = 0, rb_cnt = 0;
static int rb_paused = 0;

static void rb_push_char(int c) {
	recvbuf[rb_head] = c;
	rb_head = (rb_head + 1) % BUFSIZE;
	rb_cnt++;

	// If we're within 32 bytes of being out of space, pause for
	// the moment.
	if (!rb_paused && (BUFSIZE - rb_cnt) < 32) {
		rb_paused = 1;
		SCSPTR2 = 0x20;		// Set CTS=0
	}
}

static int rb_pop_char() {
	int c;
	c = recvbuf[rb_tail];
	rb_tail = (rb_tail + 1) % BUFSIZE;
	rb_cnt--;

	// If we're paused and clear again, re-enabled receiving.
	if (rb_paused && (BUFSIZE - rb_cnt) >= 64) {
		rb_paused = 0;
		SCSPTR2 = 0x00;
	}

	return c;
}

static int rb_space_free() {
	return BUFSIZE - rb_cnt;
}

static int rb_space_used() {
	return rb_cnt;
}


/* Serial receive and receive error interrupts. When this is triggered we
   must look for available data and error conditions, and clear them all
   out if possible. If our internal ring buffer comes close to overflowing,
   the best we can do is twiddle RTS/CTS for a while. */
static void dbgio_err_irq(irq_t src, irq_context_t * cxt) {
	// Clear status bits
	SCSCR2 &= ~0x08;
	SCSCR2 |= 0x08;

	printf("dbgio_err_irq called\n");

	// Did we get an error condition?
	if (SCFSR2 & 0x9c) {	// Check ER, BRK, FER, PER
		printf("SCFSR2 status was %04x\n", SCFSR2);
		/* Try to clear it */
		SCFCR2 = 0x06;
		SCFCR2 = 0x88;
		SCFSR2 &= ~0x9c;
	}
	if (SCLSR2 & 0x01) {	/* ORER */
		printf("SCLSR2 status was %04x\n", SCLSR2);
		/* Try to clear it */
		SCFCR2 = 0x06;
		SCFCR2 = 0x88;
		SCLSR2 = 0x00;
	}
}

static void dbgio_data_irq(irq_t src, irq_context_t * cxt) {
	// Clear status bits
	SCSCR2 &= ~0x40;
	SCSCR2 |= 0x40;

	//printf("dbgio_data_irq called\n");

	// Check for received data available.
	if (SCFSR2 & 3) {
		while (SCFDR2 & 0x1f) {
			int c = SCFRDR2;
			//printf("received char '%d'\n", c);
			rb_push_char(c);
		}
		SCFSR2 &= ~3;
	}
}

/* Initialize the SCIF port; baud_rate must be at least 9600 and
   no more than 57600. 115200 does NOT work for most PCs. */
// recv trigger to 1 byte
void dbgio_init() {
	int i;
	/* int fifo = 1; */

	/* If another handler hasn't been set, then set our serial
	   write handler to capture console output. */
	if (dbgio_printk == dbgio_null_write)
		dbgio_printk = dbgio_write_str;

	/* Hook the SCIF interrupt */
	irq_set_handler(EXC_SCIF_ERI, dbgio_err_irq);
	irq_set_handler(EXC_SCIF_BRI, dbgio_err_irq);
	irq_set_handler(EXC_SCIF_RXI, dbgio_data_irq);
	*((vuint16*)0xffd0000c) |= 0x000e << 4;
	
	/* Disable interrupts, transmit/receive, and use internal clock */
	SCSCR2 = 0;

	/* Enter reset mode */
	SCFCR2 = 0x06;
	
	/* 8N1, use P0 clock */
	SCSMR2 = 0;
	
	/* If baudrate unset, set baudrate, N = P0/(32*B)-1 */
	if (SCBRR2 == 0xff)
		SCBRR2 = (50000000 / (32 * serial_baud)) - 1;

	/* Wait a bit for it to stabilize */
	for (i=0; i<10000; i++)
		asm("nop");

	/* Unreset, enable hardware flow control, triggers on 8 bytes */
	SCFCR2 = 0x48;
	
	/* Disable manual pin control */
	SCSPTR2 = 0;
	
	/* Clear status */
	(void)SCFSR2;
	SCFSR2 = 0x60;
	(void)SCLSR2;
	SCLSR2 = 0;
	
	/* Enable transmit/receive, recv/recv error ints */
	SCSCR2 = 0x78;

	/* Start off enabled */
	serial_enabled = 1;

	/* Wait a bit for it to stabilize */
	for (i=0; i<10000; i++)
		asm("nop");
}

/* Write one char to the serial port (call serial_flush()!) */
void dbgio_write(int c) {
	int timeout = 100000;

	if (!serial_enabled) return;
	
	/* Wait until the transmit buffer has space. Too long of a failure
	   is indicative of no serial cable. */
	while (!(SCFSR2 & 0x20) && timeout > 0)
		timeout--;
	if (timeout <= 0) {
		serial_enabled = 0;
		return;
	}
	
	/* Send the char */
	SCFTDR2 = c;
	
	/* Clear status */
	SCFSR2 &= 0xff9f;
}

/* Flush all FIFO'd bytes out of the serial port buffer */
void dbgio_flush() {
	int timeout = 100000;

	if (!serial_enabled) return;

	SCFSR2 &= 0xbf;

	while (!(SCFSR2 & 0x40) && timeout > 0)
		timeout--;
	if (timeout <= 0) {
		serial_enabled = 0;
		return;
	}

	SCFSR2 &= 0xbf;
}

/* Send an entire buffer */
void dbgio_write_buffer(const uint8 *data, int len) {
	while (len-- > 0)
		dbgio_write(*data++);
	dbgio_flush();
}

/* Send an entire buffer */
void dbgio_write_buffer_xlat(const uint8 *data, int len) {
	while (len-- > 0) {
		if (*data == '\n')
			dbgio_write('\r');
		dbgio_write(*data++);
	}
	dbgio_flush();
}

/* Send a string (null-terminated) */
void dbgio_write_str(const char *str) {
	dbgio_write_buffer_xlat((const uint8*)str, strlen(str));
}

/* Null write-string function for pre-init */
void dbgio_null_write(const char *str) {
}

/* Read one char from the serial port (-1 if nothing to read) */
int dbgio_read() {
	if (!serial_enabled) return -1;

	// Do we have anything ready?
	if (rb_space_used() <= 0)
		return -1;
	else
		return rb_pop_char();
}

/* Read an entire buffer (block) */
void dbgio_read_buffer(uint8 *data, int len) {
	int c;
	while (len-- > 0) {
		while ( (c = dbgio_read()) == -1)
			;
		*data++ = c;
	}
}

/* Not re-entrant */
static char printf_buf[1024];
static spinlock_t lock = SPINLOCK_INITIALIZER;

int dbgio_printf(const char *fmt, ...) {
	va_list args;
	int i;

	/* XXX This isn't correct. We could be inside an int with IRQs
	  enabled, and we could be outside an int with IRQs disabled, which
	  would cause a deadlock here. We need an irq_is_enabled()! */
	if (!irq_inside_int())
		spinlock_lock(&lock);

	va_start(args, fmt);
	i = vsprintf(printf_buf, fmt, args);
	va_end(args);

	dbgio_printk(printf_buf);

	if (!irq_inside_int())
		spinlock_unlock(&lock);

	return i;
}

