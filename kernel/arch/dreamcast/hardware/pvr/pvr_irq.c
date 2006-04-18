/* KallistiOS ##version##

   pvr_irq.c
   Copyright (C)2002,2004 Dan Potter

 */

#include <assert.h>
#include <dc/pvr.h>
#include <dc/video.h>
#include <dc/asic.h>
#include <arch/cache.h>
#include "pvr_internal.h"

/*
   PVR interrupt handler; the way things are setup, we're gonna get
   one of these for each full vertical refresh and at the completion
   of TA data acceptance. The timing here is pretty critical. We need
   to flip pages during a vertical blank, and then signal to the program
   that it's ok to start playing with TA registers again, or we waste
   rendering time.
*/

CVSID("$Id: pvr_irq.c,v 1.10 2003/04/24 03:12:25 bardtx Exp $");

// Find the next list to DMA out. If we have none left to do, then do
// nothing. Otherwise, start the DMA and chain back to us upon completion.
static void dma_next_list(ptr_t data) {
	int i, did = 0;
	volatile pvr_dma_buffers_t * b;

	// DBG(("dma_next_list\n"));

	for (i=0; i<PVR_OPB_COUNT; i++) {
		if ((pvr_state.lists_enabled & (1 << i))
			&& !(pvr_state.lists_dmaed & (1 << i)))
		{
			// Get the buffers for this frame.
			b = pvr_state.dma_buffers + (pvr_state.ram_target ^ 1);

			// Flush the last 32 bytes out of dcache, just in case.
			// dcache_flush_range((ptr_t)(b->base[i] + b->ptr[i] - 32), 32);
			dcache_flush_range((ptr_t)(b->base[i]), b->ptr[i] + 32);
			//amt = b->ptr[i] > 16384 ? 16384 : b->ptr[i];
			//dcache_flush_range((ptr_t)(b->base[i] + b->ptr[i] - amt), amt);

			// Start the DMA transfer, chaining to ourselves.
			//DBG(("dma_begin(buf %d, list %d, base %p, len %d)\n",
			//	pvr_state.ram_target ^ 1, i,
			//	b->base[i], b->ptr[i]));
			pvr_dma_load_ta(b->base[i], b->ptr[i], 0, dma_next_list, 0);

			// Mark this list as done, and break out for now.
			pvr_state.lists_dmaed |= 1 << i;
			did++;

			break;
		}
	}

	// If that was the last one, then free up the DMA channel.
	if (!did) {
		//DBG(("dma_complete(buf %d)\n", pvr_state.ram_target ^ 1));

		// Unlock
		mutex_unlock(pvr_state.dma_lock);
		pvr_state.lists_dmaed = 0;

		// Buffers are now empty again
		pvr_state.dma_buffers[pvr_state.ram_target ^ 1].ready = 0;

		// Signal the client code to continue onwards.
		sem_signal(pvr_state.ready_sem);
		thd_schedule(1,0);
	}
}

void pvr_int_handler(uint32 code) {
	// What kind of event did we get?
	switch(code) {
	case ASIC_EVT_PVR_OPAQUEDONE:
		//DBG(("irq_opaquedone\n"));
		pvr_state.lists_transferred |= 1 << PVR_OPB_OP;
		break;
	case ASIC_EVT_PVR_TRANSDONE:
		//DBG(("irq_transdone\n"));
		pvr_state.lists_transferred |= 1 << PVR_OPB_TP;
		break;
	case ASIC_EVT_PVR_OPAQUEMODDONE:
		pvr_state.lists_transferred |= 1 << PVR_OPB_OM;
		break;
	case ASIC_EVT_PVR_TRANSMODDONE:
		pvr_state.lists_transferred |= 1 << PVR_OPB_TM;
		break;
	case ASIC_EVT_PVR_PTDONE:
		pvr_state.lists_transferred |= 1 << PVR_OPB_PT;
		break;
	case ASIC_EVT_PVR_RENDERDONE:
		//DBG(("irq_renderdone\n"));
		pvr_state.render_busy = 0;
		pvr_state.render_completed = 1;
		pvr_sync_stats(PVR_SYNC_RNDDONE);
		break;
	case ASIC_EVT_PVR_VBLINT:
		pvr_sync_stats(PVR_SYNC_VBLANK);
		break;
	}

	/* Update our stats if we finished all registration */
	switch (code) {
	case ASIC_EVT_PVR_OPAQUEDONE:
	case ASIC_EVT_PVR_TRANSDONE:
	case ASIC_EVT_PVR_OPAQUEMODDONE:
	case ASIC_EVT_PVR_TRANSMODDONE:
	case ASIC_EVT_PVR_PTDONE:
		if (pvr_state.lists_transferred == pvr_state.lists_enabled) {
			pvr_sync_stats(PVR_SYNC_REGDONE);
		}
		return;
	}

	// If it's not a vblank, ignore for now for the rest of this.
	if (code != ASIC_EVT_PVR_VBLINT)
		return;

	// If the render-done interrupt has fired then we are ready to flip to the
	// new frame buffer.
	if (pvr_state.render_completed) {
		//DBG(("view(%d)\n", pvr_state.view_target ^ 1));

		// Handle PVR stats
		pvr_sync_stats(PVR_SYNC_PAGEFLIP);

		// Switch view address to the "good" buffer
		if(pvr_state.to_texture != 2)
			pvr_state.view_target ^= 1;
		pvr_sync_view();

		// Clear the render completed flag.
		pvr_state.render_completed = 0;

		// Set the fact that the next frame should be texture rendered, if it is the case
		pvr_state.to_texture = pvr_state.to_texture == 1 ? 2 : 0;
	}

	// If all lists are fully transferred and a render is not in progress,
	// we are ready to start rendering.
	if (!pvr_state.render_busy	
		&& pvr_state.lists_transferred == pvr_state.lists_enabled)
	{
		/* XXX Note:
		   For some reason, the render must be started _before_ we sync
		   to the new reg buffers. The only reasons I can think of for this
		   are that there may be something in the reg sync that messes up
		   the render in progress, or we are misusing some bits somewhere. */

		// Begin rendering from the dirty TA buffer into the clean
		// frame buffer.
		//DBG(("start_render(%d -> %d)\n", pvr_state.ta_target, pvr_state.view_target ^ 1));
		pvr_state.ta_target ^= 1;
		pvr_begin_queued_render();
		pvr_state.render_busy = 1;
		pvr_sync_stats(PVR_SYNC_RNDSTART);

		// If we're not in DMA mode, then signal the client code
		// to continue onwards.
		if (!pvr_state.dma_mode) {
			sem_signal(pvr_state.ready_sem);
			thd_schedule(1,0);
		}

		// Switch to the clean TA buffer.
		pvr_state.lists_transferred = 0;
		pvr_sync_reg_buffer();

		// The TA is no longer busy.
		pvr_state.ta_busy = 0;
	}

	// If we're in DMA mode, the DMA source buffers are ready, and a DMA
	// is not in progress, then we are ready to start DMAing.
	if (pvr_state.dma_mode
		&& !pvr_state.ta_busy
		&& pvr_state.dma_buffers[pvr_state.ram_target ^ 1].ready
		&& mutex_trylock(pvr_state.dma_lock) >= 0)
	{
		pvr_sync_stats(PVR_SYNC_REGSTART);

		// Begin DMAing the first list.
		pvr_state.ta_busy = 1;
		dma_next_list(0);
	}
}


