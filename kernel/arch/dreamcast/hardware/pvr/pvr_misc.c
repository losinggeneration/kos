/* KallistiOS ##version##

   pvr_misc.c
   (C)2002 Dan Potter

 */

#include <assert.h>
#include <string.h>
#include <arch/timer.h>
#include <dc/pvr.h>
#include <dc/video.h>
#include "pvr_internal.h"

/*
   These are miscellaneous parameters you can set which affect the
   rendering process.
*/

CVSID("$Id: pvr_misc.c,v 1.12 2003/04/24 03:12:26 bardtx Exp $");

/* Set the background plane color (the area of the screen not covered by
   any other polygons) */
void pvr_set_bg_color(float r, float g, float b) {
	int ir, ig, ib;

	ir = (int)(255 * r);
	ig = (int)(255 * g);
	ib = (int)(255 * b);

	pvr_state.bg_color = (ir << 16) | (ig << 8) | (ib << 0);
}

/* Return the current VBlank count */
int pvr_get_vbl_count() {
	return pvr_state.vbl_count;
}

/* Fill in a statistics structure (above) from current data. This
   is a super-set of frame count. */                        
int pvr_get_stats(pvr_stats_t *stat) {
	if (!pvr_state.valid)
		return -1;

	assert(stat != NULL);

	stat->enabled_list_mask = pvr_state.lists_enabled;
	stat->vbl_count = pvr_state.vbl_count;
	stat->frame_last_time = pvr_state.frame_last_len;
	stat->reg_last_time = pvr_state.reg_last_len;
	stat->rnd_last_time = pvr_state.rnd_last_len;
	if (stat->frame_last_time != 0)
		stat->frame_rate = 1000.0f / stat->frame_last_time;
	else
		stat->frame_rate = -1.0f;
	stat->vtx_buffer_used = pvr_state.vtx_buf_used;
	stat->vtx_buffer_used_max = pvr_state.vtx_buf_used_max;
	stat->buf_last_time = pvr_state.buf_last_len;
	stat->frame_count = pvr_state.frame_count;

	return 0;
}

/******** INTERNAL STUFF ************************************************/

/* Update statistical counters */
void pvr_sync_stats(int event) {
	uint64	t;
	volatile pvr_ta_buffers_t *buf;


	/* Get the current time */
	t = timer_ms_gettime64();

	switch (event) {
	case PVR_SYNC_VBLANK:
		pvr_state.vbl_count++;
		break;

	case PVR_SYNC_REGSTART:
		pvr_state.reg_start_time = t;
		break;

	case PVR_SYNC_REGDONE:
		pvr_state.reg_last_len = (uint32)(t - pvr_state.reg_start_time);

		buf = pvr_state.ta_buffers + pvr_state.ta_target;
		pvr_state.vtx_buf_used = PVR_GET(PVR_TA_VERTBUF_POS) - buf->vertex;
		if (pvr_state.vtx_buf_used > pvr_state.vtx_buf_used_max)
			pvr_state.vtx_buf_used_max = pvr_state.vtx_buf_used;
		break;

	case PVR_SYNC_RNDSTART:
		pvr_state.rnd_start_time = t;
		break;

	case PVR_SYNC_RNDDONE:
		pvr_state.rnd_last_len = (uint32)(t - pvr_state.rnd_start_time);
		break;

	case PVR_SYNC_BUFSTART:
		pvr_state.buf_start_time = t;
		break;

	case PVR_SYNC_BUFDONE:
		pvr_state.buf_last_len = (uint32)(t - pvr_state.buf_start_time);
		break;

	case PVR_SYNC_PAGEFLIP:
		pvr_state.frame_last_len = (uint32)(t - pvr_state.frame_last_time);
		pvr_state.frame_last_time = t;
		pvr_state.frame_count++;
		break;

	}
}

/* Synchronize the viewed page with what's in pvr_state */
void pvr_sync_view() {
	vid_set_start(pvr_state.frame_buffers[pvr_state.view_target].frame);
}

/* Synchronize the registration buffer with what's in pvr_state */
void pvr_sync_reg_buffer() {
	volatile pvr_ta_buffers_t *buf;

	buf = pvr_state.ta_buffers + pvr_state.ta_target;

	/* Reset TA */
	//PVR_SET(PVR_RESET, PVR_RESET_TA);
	//PVR_SET(PVR_RESET, PVR_RESET_NONE);

	/* Set buffer pointers */
	PVR_SET(PVR_TA_OPB_START,	buf->opb);
	PVR_SET(PVR_TA_OPB_END,		buf->opb - buf->opb_size);
	PVR_SET(PVR_TA_VERTBUF_START,	buf->vertex);
	PVR_SET(PVR_TA_VERTBUF_END,	buf->vertex + buf->vertex_size);
	PVR_SET(PVR_TA_OPB_INIT,	buf->opb);

	/* Misc config parameters */
	PVR_SET(PVR_TILEMAT_CFG,	pvr_state.tsize_const);		/* Tile count: (H/32-1) << 16 | (W/32-1) */
	PVR_SET(PVR_OPB_CFG,		pvr_state.list_reg_mask);	/* List enables */
	PVR_SET(PVR_TA_INIT,		PVR_TA_INIT_GO);		/* Confirm settings */
	(void)PVR_GET(PVR_TA_INIT);
}

/* Begin a render operation that has been queued completely (i.e., the
   opposite of ta_target) */
void pvr_begin_queued_render() {
	volatile pvr_ta_buffers_t	* tbuf;
	volatile pvr_frame_buffers_t	* rbuf;
	pvr_bkg_poly_t	bkg;
	uint32		*vrl, *bkgdata;
	uint32		vert_end;
	int		i;

	/* Get the appropriate buffer */
	tbuf = pvr_state.ta_buffers + (pvr_state.ta_target ^ 1);
	rbuf = pvr_state.frame_buffers + (pvr_state.view_target ^ 1);

	/* Calculate background value for below */
	/* Small side note: during setup, the value is originally
	   0x01203000... I'm thinking that the upper word signifies
	   the length of the background plane list in dwords
	   shifted up by 4. */
	vert_end = 0x01000000 | ((PVR_GET(PVR_TA_VERTBUF_POS) - tbuf->vertex) << 1);
	
	/* Throw the background data on the end of the TA's list */
	bkg.flags1 = 0x90800000;	/* These are from libdream.. ought to figure out */
	bkg.flags2 = 0x20800440;	/*   what they mean for sure... heh =) */
	bkg.dummy = 0;
	bkg.x1 =   0.0f; bkg.y1 = 480.0f; bkg.z1 = 0.2f; bkg.argb1 = pvr_state.bg_color;
	bkg.x2 =   0.0f; bkg.y2 =   0.0f; bkg.z2 = 0.2f; bkg.argb2 = pvr_state.bg_color;
	bkg.x3 = 640.0f; bkg.y3 = 480.0f; bkg.z3 = 0.2f; bkg.argb3 = pvr_state.bg_color;
	bkgdata = (uint32 *)&bkg;
	vrl = (uint32*)(PVR_RAM_BASE | PVR_GET(PVR_TA_VERTBUF_POS));
	for (i=0; i<0x10; i++)
		vrl[i] = bkgdata[i];
	vrl[0x11] = 0;

	/* Reset the ISP/TSP, just in case */
	//PVR_SET(PVR_RESET, PVR_RESET_ISPTSP);
	//PVR_SET(PVR_RESET, PVR_RESET_NONE);

	/* Finish up rendering the current frame (into the other buffer) */
	PVR_SET(PVR_ISP_TILEMAT_ADDR, tbuf->tile_matrix);
	PVR_SET(PVR_ISP_VERTBUF_ADDR, tbuf->vertex);
	//if (!to_texture)
		PVR_SET(PVR_RENDER_ADDR, rbuf->frame);
	/* else {
		SETREG(0x8060, to_txr_addr | (1<<24));
		SETREG(0x8064, to_txr_addr | (1<<24));
	} */
	PVR_SET(PVR_BGPLANE_CFG, vert_end);	/* Bkg plane location */
	PVR_SET(PVR_BGPLANE_Z, *((uint32*)&pvr_state.zclip));
	PVR_SET(PVR_PCLIP_X, pvr_state.pclip_x);
	PVR_SET(PVR_PCLIP_Y, pvr_state.pclip_y);
	//if (!to_texture)
		PVR_SET(PVR_RENDER_MODULO, (pvr_state.w * 2) / 8);
	/* else
		SETREG(0x804c, to_txr_rp); */
	// XXX Do we _really_ need this every time?
	// SETREG(PVR_FB_CFG_2, 0x00000009);		/* Alpha mode */
	PVR_SET(PVR_ISP_START, PVR_ISP_START_GO);	/* Start render */
}

void pvr_blank_polyhdr(int type) {
	pvr_poly_hdr_t poly;

	// Make it.
	pvr_blank_polyhdr_buf(type, &poly);

	// Submit it
	pvr_prim(&poly, sizeof(poly));
}

void pvr_blank_polyhdr_buf(int type, pvr_poly_hdr_t * poly) {
	/* Empty it out */
	memset(poly, 0, sizeof(pvr_poly_hdr_t));

	/* Put in the list type */
	poly->cmd = (type << PVR_TA_CMD_TYPE_SHIFT) | 0x80840012;

	/* Fill in dummy values */
	poly->d1 = poly->d2 = poly->d3 = poly->d4 = 0xffffffff;

}

