/* KallistiOS ##version##

   snd_iface.c
   (c)2000-2002 Dan Potter

   SH-4 support routines for accessing the AICA via the standard KOS driver
*/

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <kos/sem.h>
#include <arch/timer.h>
#include <dc/g2bus.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>

#include "arm/aica_cmd_iface.h"

CVSID("$Id: snd_iface.c,v 1.5 2002/09/13 05:03:10 bardtx Exp $");

/* the address of the sound ram from the SH4 side */
#define SPU_RAM_BASE		0xa0800000

/* Are we initted? */
static int initted = 0;

/* This will come from a seperately linked object file */
extern uint8 snd_stream_drv[];
extern uint8 snd_stream_drv_end[];

/* Thread semaphore */
static semaphore_t * sem_qram;

/* Initialize driver; note that this replaces the AICA program so that
   if you had anything else going on, it's gone now! */
int snd_init() {
	int amt;

	/* Finish loading the stream driver */
	if (!initted) {
		spu_disable();
		spu_memset(0, 0, AICA_RAM_START);
		amt = snd_stream_drv_end - snd_stream_drv;
		if (amt % 4)
			amt = (amt + 4) & ~3;
		printf("snd_init(): loading %d bytes into SPU RAM\n", amt);
		spu_memload(0, snd_stream_drv, amt);

		/* Enable the AICA and give it a few ms to start up */
		spu_enable();
		timer_spin_sleep(10);

		/* Initialize the RAM allocator */
		snd_mem_init(AICA_RAM_START);

		/* Setup semaphores */
		sem_qram = sem_create(1);
	}

	initted = 1;
	
	return 0;
}

/* Shut everything down and free mem */
void snd_shutdown() {
	if (initted) {
		spu_disable();
		sem_destroy(sem_qram);
		snd_mem_shutdown();
		initted = 0;
	}
}

/* Submit a request to the SH4->AICA queue; size is in uint32's */
int snd_sh4_to_aica(void *packet, uint32 size) {
	uint32	qa, bot, start, top, *pkt32, cnt;
	assert_msg( size < 256, "SH4->AICA packets may not be >256 uint32's long" );

	sem_wait(sem_qram);

	/* Set these up for reference */
	qa = SPU_RAM_BASE + AICA_MEM_CMD_QUEUE;
	assert_msg( g2_read_32(qa + offsetof(aica_queue_t, valid)), "Queue is not yet valid" );

	bot = SPU_RAM_BASE + g2_read_32(qa + offsetof(aica_queue_t, data));
	top = bot + g2_read_32(qa + offsetof(aica_queue_t, size));
	start = bot + g2_read_32(qa + offsetof(aica_queue_t, head));
	pkt32 = (uint32 *)packet;
	cnt = 0;

	while (size > 0) {
		/* Fifo wait if necessary */
		if (!(cnt % 8))
			g2_fifo_wait();
		cnt++;

		/* Write the next dword */
		g2_write_32(start, *pkt32++);

		/* Move our counters */
		start += 4;
		if (start >= top)
			start = bot;
		size--;
	}

	/* Finally, write a new head value to signify that we've added
	   a packet for it to process */
	g2_write_32(qa + offsetof(aica_queue_t, head), start - bot);

	/* We could wait until head == tail here for processing, but there's
	   not really much point; it'll just slow things down. */

	sem_signal(sem_qram);

	return 0;
}

/* Start processing requests in the queue */
void snd_sh4_to_aica_start() {
	g2_write_32(SPU_RAM_BASE + AICA_MEM_CMD_QUEUE + offsetof(aica_queue_t, process_ok), 1);
}

/* Stop processing requests in the queue */
void snd_sh4_to_aica_stop() {
	g2_write_32(SPU_RAM_BASE + AICA_MEM_CMD_QUEUE + offsetof(aica_queue_t, process_ok), 0);
}

/* Transfer one packet of data from the AICA->SH4 queue. Expects to
   find AICA_CMD_MAX_SIZE dwords of space available. Returns -1
   if failure, 0 for no packets available, 1 otherwise. Failure
   might mean a permanent failure since the queue is probably out of sync. */
int snd_aica_to_sh4(void *packetout) {
	uint32	bot, start, stop, top, size, cnt, *pkt32;

	sem_wait(sem_qram);

	/* Set these up for reference */
	bot = SPU_RAM_BASE + AICA_MEM_RESP_QUEUE;
	assert_msg( g2_read_32(bot + offsetof(aica_queue_t, valid)), "Queue is not yet valid" );

	top = SPU_RAM_BASE + AICA_MEM_RESP_QUEUE + g2_read_32(bot + offsetof(aica_queue_t, size));
	start = SPU_RAM_BASE + AICA_MEM_RESP_QUEUE + g2_read_32(bot + offsetof(aica_queue_t, tail));
	stop = SPU_RAM_BASE + AICA_MEM_RESP_QUEUE + g2_read_32(bot + offsetof(aica_queue_t, head));
	cnt = 0;
	pkt32 = (uint32 *)packetout;

	/* Is there anything? */
	if (start == stop) {
		sem_signal(sem_qram);
		return 0;
	}

	/* Check for packet size overflow */
	size = g2_read_32(start + offsetof(aica_cmd_t, size));
	if (cnt >= AICA_CMD_MAX_SIZE) {
		sem_signal(sem_qram);
		dbglog(DBG_ERROR, "snd_aica_to_sh4(): packet larger than %d dwords\n", AICA_CMD_MAX_SIZE);
		return -1;
	}

	/* Find stop point for this packet */
	stop = start + size*4;
	if (stop > top)
		stop -= top - (SPU_RAM_BASE + AICA_MEM_RESP_QUEUE);
	
	while (start != stop) {
		/* Fifo wait if necessary */
		if (!(cnt % 8))
			g2_fifo_wait();
		cnt++;

		/* Read the next dword */
		*pkt32++ = g2_read_32(start);

		/* Move our counters */
		start += 4;
		if (start >= top)
			start = bot;
		cnt++;
	}

	/* Finally, write a new tail value to signify that we've removed a packet */
	g2_write_32(bot + offsetof(aica_queue_t, tail), start - (SPU_RAM_BASE + AICA_MEM_RESP_QUEUE));

	sem_signal(sem_qram);

	return 1;
}

/* Poll for responses from the AICA. We assume here that we're not
   running in an interrupt handler (thread perhaps, of whoever
   is using us). */
void snd_poll_resp() {
	int		rv;
	uint32		pkt[AICA_CMD_MAX_SIZE];
	aica_cmd_t	* pktcmd;

	pktcmd = (aica_cmd_t *)pkt;

	while ( (rv = snd_aica_to_sh4(pkt)) > 0 ) {
		dbglog(DBG_DEBUG, "snd_poll_resp(): Received packet id %08lx, ts %08lx from AICA\n",
			pktcmd->cmd, pktcmd->timestamp);
	}
	if (rv < 0)
		dbglog(DBG_ERROR, "snd_poll_resp(): snd_aica_to_sh4 failed, giving up\n");
}
