/* This sample program shows off 800x608, the mythical "unsupported" video
   mode that Sega will probably complain doesn't work (like they did for
   our 320x240 modes earlier on ;-). This will probably only work on a VGA
   monitor and I highly recommend that you don't try it on a non-multisync.
   Still needs some tweaking.

   In here I also demonstrate how to use the bfont BIOS font routines, and
   how to initialize only selected parts of KOS (resulting in a much 
   smaller and less polluted binary).

 */

#include <kos.h>

#define W 800
#define H 608

int main(int argc, char **argv) {
	int x, y;

	/* Set video mode */
	vid_set_mode(DM_800x608, PM_RGB565);
	
	for (y=0; y<H; y++)
		for (x=0; x<W; x++) {
			int c = (x ^ y) & 255;
			vram_s[y*W+x] = ((c >> 3) << 0);
		}

	for (y=0; y<H; y+=24) {
		char tmp[16];
		sprintf(tmp, "%d", y);
		bfont_draw_str(vram_s+y*W+10, W, 0, tmp);
	}

	for (x=0; x<W; x+=100) {
		char tmp[16];
		sprintf(tmp, "%d", x/10);
		bfont_draw_str(vram_s+10*W+x, W, 0, tmp);
	}

	/* Pause to see the results */
	usleep(5*1000*1000);

	return 0;
}
