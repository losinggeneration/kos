/* KallistiOS ##version##

   pvrmap.c
   (c)2002 Dan Potter
*/

#include <kos.h>

/* This sample doesn't particularly do anything helpful, but it shows off
   the basic usage of the built-in page table functions for remapping
   regions of memory. That main drawing code should certainly pull
   a double-take from any good coder ;) */

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);

int main(int argc, char **argv) {
	mmucontext_t * cxt;
	uint16 * vr;
	int x, y, done = 0;
	
	/* Initialize MMU support */
	mmu_init();

	/* Setup a context */
	cxt = mmu_context_create(0);
	mmu_use_table(cxt);
	mmu_switch_context(cxt);

	/* Map the PVR video memory to 0 */
	mmu_page_map(cxt, 0, 0x05000000 >> PAGESIZE_BITS, (8*1024*1024) >> PAGESIZE_BITS,
		MMU_ALL_RDWR, MMU_NO_CACHE, MMU_NOT_SHARED, MMU_DIRTY);

	/* Draw a nice pattern to the NULL space */
	vr = NULL;
	for (y=0; y<480; y++) {
		for (x=0; x<640; x++) {
			int v = ((x*x+y*y) & 255);
			if (v >= 128)
				v = 127 - (v-128);
			vr[y*640+x] = ((v >> 3) << 11)
				| ((v >> 2) << 5)
				| ((v >> 3) << 0);
		}
	}

	/* Draw some text */
	bfont_draw_str(vr + 20*640+20, 640, 0, "Press START!");

	/* Wait for start */
	while (!done) {
		MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
			if (st->buttons & CONT_START)
				done = 1;
		MAPLE_FOREACH_END()
	}

	/* Destroy the context */
	mmu_context_destroy(cxt);

	/* Shutdown MMU support */
	mmu_shutdown();

	return 0;
}


