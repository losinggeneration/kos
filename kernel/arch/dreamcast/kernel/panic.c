/* KallistiOS ##version##

   panic.c
   (c)2001 Dan Potter
*/

#include <stdio.h>
#include <malloc.h>
#include <arch/arch.h>

CVSID("$Id: panic.c,v 1.3 2002/02/18 02:59:20 bardtx Exp $");

/* If something goes badly wrong in the kernel and you don't think you
   can recover, call this. This is a pretty standard tactic from *nixy
   kernels which ought to be avoided if at all possible. */
void panic(const char *msg) {
	printf("kernel panic: %s\r\n", msg);
	arch_abort();
}

