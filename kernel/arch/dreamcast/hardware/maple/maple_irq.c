/* KallistiOS ##version##

   maple_irq.c
   (c)2002 Dan Potter
 */

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <dc/maple.h>
#include <dc/asic.h>
#include <kos/thread.h>

CVSID("$Id: maple_irq.c,v 1.2 2002/10/10 06:39:47 bardtx Exp $");

/*********************************************************************/
/* VBlank IRQ handler */

/* Fwd declare */
static void vbl_autodet_callback(maple_frame_t *);

/* Send a DEVINFO command for the given port/unit */
static void vbl_send_devinfo(int p, int u) {
	maple_device_t * dev;

	dev = &maple_state.ports[p].units[u];

	/* Reserve access; if we don't get it, forget about it */
	if (maple_frame_lock(&dev->frame) < 0)
		return;

	/* Setup our autodetect frame to probe at a new device */
	maple_frame_init(&dev->frame);
	dev->frame.cmd = MAPLE_COMMAND_DEVINFO;
	dev->frame.dst_port = p;
	dev->frame.dst_unit = u;
	dev->frame.callback = vbl_autodet_callback;
	maple_queue_frame(&dev->frame);
}

/* Do a potential disconnect on the named device (check to make sure it
   was connected first) */
static void vbl_chk_disconnect(int p, int u) {
	if (maple_state.ports[p].units[u].valid) {
#if MAPLE_IRQ_DEBUG
		dbglog(DBG_KDEBUG, "maple: detach on device %c%c\n",
			'A'+p, '0'+u);
#endif
		if (maple_driver_detach(p, u) >= 0) {
			assert(!maple_state.ports[p].units[u].valid);
		}
	}
}

/* Check the sub-devices for a top-level port */
static void vbl_chk_subdevs(int p, uint8 newmask) {
	int oldmask, chkmask, u;

	/* Get the old mask */
	oldmask = maple_state.ports[p].units[0].dev_mask;

	/* Is it different from the new mask? */
	if (oldmask != newmask) {
		/* Yep -- go through and check the status of each sub-dev */
		for (u=1; u<MAPLE_UNIT_COUNT; u++) {
			chkmask = 1 << (u-1);

			/* Wasn't set but is set now == newly attached device. Send it a
			   sub-query. Was set but isn't set now == newly detached
			   device. Do a driver detach on it. */
			if (!(oldmask & chkmask) && (newmask & chkmask)) {
				/* Send a further query */
				vbl_send_devinfo(p, u);
			} else if ((oldmask & chkmask) && !(newmask & chkmask)) {
				/* Do a disconnect */
				vbl_chk_disconnect(p, u);
			}
		}

		/* Update with the new sub-dev mask */
		maple_state.ports[p].units[0].dev_mask = newmask;
	}
}

/* Handles autodetection of hotswapping; basically every periodic
   interrupt, we choose a new port/unit pair and probe it for 
   device info. If the same thing is plugged in there, we assume
   that nothing has changed. Otherwise, some device (un/re)binding has
   to occur.

   Note that we could actually set this up as a maple device driver,
   but that might complicate things if we're swapping device structures
   around in the middle of a list traversal, so we do it here as a 
   special case instead. */
static void vbl_autodet_callback(maple_frame_t * frm) {
	maple_response_t	*resp;
	int			p, u;

	/* So.. did we get a response? */
	resp = (maple_response_t *)frm->recv_buf;
	p = frm->dst_port; u = frm->dst_unit;

	if (resp->response == MAPLE_RESPONSE_NONE) {
		/* No device, or not functioning properly; check for removal */
		if (u == 0) {
			/* Top-level device -- detach all sub-devices as well */
			for (u=0; u<MAPLE_UNIT_COUNT; u++) {
				vbl_chk_disconnect(p, u);
			}
			maple_state.ports[p].units[0].dev_mask = 0;
		} else {
			/* Not a top-level device -- only detach this device */
			vbl_chk_disconnect(p, u);
		}
	} else if (resp->response == MAPLE_RESPONSE_DEVINFO) {
		/* Device is present, check for connections */
		if (!maple_state.ports[p].units[u].valid) {
#if MAPLE_IRQ_DEBUG
			dbglog(DBG_KDEBUG, "maple: attach on device %c%c\n",
				'A'+p, '0'+u);
#endif
			if (maple_driver_attach(frm) >= 0) {
				assert(maple_state.ports[p].units[u].valid);
			}
		}

		/* If this is a top-level port, then also check any
		   sub-devices that claim to be attached */
		if (u == 0)
			vbl_chk_subdevs(p, resp->src_addr);
	} else {
		/* dbglog(DBG_KDEBUG, "maple: unknown response %d on device %c%c\n",
			resp->response, 'A'+p, '0'+u); */
	}

	maple_frame_unlock(frm);
}

/* Move on to the next device for next time */
static void vbl_ad_advance() {
	maple_state.detect_port_next++;
	if (maple_state.detect_port_next >= MAPLE_PORT_COUNT) {
		maple_state.detect_port_next = 0;
		maple_state.detect_wrapped++;
	}
}

static void vbl_autodetect() {
	int p, u;

	/* Queue a detection on the next device */
	p = maple_state.detect_port_next;
	u = maple_state.detect_unit_next;
	vbl_send_devinfo(p, u);

	/* Move to the next device */
	vbl_ad_advance();
}

/* Called on every VBL (~60fps) */
void maple_vbl_irq_hnd(uint32 code) {
	maple_driver_t *drv;

	/* dbgio_write_str("inside vbl_irq_hnd\n"); */

	/* Count, for fun and profit */
	maple_state.vbl_cntr++;

	/* Autodetect changed devices */
	vbl_autodetect();

	/* Call all registered drivers' periodic callbacks */
	LIST_FOREACH(drv, &maple_state.driver_list, drv_list) {
		if (drv->periodic != NULL)
			drv->periodic(drv);
	}

	/* Send any queued data */
	if (!maple_state.dma_in_progress)
		maple_queue_flush();

	/* dbgio_write_str("finish vbl_irq_hnd\n"); */
}

/*********************************************************************/
/* Maple DMA completion handler */

/* Called after a Maple DMA send / receive pair completes */
void maple_dma_irq_hnd(uint32 code) {
	maple_frame_t	*i;
	int8		resp;

	/* dbgio_write_str("start dma_irq_hnd\n"); */

	/* Count, for fun and profit */
	maple_state.dma_cntr++;

	/* ACK the receipt */
	maple_state.dma_in_progress = 0;

#if MAPLE_DMA_DEBUG
	maple_sentinel_verify("maple_state.dma_buffer", maple_state.dma_buffer, MAPLE_DMA_SIZE);
#endif

	/* For each queued frame, call its callback if it's done */
	TAILQ_FOREACH(i, &maple_state.frame_queue, frameq) {
		/* Skip any unsent or stale items */
		if (i->state != MAPLE_FRAME_SENT)
			continue;

		/* Check to see if it got a proper response; we might
		   have to resubmit the request */
		resp = ((int8*)i->recv_buf)[0];
		if (resp == MAPLE_RESPONSE_AGAIN) {
			i->state = MAPLE_FRAME_UNSENT;
			continue;
		}

#if MAPLE_DMA_DEBUG
		maple_sentinel_verify("i->recv_buf", i->recv_buf, 1024);
#endif

		/* Mark it as responded to */
		i->state = MAPLE_FRAME_RESPONDED;

		/* It's safe to do this during a TAILQ_FOREACH (verified) but
		   this isn't a good practice for non-TAILQ types =) */
		maple_queue_remove(i);

		/* If it's got a callback, call it; otherwise unlock
		   it manually (or it'll never get used again) */
		if (i->callback != NULL)
			i->callback(i);
		else
			maple_frame_unlock(i);
	}

	/* dbgio_write_str("finish dma_irq_hnd\n"); */
}

