/* KallistiOS ##version##

   arch/dreamcast/kernel/irq.c
   (c)2000-2001 Dan Potter
*/

/* This module contains low-level handling for IRQs and related exceptions. */

#include <string.h>
#include <stdio.h>
#include <arch/arch.h>
#include <arch/types.h>
#include <arch/irq.h>
#include <arch/timer.h>
#include <kos/dbgio.h>
#include <kos/thread.h>
#include <kos/library.h>

CVSID("$Id: irq.c,v 1.12 2003/02/14 06:33:47 bardtx Exp $");

/* Exception table -- this table matches (EXPEVT>>4) to a function pointer.
   If the pointer is null, then nothing happens. Otherwise, the function will
   handle the exception. */
static irq_handler irq_handlers[0x100];
static irq_handler trapa_handlers[0x100];

/* Global exception handler -- hook this if you want to get each and every
   exception; you might get more than you bargained for, but it can be useful. */
static irq_handler irq_hnd_global;

/* Default IRQ context location */
static irq_context_t irq_context_default;

/* Are we inside an interrupt? */
static int inside_int;
int irq_inside_int() {
	return inside_int;
}

/* Set a handler, or remove a handler */
int irq_set_handler(irq_t code, irq_handler hnd) {
	/* Make sure they don't do something crackheaded */
	if (code >= 0x1000 || (code & 0x000f)) return -1;
	
	code = code >> 4;
	irq_handlers[code] = hnd;
	
	return 0;
}

/* Get the address of the current handler */
irq_handler irq_get_handler(irq_t code) {
	/* Make sure they don't do something crackheaded */
	if (code >= 0x1000 || (code & 0x000f)) return NULL;
	
	code = code >> 4;
	return irq_handlers[code];
}

/* Set a global handler */
int irq_set_global_handler(irq_handler hnd) {
	irq_hnd_global = hnd;
	return 0;
}

/* Get the global exception handler */
irq_handler irq_get_global_handler() {
	return irq_hnd_global;
}

/* Set or remove a trapa handler */
int trapa_set_handler(irq_t code, irq_handler hnd) {
	if (code > 0xff) return -1;

	trapa_handlers[code] = hnd;

	return 0;
}

/* Print a kernel panic reg dump */
extern irq_context_t *irq_srt_addr;
void irq_dump_regs(int code, int evt) {
	uint32 *regs = irq_srt_addr->r;
	dbglog(DBG_DEAD, "Unhandled exception: PC %08lx, code %d, evt %04x\n",
		irq_srt_addr->pc, code, (uint16)evt);
	dbglog(DBG_DEAD, " R0-R7: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
		regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7]);
	dbglog(DBG_DEAD, " R8-R15: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
		regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15]);
	dbglog(DBG_DEAD, " SR %08lx PR %08lx\n", irq_srt_addr->sr, irq_srt_addr->pr);
	arch_stk_trace_at(regs[14], 0);
	/* dbgio_printf(" Vicinity code ");
	dbgio_printf(" @%08lx: %04x %04x %04x %04x %04x\n",
		srt_addr->pc-4, *((uint16*)(srt_addr->pc-4)), *((uint16*)(srt_addr->pc-2)),
		*((uint16*)(srt_addr->pc)), *((uint16*)(srt_addr->pc+2)), *((uint16*)(srt_addr->pc+4))); */
}

/* The C-level routine that processes context switching and other
   types of interrupts. NOTE: We are running on the stack of the process
   that was interrupted! */
volatile uint32 jiffies = 0;
void irq_handle_exception(int code) {
	/* Get the exception code */
	/* volatile uint32 *tra = (uint32*)0xff000020; */
	volatile uint32 *expevt = (uint32*)0xff000024;
	volatile uint32 *intevt = (uint32*)0xff000028;
	uint32 evt = 0;
	int handled = 0;

	/* If it's a code 0, well, we shouldn't be here. */
	if (code == 0) panic("spurious RESET exception");
	
	/* If it's a code 1 or 2, grab the event from expevt. */
	if (code == 1 || code == 2) evt = *expevt;
	
	/* If it's a code 3, grab the event from intevt. */
	if (code == 3) evt = *intevt;

	if (inside_int) {
		irq_handler hnd = irq_handlers[EXC_DOUBLE_FAULT >> 4];
		if (hnd != NULL) {
			hnd(EXC_DOUBLE_FAULT, irq_srt_addr);
		} else
			irq_dump_regs(code, evt);
		thd_pslist(dbgio_printf);
		// library_print_list(dbgio_printf);
		panic("double fault");
	}

	inside_int = 1;

	/* If there's a global handler, call it */
	if (irq_hnd_global) {
		irq_hnd_global(evt, irq_srt_addr);
		handled = 1;
	}

	/* dbgio_printf("got int %04x %04x\n", code, evt); */

	/* If it's a timer interrupt, clear the status */
	if (evt >= EXC_TMU0_TUNI0 && evt <= EXC_TMU2_TUNI2) {
		if (evt == EXC_TMU0_TUNI0) {
			timer_clear(TMU0);
		} else if (evt == EXC_TMU1_TUNI1) {
			timer_clear(TMU1);
		} else {
			timer_clear(TMU2);
		}
		handled = 1;
	}

	/* If there's a handler, call it */
	{
		irq_handler hnd = irq_handlers[evt >> 4];
		if (hnd != NULL) {
			hnd(evt, irq_srt_addr);
			handled = 1;
		}
	}

	if (!handled) {
		irq_handler hnd = irq_handlers[EXC_UNHANDLED_EXC >> 4];
		if (hnd != NULL) {
			hnd(evt, irq_srt_addr);
		} else
			irq_dump_regs(code, evt);
		panic("unhandled IRQ/Exception");
	}

	/* dbgio_printf("returning from int\n"); */

	irq_disable();
	inside_int = 0;
}

void irq_handle_trapa(irq_t code, irq_context_t *context) {
	/* Get the exception code */
	volatile uint32 *tra = (uint32*)0xff000020;
	irq_handler hnd;
	uint32 vec;

	/* Get the trapa vector */
	vec = (*tra) >> 2;
	
	/* Check for handler and call if present */
	hnd = trapa_handlers[vec];
	if (hnd != NULL)
		hnd(vec, context);
}


extern void irq_vma_table();

/* Switches register banks; call this outside of exception handling
   (but make sure interrupts are off!!) to change where registers will
   go to, or call it inside an exception handler to switch contexts. 
   Make sure you have at least REG_BYTE_CNT bytes available. DO NOT
   ALLOW ANY INTERRUPTS TO HAPPEN UNTIL THIS HAS BEEN CALLED AT 
   LEAST ONCE! */
void irq_set_context(irq_context_t *regbank) {
	irq_srt_addr = regbank;
}

/* Return the current IRQ context */
irq_context_t *irq_get_context() {
	return irq_srt_addr;
}

/* Fill a newly allocated context block for usage with supervisor/kernel
   or user mode. The given parameters will be passed to the called routine (up
   to the architecture maximum). */   
void irq_create_context(irq_context_t *context, uint32 stkpntr,
		uint32 routine, uint32 *args, int usermode) {
	int i;

	/* Clear out user and FP regs */
	for (i=0; i<16; i++) {
		context->r[i] = 0;
		context->fr[i] = 0;
		context->frbank[i] = 0;
	}

	/* Set misc system regs */
	context->gbr = context->mach = context->macl = 0;
	context->vbr = 0;	/* This is not relevant because the
				   context switcher doesn't touch it */

	/* Set default floating point control regs */
	context->fpscr = 0;
	context->fpul = 0;

	/* Setup the program frame */
	context->pc = (uint32)routine;
	context->pr = 0;
	context->sr = 0x40000000;	/* note: need to handle IMASK */
	context->r[15] = stkpntr;
	context->r[14] = 0xffffffff;

	/* Copy up to four args */
	context->r[4] = args[0];
	context->r[5] = args[1];
	context->r[6] = args[2];
	context->r[7] = args[3];

	/* Handle user mode */
	if (usermode) {
		context->sr &= ~0x40000000;
		context->r[15] &= ~0xf0000000;
	}
}

/* Default timer handler (until threads can take over) */
static void irq_def_timer(irq_t src, irq_context_t *context) { }

/* Default FPU exception handler (can't seem to turn these off) */
static void irq_def_fpu(irq_t src, irq_context_t *context) {
	context->pc += 2;
}

/* Pre-init SR and VBR */
static uint32 pre_sr, pre_vbr;

/* Have we been initialized? */
static int initted = 0;

/* Init routine */
int irq_init() {
	/* Save SR and VBR */
	asm("stc	sr,r0\n"
	    "mov.l	r0,%0" : : "m"(pre_sr));
	asm("stc	vbr,r0\n"
	    "mov.l	r0,%0" : : "m"(pre_vbr));

	/* Make sure interrupts are disabled */
	irq_disable();

	/* Blank the exception handler tables */
	memset(irq_handlers, 0, sizeof(irq_handlers));
	memset(trapa_handlers, 0, sizeof(trapa_handlers));
	irq_hnd_global = NULL;

	/* Default to not in an interrupt */
	inside_int = 0;

	/* Set a default timer handler */
	irq_set_handler(TIMER_IRQ, irq_def_timer);

	/* Set a trapa handler */
	irq_set_handler(EXC_TRAPA, irq_handle_trapa);

	/* Set a default FPU exception handler */
	irq_set_handler(EXC_FPU, irq_def_fpu);

	/* Set a default context (will be superceded if threads are
	   enabled later) */
	irq_set_context(&irq_context_default);

	/* Set VBR to our exception table above, but don't enable
	   exceptions and IRQs yet. */
	asm("	! Set VBR\n"
	    "	mov.l	_vbr_addr,r0\n"
	    "	ldc	r0,vbr\n"
	    "	bra	_after_vbr\n"
	    "	nop\n"
	    "	.align 2\n"
	"_vbr_addr:\n"
	    "	.long	_irq_vma_table\n"
	"_after_vbr:\n");

	initted = 1;
	
	return 0;
}


void irq_shutdown() {
	if (!initted)
		return;
		
	/* Restore SR and VBR */
	asm("mov.l	%0,r0\n"
	    "ldc	r0,sr" : : "m"(pre_sr));
	asm("mov.l	%0,r0\n"
	    "ldc	r0,vbr" : : "m"(pre_vbr));

}





