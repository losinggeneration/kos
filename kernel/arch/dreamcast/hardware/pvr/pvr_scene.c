/* KallistiOS ##version##

   pvr_scene.c
   Copyright (C)2002,2004 Dan Potter

 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <kos/thread.h>
#include <dc/pvr.h>
#include <dc/sq.h>
#include "pvr_internal.h"

/*

   Scene rendering

   Please see ../../include/dc/pvr.h for more info on this API!
 
*/

CVSID("$Id: pvr_scene.c,v 1.9 2003/03/09 01:19:31 bardtx Exp $");

void * pvr_set_vertex_buffer(pvr_list_t list, void * buffer, int len) {
	void * oldbuf;

	// Make sure we have global DMA usage enabled. The DMA can still
	// be used in other situations, but the user must take care of
	// that themselves.
	assert( pvr_state.dma_mode );

	// Make sure it's a valid list.
	assert( list >= 0 && list < PVR_OPB_COUNT );

	// Make sure it's an _enabled_ list.
	assert( pvr_state.lists_enabled & (1 << list) );

	// Make sure the buffer parameters are valid.
	assert( !( ((ptr_t)buffer) & 31 ) );
	assert( !( len & 63 ) );

	// Save the old value.
	oldbuf = pvr_state.dma_buffers[0].base[list];

	// Write new values.
	pvr_state.dma_buffers[0].base[list] = (uint8 *)buffer;
	pvr_state.dma_buffers[0].ptr[list] = 0;
	pvr_state.dma_buffers[0].size[list] = len / 2;
	pvr_state.dma_buffers[0].ready = 0;
	pvr_state.dma_buffers[1].base[list] = ((uint8 *)buffer) + len / 2;
	pvr_state.dma_buffers[1].ptr[list] = 0;
	pvr_state.dma_buffers[1].size[list] = len / 2;
	pvr_state.dma_buffers[1].ready = 0;

	return oldbuf;
}

void * pvr_vertbuf_tail(pvr_list_t list) {
	uint8 * bufbase;

	// Check the validity of the request.
	assert( list >= 0 && list < PVR_OPB_COUNT );
	assert( pvr_state.dma_mode );

	// Get the buffer base.
	bufbase = pvr_state.dma_buffers[pvr_state.ram_target].base[list];
	assert( bufbase );

	// Return the current end of the buffer.
	return bufbase + pvr_state.dma_buffers[pvr_state.ram_target].ptr[list];
}

void pvr_vertbuf_written(pvr_list_t list, uint32 amt) {
	uint32 val;

	// Check the validity of the request.
	assert( list >= 0 && list < PVR_OPB_COUNT );
	assert( pvr_state.dma_mode );

	// Change the current end of the buffer.
	val = pvr_state.dma_buffers[pvr_state.ram_target].ptr[list];
	val += amt;
	assert( val < pvr_state.dma_buffers[pvr_state.ram_target].size[list] );
	pvr_state.dma_buffers[pvr_state.ram_target].ptr[list] = val;
}

/* Begin collecting data for a frame of 3D output to the off-screen
   frame buffer */
void pvr_scene_begin() {
	int i;

	// Clear these out in case we're using DMA.
	if (pvr_state.dma_mode) {
		for (i=0; i<PVR_OPB_COUNT; i++) {
			pvr_state.dma_buffers[pvr_state.ram_target].ptr[i] = 0;
		}
		pvr_sync_stats(PVR_SYNC_BUFSTART);
		DBG(("pvr_scene_begin(dma -> %d)\n", pvr_state.ram_target));
	} else {
		// Get general stuff ready.
		pvr_state.list_reg_open = -1;
		pvr_state.lists_closed = 0;

		// We assume registration is starting immediately
		pvr_sync_stats(PVR_SYNC_REGSTART);
	}
}

/* Begin collecting data for a frame of 3D output to the specified texture;
   pass in the size of the buffer in rx and ry, and the return values in
   rx and ry will be the size actually used (if changed). Note that
   currently this only supports screen-sized output! */
void pvr_scene_begin_txr(pvr_ptr_t txr, uint32 *rx, uint32 *ry) {
	assert_msg(0, "not implemented yet");
}

/* Begin collecting data for the given list type. Lists do not have to be
   submitted in any particular order, but all types of a list must be 
   submitted at once. If the given list has already been closed, then an
   error (-1) is returned. */
int pvr_list_begin(pvr_list_t list) {
	/* Check to make sure we can do this */
#ifndef NDEBUG
	if (pvr_state.lists_closed & (1 << list)) {
		dbglog(DBG_WARNING, "pvr_list_begin: attempt to open already closed list\n");
		return -1;
	}
#endif	/* !NDEBUG */

	assert( !pvr_state.dma_mode );

	/* If we already had a list open, close it first */
	if (pvr_state.list_reg_open != -1)
		pvr_list_finish();

	/* Ok, set the flag */
	pvr_state.list_reg_open = list;

	return 0;
}

/* End collecting data for the current list type. Lists can never be opened
   again within a single frame once they have been closed. Thus submitting
   a primitive that belongs in a closed list is considered an error. Closing
   a list that is already closed is also an error (-1). Note that if you open
   a list but do not submit any primitives, this causes a hardware error. For
   simplicity we just always submit a blank primitive. */
static int primcnt = 0;
int pvr_list_finish() {
	/* Check to make sure we can do this */
#ifndef NDEBUG
	if (pvr_state.list_reg_open == -1) {
		dbglog(DBG_WARNING, "pvr_list_finish: attempt to close unopened list\n");
		return -1;
	}
#endif	/* !NDEBUG */

	assert( !pvr_state.dma_mode );

	/* In case we haven't sent anything in this list, send a dummy */
	if (!primcnt)
		pvr_blank_polyhdr(pvr_state.list_reg_open);
	primcnt = 0;
	
	/* Set the flags */
	pvr_state.lists_closed |= (1 << pvr_state.list_reg_open);
	pvr_state.list_reg_open = -1;

	/* Send an EOL marker */
	sq_set32((void *)PVR_TA_INPUT, 0, 32);

	return 0;
}

/* Submit a primitive of the _current_ list type; note that any values
   submitted in this fashion will go directly to the hardware without any
   sort of buffering, and submitting a primitive of the wrong type will
   quite likely ruin your scene. Note that this also will not work if you
   haven't begun any list types (i.e., all data is queued). Returns -1 for
   failure. */
int pvr_prim(void * data, int size) {
	/* Check to make sure we can do this */
#ifndef NDEBUG
	if (pvr_state.list_reg_open == -1) {
		dbglog(DBG_WARNING, "pvr_prim: attempt to submit to unopened list\n");
		return -1;
	}

	// Put this inside here since it could slow things down a lot.
	assert( !pvr_state.dma_mode );
#endif	/* !NDEBUG */

	primcnt=1;

	/* Send the data */
	sq_cpy((void *)PVR_TA_INPUT, data, size);
	
	return 0;
}

int pvr_list_prim(pvr_list_t list, void * data, int size) {
	volatile pvr_dma_buffers_t * b;
	
	b = pvr_state.dma_buffers + pvr_state.ram_target;
	assert( b->base[list] );

	assert( !(size & 31 ) );

	memcpy(b->base[list] + b->ptr[list], data, size);
	b->ptr[list] += size;
	assert( b->ptr[list] <= b->size[list] );

	return 0;
}

int pvr_list_flush(pvr_list_t list) {
	assert_msg(0, "not implemented yet");
	return -1;
}

/* Call this after you have finished submitting all data for a frame; once
   this has been called, you can not submit any more data until one of the
   pvr_scene_begin() functions is called again. An error (-1) is returned if
   you have not started a scene already. */
int pvr_scene_finish() {
	int i, o;
	volatile pvr_dma_buffers_t * b;

	// If we're in DMA mode, then this works a little differently...
	if (pvr_state.dma_mode) {
		DBG(("pvr_scene_finish(dma -> %d)\n", pvr_state.ram_target));
		// If any enabled lists are empty, fill them with a blank polyhdr. Also
		// add a zero-marker to the end of each list.
		for (i=0; i<PVR_OPB_COUNT; i++) {
			// Not enabled -> skip
			if (!(pvr_state.lists_enabled & (1 << i)))
				continue;

			// Make sure there's at least one primitive in each.
			b = pvr_state.dma_buffers + pvr_state.ram_target;
			if (b->ptr[i] == 0) {
				pvr_blank_polyhdr_buf(i, (pvr_poly_hdr_t*)(b->base[i]));
				b->ptr[i] += 32;
			}

			// Put a zero-marker on the end.
			memset(b->base[i] + b->ptr[i], 0, 32);
			b->ptr[i] += 32;

			// Verify that there is no overrun.
			assert( b->ptr[i] <= b->size[i] );
		}

		// Flip buffers and mark them complete.
		o = irq_disable();
		pvr_state.dma_buffers[pvr_state.ram_target].ready = 1;
		pvr_state.ram_target ^= 1;
		irq_restore(o);

		pvr_sync_stats(PVR_SYNC_BUFDONE);
	} else {
		/* If a list was open, close it */
		if (pvr_state.list_reg_open != -1)
			pvr_list_finish();

		/* If any lists weren't submitted, then submit blank ones now */
		for (i=0; i<PVR_OPB_COUNT; i++) {
			if ( (pvr_state.lists_enabled & (1 << i))
					&& (!( pvr_state.lists_closed & (1 << i) )) ) {
				pvr_list_begin(i);
				pvr_blank_polyhdr(i);
				pvr_list_finish();
			}
		}
	}

	/* Ok, now it's just a matter of waiting for the interrupt... */
	return 0;
}

int pvr_wait_ready() {
	int t;

	assert( pvr_state.valid );

	t = sem_wait_timed(pvr_state.ready_sem, 100);
	if (t < 0) {
		dbglog(DBG_WARNING, "pvr_wait_ready: timed out\n");
		return -1;
	}

	return 0;
}

int pvr_check_ready() {
	assert( pvr_state.valid );

	if (sem_count(pvr_state.ready_sem) > 0)
		return 0;
	else
		return -1;
}




