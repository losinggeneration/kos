/* KallistiOS ##version##

   purupuru.c
   Copyright (C)2003 Dan Potter
*/

#include <dc/maple.h>
#include <dc/maple/purupuru.h>

/* We actually don't have any decent specs on the purupuru (vibration)
   pack right now, so this module doesn't do much besides make the
   detection work. */

static void purupuru_periodic(maple_driver_t *drv) {
}

static int purupuru_attach(maple_driver_t *drv, maple_device_t *dev) {
	dev->status_valid = 1;
	return 0;
}

static void purupuru_detach(maple_driver_t *drv, maple_device_t *dev) {
}

/* Device Driver Struct */
static maple_driver_t purupuru_drv = {
	functions:	MAPLE_FUNC_PURUPURU,
	name:		"PuruPuru (Vibration) Pack",
	periodic:	purupuru_periodic,
	attach:		purupuru_attach,
	detach:		purupuru_detach
};

/* Add the mouse to the driver chain */
int purupuru_init() {
	return maple_driver_reg(&purupuru_drv);
}

void purupuru_shutdown() {
	maple_driver_unreg(&purupuru_drv);
}

