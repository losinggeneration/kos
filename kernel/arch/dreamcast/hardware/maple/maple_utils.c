/* KallistiOS ##version##

   maple_util.c
   (c)2002 Dan Potter
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dc/maple.h>

CVSID("$Id: maple_utils.c,v 1.1 2002/02/22 07:34:20 bardtx Exp $");

/* Enable / Disable the bus */
void maple_bus_enable() {
	maple_write(MAPLE_ENABLE, MAPLE_ENABLE_ENABLED);
}
void maple_bus_disable() {
	maple_write(MAPLE_ENABLE, MAPLE_ENABLE_DISABLED);
}

/* Start / Stop DMA */
void maple_dma_start() {
	maple_write(MAPLE_STATE, MAPLE_STATE_DMA);
}
void maple_dma_stop() {
	maple_write(MAPLE_STATE, MAPLE_STATE_IDLE);
}

int maple_dma_in_progress() {
	return maple_read(MAPLE_STATE) & MAPLE_STATE_DMA;
}

/* Set the DMA Address */
void maple_dma_addr(void *ptr) {
	maple_write(MAPLE_DMAADDR, ((uint32) ptr) & 0x1fffffff);
}

/* Return a "maple address" for a port,unit pair */
uint8 maple_addr(int port, int unit) {
	uint8 addr;

	assert(port < MAPLE_PORT_COUNT && unit < MAPLE_UNIT_COUNT);

	addr = port << 6;
	if (unit != 0)
		addr |= (1 << (unit - 1)) & 0x1f;
	else
		addr |= 0x20;

	return addr;
}

/* Decompose a "maple address" into a port,unit pair */
/* WARNING: Won't work on multi-cast addresses! */
void maple_raddr(uint8 addr, int * port, int * unit) {
	*port = (addr >> 6) & 3;
	if (addr & 0x20)
		*unit = 0;
	else if (addr & 0x10)
		*unit = 5;
	else if (addr & 0x08)
		*unit = 4;
	else if (addr & 0x04)
		*unit = 3;
	else if (addr & 0x02)
		*unit = 2;
	else if (addr & 0x01)
		*unit = 1;
	else {
		dbglog(DBG_ERROR, "maple_raddr: invalid address %02x\n", addr);
		*port = -1;
		*unit = -1;
		assert_msg(0, "invalid unit id");
	}
}

/* Strings for maple device capabilities */
static const char *maple_cap_names[] = {
	"LightGun",
	"Keyboard",
	"Argun",
	"Microphone",
	"Clock",
	"LCD",
	"MemoryCard",
	"Controller",
	NULL, NULL, NULL, NULL,
	NULL, NULL,
	"Mouse",
	"JumpPack"
};
#define maple_cap_name_cnt (sizeof(maple_cap_names)/sizeof(char *))

/* Print the capabilities of a given driver to dbglog; NOT THREAD SAFE */
static char caps_buffer[1024];
const char * maple_pcaps(uint32 functions) {
	int i, o;

	for (o=0, i=0; i<32; i++) {
		if (functions & (0x80000000 >> i)) {
			if (i > maple_cap_name_cnt || maple_cap_names[i] == NULL) {
				sprintf(caps_buffer+o, "UNKNOWN(%08x), ", (0x80000000 >> i));
				o += strlen(caps_buffer + o);
			} else {
				sprintf(caps_buffer+o, "%s, ", maple_cap_names[i]);
				o += strlen(caps_buffer + o);
			}
		}
	}
	if (o > 0) {
		o -= 2;
		caps_buffer[o] = '\0';
	}

	return caps_buffer;
}

static const char *maple_resp_names[] = {
	"EFILEERR",
	"EAGAIN",
	"EBADCMD",
	"EBADFUNC",
	"ENONE",
	NULL, NULL, NULL, NULL, NULL,
	"DEVINFO",
	"ALLINFO",
	"OK",
	"DATATRF"
};
#define maple_resp_name_cnt (sizeof(maple_resp_names)/sizeof(char *))

/* Return a string representing the maple response code */
const char * maple_perror(int response) {
	response += 5;
	if (response < 0 || response >= maple_resp_name_cnt)
		return "UNKNOWN";
	else
		return maple_resp_names[response];
}

/* Determine if a given device is valid */
int maple_dev_valid(int p, int u) {
	return maple_state.ports[p].units[u].valid;
}

#if MAPLE_DMA_DEBUG
/* Debugging help */
void maple_sentinel_setup(void * buffer, int bufsize) {
	assert( bufsize % 4 == 0 );
	memset4(buffer, 0xdeadbeef, bufsize);
}

void maple_sentinel_verify(const char * bufname, void * buffer, int bufsize) {
	int i;
	uint32 *b32;
	
	assert( bufsize % 4 == 0 );

	b32 = ((uint32 *)buffer) - 512/4;
	for (i=0; i<512/4; i++) {
		if (b32[i] != 0xdeadbeef) {
			dbglog(DBG_ERROR, "*** BUFFER CHECK FAILURE *** %s failed at pre-offset %d\n",
				bufname, i);
			assert( 0 );
		}
	}

	b32 = ((uint32 *)buffer) + bufsize/4;
	for (i=0; i<512/4; i++) {
		if (b32[i] != 0xdeadbeef) {
			dbglog(DBG_ERROR, "*** BUFFER CHECK FAILURE *** %s failed at post-offset %d\n",
				bufname, i);
			assert( 0 );
		}
	}
}
#endif
