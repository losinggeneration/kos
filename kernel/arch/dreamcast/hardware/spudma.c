/* KallistiOS ##version##
  
   spudma.c
   Copyright (C)2001,2002,2004 Dan Potter
 */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <dc/spu.h>
#include <dc/asic.h>
#include <kos/sem.h>

CVSID("$Id: spudma.c,v 1.4 2002/02/10 02:30:10 bardtx Exp $");

/*

Handles the DMA part of the SPU functionality.

Thanks to Bitmaster for the info on SPU DMA, and Roger Cattermole who
got a well-functioning PVR DMA module (helped this a bit).

XXX: Right now this conflicts with PVR DMA but we ought to be able
to fix that by going to another channel.

XXX: This ought to be abstracted out to allow usage with the parallel
port as well.

*/

typedef struct {
	uint32		ext_addr;		/* External address (SPU-RAM or parallel port) */
	uint32		sh4_addr;		/* SH-4 Address */
	uint32		size;			/* Size in bytes; all addresses and sizes must be 32-byte aligned */
	uint32		dir;			/* 0: cpu->ext; 1: ext->cpu */
	uint32		mode;			/* 5 for SPU transfer */
	uint32		ctrl1;			/* b0 */
	uint32		ctrl2;			/* b0 */
	uint32		u1;			/* ?? */
} g2_dma_ctrl_t;

typedef struct {
	uint32		ext_addr;
	uint32		sh4_addr;
	uint32		size;
	uint32		status;
} g2_dma_stat_t;

typedef struct {
	g2_dma_ctrl_t	dma[4];
	uint32		u1[4];			/* ?? */
	uint32		wait_state;
	uint32		u2[10];			/* ?? */
	uint32		magic;
	g2_dma_stat_t	dma_stat[4];
} g2_dma_reg_t;

/* DMA registers */
static vuint32	* const shdma = (vuint32 *)0xffa00000;
static volatile g2_dma_reg_t * const extdma = (g2_dma_reg_t *)0xa05f7800;
const int	chn = 0;	/* 0 for SPU; 1, 2, 3 for EXT */

/* DMAC registers. We use channel 3 here to avoid conflicts with the PVR. */
#define DMAC_SAR3	0x30/4
#define DMAC_DAR3	0x34/4
#define DMAC_DMATCR3	0x38/4
#define DMAC_CHCR3	0x3c/4
#define DMAC_DMAOR	0x40/4

/* Signaling semaphore */
static semaphore_t * dma_done;
static int dma_blocking;   
static spu_dma_callback_t dma_callback;
static ptr_t dma_cbdata;

static void dma_disable() {
	/* Disable the DMA */
	extdma->dma[chn].ctrl1 = 0;
	extdma->dma[chn].ctrl2 = 0;
	shdma[DMAC_CHCR3] &= ~1;
}

static void spu_dma_irq(uint32 code) {
	if (shdma[DMAC_DMATCR3] != 0)
		dbglog(DBG_INFO, "spu_dma: DMA did not complete successfully\n");

	dma_disable();

	// Call the callback, if any.
	if (dma_callback) {
		dma_callback(dma_cbdata);
		dma_callback = NULL;
		dma_cbdata = 0;
	}

	// Signal the calling thread to continue, if any.
	if (dma_blocking) {
		sem_signal(dma_done);
		thd_schedule(1, 0);
		dma_blocking = 0;
	}
}

// XXX: Write (and check here) spu_dma_ready()
int spu_dma_transfer(void *from, uint32 dest, uint32 length, int block,
	spu_dma_callback_t callback, ptr_t cbdata)
{
	uint32 val;

	/* Check alignments */
	if ( ((uint32)from) & 31 ) {
		dbglog(DBG_ERROR, "spu_dma: unaligned source DMA %p\n", from);
		errno = EFAULT;
		return -1;
	}
	if ( ((uint32)dest) & 31 ) {
		dbglog(DBG_ERROR, "spu_dma: unaligned dest DMA %p\n", (void *)dest);
		errno = EFAULT;
		return -1;
	}
	length = (length + 0x1f) & ~0x1f;

	/* Adjust destination to SPU RAM */
	dest += 0x00800000;

	val = shdma[DMAC_CHCR3];

	// DE bit set so we must clear it?
	if (val & 1)
		shdma[DMAC_CHCR3] = val | 1;
	// TE bit set so we must clear it?
	if (val & 2)
		shdma[DMAC_CHCR3] = val | 2;

	/* Setup the SH-4 channel */
	shdma[DMAC_SAR3] = 0;		/* SAR3 = 0 */
	shdma[DMAC_DMATCR3] = 0;	/* DMATCR3 = 0 */
	shdma[DMAC_CHCR3] = 0x12c1;	/* CHCR3 = 0x12c0; 32-byte block transfer,
					   burst mode, external request, single address mode,
					   source address incremented; */

	val = shdma[DMAC_DMAOR];
	if ((val & 0x8007) != 0x8001) {
		dbglog(DBG_ERROR, "spu_dma: failed DMAOR check\n");
		errno = EIO;
		return -1;
	}

	dma_blocking = block;
	dma_callback = callback;
	dma_cbdata = cbdata;

	/* Start the DMA transfer */
	extdma->dma[chn].ctrl1 = 0;
	extdma->dma[chn].ctrl2 = 0;
	extdma->dma[chn].ext_addr = dest & 0x1fffffe0;
	extdma->dma[chn].sh4_addr = ((uint32)from) & 0x1fffffe0;
	extdma->dma[chn].size = (length & ~31) | 0x80000000;
	extdma->dma[chn].dir = 0;
	extdma->dma[chn].mode = 5;	/* SPU == 5 */
	extdma->dma[chn].ctrl1 = 1;
	extdma->dma[chn].ctrl2 = 1;

	/* Wait for us to be signaled */
	if (block)
		sem_wait(dma_done);

	return 0;
}

int spu_dma_init() {
	/* Create an initially blocked semaphore */
	dma_done = sem_create(0);
	dma_blocking = 0;
	dma_callback = NULL;
	dma_cbdata = 0;

	// Hook the interrupt
	asic_evt_set_handler(ASIC_EVT_SPU_DMA, spu_dma_irq);
	asic_evt_enable(ASIC_EVT_PVR_DMA, ASIC_IRQ_DEFAULT);

	/* Setup the DMA transfer on the external side */
	extdma->wait_state = 27;
	extdma->magic = 0x4659404f;

	return 0;
}

void spu_dma_shutdown() {
	// Unhook the G2 interrupt
	asic_evt_disable(ASIC_EVT_PVR_DMA, ASIC_IRQ_DEFAULT);
	asic_evt_set_handler(ASIC_EVT_SPU_DMA, NULL);

	/* Destroy the semaphore */
	sem_destroy(dma_done);

	/* Turn off any remaining DMA */
	dma_disable();
}

