/* KallistiOS ##version##

   arch/dreamcast/kernel/mmu.c
   (c)2001 Dan Potter
*/

/* SH-4 MMU related functions, ported up from KOS-MMU */

#include <string.h>
#include <malloc.h>

#include <kos/thread.h>
#include <arch/arch.h>
#include <arch/types.h>
#include <arch/irq.h>
#include <arch/syscall.h>
#include <arch/mmu.h>
#include <arch/dbgio.h>
#include <arch/cache.h>

CVSID("$Id: mmu.c,v 1.3 2002/10/08 07:46:20 bardtx Exp $");

/********************************************************************************/
/* Register definitions */

static volatile uint32 * const pteh = (uint32 *)(0xff000000);
static volatile uint32 * const ptel = (uint32 *)(0xff000004);
static volatile uint32 * const ptea = (uint32 *)(0xff000034);
static volatile uint32 * const ttb = (uint32 *)(0xff000008);
static volatile uint32 * const tea = (uint32 *)(0xff00000c);
static volatile uint32 * const mmucr = (uint32 *)(0xff000010);

#define SET_PTEH(VA, ASID) \
	do { *pteh = ((VA) & 0xfffffc00) | ((ASID) & 0xff); } while(0)

#define SET_PTEL(PA, V, SZ, PR, C, D, SH, WT) \
	do { *ptel = ((PA) & 0x1ffffc00) | ((V) << 8) \
		| ( ((SZ) & 2) << 6 ) | ( ((SZ) & 1) << 4 ) \
		| ( (PR) << 5 ) \
		| ( (C) << 3 ) \
		| ( (D) << 2 ) \
		| ( (SH) << 1 ) \
		| ( (WT) << 0 ); } while(0)

#define SET_TTB(TTB) \
	do { *ttb = TTB; } while(0)

#define SET_MMUCR(URB, URC, SQMD, SV, TI, AT) \
	do { *mmucr = ((URB) << 18) \
		| ((URC) << 10) \
		| ((SQMD) << 9) \
		| ((SV) << 8) \
		| ((TI) << 2) \
		| ((AT) << 0); } while(0)

#define SET_URC(URC) \
	do { *mmucr = (*mmucr & ~(63 << 10)) \
		| (((URC) & 63) << 10); } while(0)

#define GET_URC() ((*mmucr >> 10) & 63)

#define INCR_URC() \
	do { SET_URC(GET_URC() + 1); } while(0)

/********************************************************************************/

/* "Current" page tables (for TLB exception handling) */
mmucontext_t *mmu_cxt_current;

/********************************************************************************/
/* Physical hardware management */

static inline void mmu_ldtlb(int asid, uint32 virt, uint32 phys, int sz, int pr, int c, int d,
		int sh, int wt) {
	SET_PTEH(virt, asid);
	SET_PTEL(phys, 1, sz, pr, c, d, sh, wt);
	asm("ldtlb");
}
static inline void mmu_ldtlb_wait() {
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
}

/* Defined in mmuitlb.s */
void mmu_reset_itlb();

/********************************************************************************/
/* Table management */

/* Set the "current" page tables for TLB handling */
void mmu_use_table(mmucontext_t *context) {
	mmu_cxt_current = context;
}

/* Allocate a page table shell; no actual sub-contexts will be allocated
   until a mapping is performed. */
mmucontext_t *mmu_context_create(int asid) {
	mmucontext_t	*cont;
	int		i;

	cont = (mmucontext_t*)malloc(sizeof(mmucontext_t));
	cont->asid = asid;
	for (i=0; i<MMU_PAGES; i++)
		cont->sub[i] = NULL;

	return cont;
}

/* Destroy an MMU context when a process is being destroyed. */
void mmu_context_destroy(mmucontext_t *context) {
	int i;

	for (i=0; i<MMU_PAGES; i++) {
		if (context->sub[i] != NULL)
			free(context->sub[i]);
	}
	free(context);
}

/* Using the given page tables, return a pointer to the page entry
   matching the given virtual page ID, or return NULL if there
   isn't one. */
static mmupage_t *map_virt(mmucontext_t *context, int virtpage) {
	mmusubcontext_t	*sub;
	mmupage_t	*page;
	int		top, bot;
	
	/* Get back the virtual address */
	virtpage = virtpage << MMU_IND_BITS;

	/* Mask out and grab the top and bottom indeces */
	top = (virtpage >> MMU_TOP_SHIFT) & MMU_TOP_MASK;
	bot = (virtpage >> MMU_BOT_SHIFT) & MMU_BOT_MASK;

	/* Look up the top-level sub-context */
	sub = context->sub[top];
	if (sub == NULL)
		return NULL;

	/* Look up the bottom-level page */
	page = sub->page + bot;
	if (!page->valid)
		return NULL;

	/* Return the physical page number */
	return page;
}

/* Using the given page tables, translate the virtual page ID to a 
   physical page ID. Return -1 on failure. */
int mmu_virt_to_phys(mmucontext_t *context, int virtpage) {
	mmupage_t	*page;

	page = map_virt(context, virtpage);
	if (!page)
		return -1;
	else
		return page->physical;
}

/* Switch to the given context; invalidate any caches as neccessary */
void mmu_switch_context(mmucontext_t *context) {
	SET_PTEH(0, context->asid);
}

/* Set the given virtual page to map to the given physical page; implies
   turning on the "valid" bit. */
static void mmu_page_map_single(mmucontext_t *context,
		int virtpage, int physpage,
		int prot, int cache, int share, int dirty) {
	mmusubcontext_t	*sub;
	mmupage_t	*page;
	int		top, bot, i;
	
	/* Get back the virtual address */
	virtpage = virtpage << MMU_IND_BITS;

	/* Mask out and grab the top and bottom indeces */
	top = (virtpage >> MMU_TOP_SHIFT) & MMU_TOP_MASK;
	bot = (virtpage >> MMU_BOT_SHIFT) & MMU_BOT_MASK;

	/* Look up the top-level sub-context; if there isn't one, create one. */
	sub = context->sub[top];
	if (sub == NULL) {
		sub = (mmusubcontext_t *)malloc(sizeof(mmusubcontext_t));
		for (i=0; i<MMU_SUB_PAGES; i++)
			sub->page[i].valid = 0;
		context->sub[top] = sub;
	}

	/* Look up the bottom-level page */
	page = sub->page + bot;

	/* XXX Invalidate ITLB if neccessary when page->valid == 1 */
	page->physical = physpage;
	page->prkey = prot;
	switch(cache) {
	case MMU_NO_CACHE:
		page->cache = 0; break;
	case MMU_CACHE_BACK:
		page->cache = 0; page->wthru = 0; break;	/* XXX tmp */
	case MMU_CACHE_WT:
		page->cache = 0; page->wthru = 1; break;
	default:
		page->cache = 0; page->wthru = 0; break;
	}
	page->dirty = 1;	/* XXX Initial-write exception not called */
	page->blank = 0;
	page->shared = share;
	page->valid = 1;
}

/* Map N pages sequentially */
void mmu_page_map(mmucontext_t *context,
		int virtpage, int physpage, int count,
		int prot, int cache, int share, int dirty) {
	while (count > 0) {
		mmu_page_map_single(context,
			virtpage, physpage,
			prot, cache, share, dirty);
		virtpage++;
		physpage++;
		count--;
	}
}

#if 0	/* Only applies to KOS-MMU */
/* Syscall version of mmu_page_map; all parameters are adjusted to
   even page boundaries; if src is NULL, anonymous pages are mapped
   (allocated from the heap pool); if src is non-NULL, the address
   is considered to be a physical address. Use munmap to free them. */
void sc_mmu_mmap(uint32 dst, size_t len, uint32 src) {
	int anon = 0;
	
	/* Adjust length to page boundary */
	if (len & PAGEMASK)
		len = (len & ~PAGEMASK) + PAGESIZE;
	len >>= PAGESIZE_BITS;

	/* If no src pointer, then allocate anonymous pages */
	if (!src) {
		src = (uint32)mm_palloc(len, proc_current->pid);
		if (src == 0)
			RETURN(0);
		anon = 1;
	}

	/* Do the actual mapping */
	/*dbgio_printf("sc: mmu_page_map(%08x,%08x,%08x,%08x,%d,%d,%d,%d)\n", 
		proc_current->pt, dst >> PAGESIZE_BITS, src >> PAGESIZE_BITS, len,
		MMU_ALL_RDWR, anon ? MMU_CACHEABLE : MMU_NO_CACHE, MMU_SHARED, MMU_DIRTY); */
	mmu_page_map(proc_current->pt,
		dst >> PAGESIZE_BITS, src >> PAGESIZE_BITS, len,
		MMU_ALL_RDWR,
		anon ? MMU_CACHEABLE : MMU_NO_CACHE,
		MMU_NOT_SHARED,
		MMU_DIRTY);
	RETURN(dst);
}
#endif /* 0 */

/* Copy a chunk of data from a process' address space into a
   kernel buffer, taking into account page mappings.

   This routine is pretty nasty.. this is completely platform
   generic but should probably be replaced by a nice assembly
   routine for each platform as appropriate. */
int mmu_copyin(mmucontext_t *context, uint32 srcaddr, uint32 srccnt, void *buffer) {
	mmupage_t	*srcpage;
	uint32		srcptr;
	uint32		src;
	int		copied, run, srckrn;
	uint8		*dst;

	/* Setup source pointers */
	srcptr = (uint32)srcaddr;
	if (!(srcptr & 0x8000000)) {
		srcpage = map_virt(context, srcptr >> PAGESIZE_BITS);
		if (srcpage == NULL)
			panic("mmu_copyv with invalid source page");
		src = (srcpage->physical << PAGESIZE_BITS) | ( srcptr & PAGEMASK );
		srckrn = 0;
	} else {
		src = srcptr;
		srckrn = 1;
	}

	/* Setup destination pointers */
	dst = (uint8*)buffer;

	/* Do the actual copy */
	copied = 0;
	while (srccnt > 0) {
		/* Determine the largest run we can get away with */

		/* What's left of source page */
		run = PAGESIZE - (srcptr & PAGEMASK);

		/* What's left of source count */
		if ( srccnt < run )
			run = srccnt;

		/* Do the segment copy */
		memcpy(dst, (void*)(src | 0x80000000), run);

		/* Adjust all the pointers */
		src += run; srcptr += run;
		dst += run;

		/* Check for overruns */
		srccnt -= run;
		if (srccnt <= 0) {
			break;
		} else {
			if (!srckrn && (srcptr & ~PAGEMASK) != ((srcptr-run) & ~PAGEMASK) ) {
				srcpage = map_virt(context, srcptr >> PAGESIZE_BITS);
				if (srcpage == NULL)
					panic("mmu_copyv with invalid source page (in loop)");
				src = (srcpage->physical << PAGESIZE_BITS)
					| (srcptr - (srcptr & ~PAGEMASK));
			}
		}

		copied += run;
	}

	return copied;
}

/* Copy a chunk of data from one process' address space to another
   process' address space, taking into account page mappings.

   This routine is pretty nasty.. this is completely platform
   generic but should probably be replaced by a nice assembly
   routine for each platform as appropriate. */
int mmu_copyv(mmucontext_t *context1, iovec_t *iov1, int iovcnt1,
		mmucontext_t *context2, iovec_t *iov2, int iovcnt2) {
	mmupage_t	*srcpage, *dstpage;
	int		srciov, dstiov;
	int		srccnt, dstcnt;
	uint32		srcptr, dstptr;
	uint32		src, dst;
	int		copied, run;
	int		srckrn, dstkrn;
	/* static int	sproket = 0; */

	/* timer_disable_primary();
	irq_enable(); */

	/* Setup source pointers */
	srciov = 0;
	srccnt = iov1[srciov].iov_len;
	srcptr = (uint32)iov1[srciov].iov_base;
	if (!(srcptr & 0x80000000)) {
		srcpage = map_virt(context1, srcptr >> PAGESIZE_BITS);
		if (srcpage == NULL)
			panic("mmu_copyv with invalid source page");
		src = (srcpage->physical << PAGESIZE_BITS) | ( srcptr & PAGEMASK );
		srckrn = 0;
	} else {
		src = srcptr;
		srckrn = 1;
	}

	/* Setup destination pointers */
	dstiov = 0;
	dstcnt = iov2[dstiov].iov_len;
	dstptr = (uint32)iov2[dstiov].iov_base;
	if (!(dstptr & 0x80000000)) {
		dstpage = map_virt(context2, dstptr >> PAGESIZE_BITS);
		if (dstpage == NULL)
			panic("mmu_copyv with invalid destination page");
		dst = (dstpage->physical << PAGESIZE_BITS) | ( dstptr & PAGEMASK );
		dstkrn = 0;
	} else {
		dst = dstptr;
		dstkrn = 1;
	}

	/* Do the actual copy */
	copied = 0;
	while (srciov < iovcnt1 && dstiov < iovcnt2) {
		/* Determine the largest run we can get away with */

		/* What's left of source page */
		run = PAGESIZE - (srcptr & PAGEMASK);

		/* What's left of destination page */
		if ( (PAGESIZE - (dstptr & PAGEMASK)) < run )
			run = PAGESIZE - (dstptr & PAGEMASK);

		/* What's left of source iov */
		if ( srccnt < run )
			run = srccnt;

		/* What's left of dest iov */
		if ( dstcnt < run )
			run = dstcnt;

		/* Do the segment copy */
		/* if (!sproket) {
			dbgio_printf("Copying %08lx -> %08lx (%08lx -> %08lx), %d bytes\n",
				srcptr, dstptr, src, dst, run);
			dbgio_flush();
			dbgio_flush();
			dbgio_flush();
			dbgio_flush();
			sproket = 1;
		} */
		//debug();
		memcpy((void*)(dst | 0xa0000000), (void*)(src | 0x80000000), run);
		/* dcache_inval_range(dstptr, run); */
		dcache_inval_range(dst | 0x80000000, run);
		//undebug();

		/* Adjust all the pointers */
		src += run; srcptr += run;
		dst += run; dstptr += run;
		copied += run;

		/* Check for overruns */
		srccnt -= run;
		if (srccnt <= 0) {
			srciov++;
			if (srciov >= iovcnt1) break;
			
			srccnt = iov1[srciov].iov_len;
			srcptr = (uint32)iov1[srciov].iov_base;
			if (!srckrn) {
				srcpage = map_virt(context1, srcptr >> PAGESIZE_BITS);
				if (srcpage == NULL)
					panic("mmu_copyv with invalid source page (in loop)");
				src = (srcpage->physical << PAGESIZE_BITS) | ( srcptr & PAGEMASK );
			} else {
				src = srcptr;
			}
		} else {
			if (!srckrn && (srcptr & ~PAGEMASK) != ((srcptr-run) & ~PAGEMASK) ) {
				srcpage = map_virt(context1, srcptr >> PAGESIZE_BITS);
				if (srcpage == NULL)
					panic("mmu_copyv with invalid source page (in loop)");
				src = (srcpage->physical << PAGESIZE_BITS)
					| (srcptr - (srcptr & ~PAGEMASK));
			}
		}

		dstcnt -= run;
		if (dstcnt <= 0) {
			dstiov++;
			if (dstiov >= iovcnt2) break;

			dstcnt = iov2[dstiov].iov_len;
			dstptr = (uint32)iov2[dstiov].iov_base;
			if (!dstkrn) {
				dstpage = map_virt(context2, dstptr >> PAGESIZE_BITS);
				if (dstpage == NULL)
					panic("mmu_copyv with invalid destination page (in loop)");
				dst = (dstpage->physical << PAGESIZE_BITS) | ( dstptr & PAGEMASK );
			} else {
				dst = dstptr;
			}
		} else {
			if (!dstkrn && (dstptr & ~PAGEMASK) != ((dstptr-run) & ~PAGEMASK)) {
				dstpage = map_virt(context2, dstptr >> PAGESIZE_BITS);
				if (dstpage == NULL)
					panic("mmu_copyv with invalid destination page (in loop)");
				dst = (dstpage->physical << PAGESIZE_BITS)
					| (dstptr - (dstptr & ~PAGEMASK));
			}
		}
	}

	return copied;
}
		

/********************************************************************************/
/* Exception handlers */

static int last_urc;
static mmu_mapfunc_t map_func;

mmu_mapfunc_t mmu_map_get_callback() {
	return map_func;
}

mmu_mapfunc_t mmu_map_set_callback(mmu_mapfunc_t newfunc) {
	mmu_mapfunc_t tmp = map_func;
	map_func = newfunc;
	return tmp;
}

static void unhandled_mmu(irq_t source, irq_context_t *context) {
	int i;
	
	dbgio_printf("Exception happened in tid %d at PC %08lx, SR %08lx\n",
		thd_current->tid, context->pc, context->sr);
	dbgio_printf(" PTEH = %08lx, PTEL = %08lx\n", *pteh, *ptel);
	dbgio_printf(" TTB = %08lx, TEA = %08lx\n", *ttb, *tea);
	dbgio_printf(" MMUCR = %08lx\n", *mmucr);

	for (i=0; i<512; i++)
		dbgio_flush();

	panic("unhandled MMU exception");
}

/* Generic handler that takes a missed TLB exception and loads the
   appropriate entry into the UTLB. */
static void gen_tlb_miss(const char *what, irq_t source, irq_context_t *context) {
	mmupage_t *page;
	uint32 addr;

	/* Get the offending reference */
	addr = *tea;

	/* Do we have a mapping func? */
	if (!map_func) {
		dbgio_printf("%s: no mapping function to map address %08lx!\n",
			what, addr);
		unhandled_mmu(source, context);
	}
	
	/* Do we have page tables? */
	if (map_func == map_virt && !mmu_cxt_current) {
		dbgio_printf("%s: no page tables installed to map address %08lx!\n",
			what, addr);
		unhandled_mmu(source, context);
	}

	/* Translate it to the proper physical address */
	page = map_func(mmu_cxt_current, addr >> PAGESIZE_BITS);
	if (!page) {
		dbgio_printf("%s: cannot map virtual address %08lx\n", what, addr);
		unhandled_mmu(source, context);
	}

	/* Make sure we don't overwrite the last TLB entry */
	if (GET_URC() == last_urc) {
		last_urc++;
		SET_URC(last_urc);
	} else {
		last_urc = GET_URC();
	}

	/* Load the mapping */
	//dbgio_printf("asid %d: loading up mapping %08x -> %08x, prkey=%d into %x\n",
	//	proc_current->pt->asid, *tea, page->physical << PAGESIZE_BITS, page->prkey, last_urc);
	mmu_ldtlb(mmu_cxt_current->asid, *tea, page->physical << PAGESIZE_BITS, 1, page->prkey,
		page->cache, page->dirty, page->shared, page->wthru);
	mmu_ldtlb_wait();
}

/* Instruction TLB miss exception */
static void itlb_miss(irq_t source, irq_context_t *context) {
	gen_tlb_miss("itlb_miss", source, context);
}

/* Instruction TLB protection violation */
static void itlb_pv(irq_t source, irq_context_t *context) {
	dbgio_printf("itlb_pv\n");
	unhandled_mmu(source, context);
}

/* Should eventually handle data address read/write here */

/* Data TLB miss (read) */
static void dtlb_miss_read(irq_t source, irq_context_t *context) {
	gen_tlb_miss("dtlb_miss_read", source, context);
}

/* Data TLB miss (write) */
static void dtlb_miss_write(irq_t source, irq_context_t *context) {
	gen_tlb_miss("dtlb_miss_write", source, context);
}

/* Data TLB protection violation (read) */
static void dtlb_pv_read(irq_t source, irq_context_t *context) {
	dbgio_printf("dtlb_pv_read\n");
	unhandled_mmu(source, context);
}

/* Data TLB protection violation (write) */
static void dtlb_pv_write(irq_t source, irq_context_t *context) {
	dbgio_printf("dtlb_pv_write\n");
	unhandled_mmu(source, context);
}

/* Initial page write exception */
static void initial_page_write(irq_t source, irq_context_t *context) {
	dbgio_printf("initial_page_write\n");
	unhandled_mmu(source, context);
}

/********************************************************************************/
/* Init routine */
int mmu_init() {
	/* Setup last URC counter (to make sure we don't thrash the
	   TLB caches accidentally) */
	last_urc = 0;

	/* Set the default mapping func */
	map_func = map_virt;

	/* No context yet */
	mmu_cxt_current = NULL;

	/* Set up interrupt handlers */
	irq_set_handler(EXC_ITLB_MISS, itlb_miss);
	irq_set_handler(EXC_ITLB_PV, itlb_pv);
	irq_set_handler(EXC_DTLB_MISS_READ, dtlb_miss_read);
	irq_set_handler(EXC_DTLB_MISS_WRITE, dtlb_miss_write);
	irq_set_handler(EXC_DTLB_PV_READ, dtlb_pv_read);
	irq_set_handler(EXC_DTLB_PV_WRITE, dtlb_pv_write);
	irq_set_handler(EXC_INITIAL_PAGE_WRITE, initial_page_write);

	/* Turn on MMU */
	/* URB=0x3f, URC=0, SQMD=1, SV=0, TI=1, AT=1 */
	SET_MMUCR(0x3f, 0, 1, 0, 1, 1);

	/* Clear the ITLB */
	mmu_reset_itlb();

	/* All done */
	return 0;
}

/* Shutdown */
void mmu_shutdown() {
	/* Turn off MMU */
	*mmucr = 0x00000204;
	
	/* Unhook the IRQ handlers */
	irq_set_handler(EXC_ITLB_MISS, NULL);
	irq_set_handler(EXC_ITLB_PV, NULL);
	irq_set_handler(EXC_DTLB_MISS_READ, NULL);
	irq_set_handler(EXC_DTLB_MISS_WRITE, NULL);
	irq_set_handler(EXC_DTLB_PV_READ, NULL);
	irq_set_handler(EXC_DTLB_PV_WRITE, NULL);
	irq_set_handler(EXC_INITIAL_PAGE_WRITE, NULL);
}

