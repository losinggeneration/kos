! KallistiOS ##version##
!
! startup.s
! (c)2000-2001 Dan Potter
!
! This file must appear FIRST in your linking order, or your program won't
! work correctly as a raw binary.
!
! This is very loosely based on Marcus' crt0.s/startup.s
!

.globl start
.globl _start
.globl _arch_real_exit
.globl __arch_old_sr
.globl __arch_old_vbr
.globl __arch_old_stack
.globl __arch_old_fpscr

_start:
start:
	! Disable interrupts (if they're enabled)
	mov.l	old_sr_addr,r0
	stc	sr,r1
	mov.l	r1,@r0
	mov.l	init_sr,r0
	ldc	r0,sr

	! Run in the P2 area
	mov.l	setup_cache_addr,r0
	mov.l	p2_mask,r1
	or	r1,r0
	jmp	@r0
	nop

setup_cache:
	! Now that we are in P2, it's safe to enable the cache
	! Check to see if we should enable OCRAM.
	mov.l	kos_init_flags_addr, r0
	add	#2, r0
	mov.w	@r0, r0
	tst	#1, r0
	bf	.L_setup_cache_L0
	mov.w	ccr_data,r1
	bra	.L_setup_cache_L1
	nop
.L_setup_cache_L0:
	mov.w	ccr_data_ocram,r1
.L_setup_cache_L1:
	mov.l	ccr_addr,r0
	mov.l	r1,@r0

	! After changing CCR, eight instructions must be executed before
	! it's safe to enter a cached area such as P1
	nop			! 1
	nop			! 2
	nop			! 3
	nop			! 4
	nop			! 5 (d-cache now safe)
	nop			! 6
	mov.l	init_addr,r0	! 7
	mov	#0,r1		! 8
	jmp	@r0		! go
	mov	r1,r0
	nop

init:
	! Save old PR on old stack so we can get to it later
	sts.l	pr,@-r15

	! Save the current stack, and set a new stack (higher up in RAM)
	mov.l	old_stack_addr,r0
	mov.l	r15,@r0
	mov.l	new_stack,r15

	! Save VBR
	mov.l	old_vbr_addr,r0
	stc	vbr,r1
	mov.l	r1,@r0

	! Save FPSCR
	mov.l	old_fpscr_addr,r0
	sts	fpscr,r1
	mov.l	r1,@r0

	! Reset FPSCR
	mov	#4,r4		! Use 00040000 (DN=1)
	mov.l	fpscr_addr,r0
	jsr	@r0
	shll16	r4

	! Setup a sentinel value for frame pointer in case we're using
	! FRAME_POINTERS for stack tracing.
	mov	#-1,r14

	! Jump to the kernel main
	mov.l	main_addr,r0
	jsr	@r0
	nop

	! Program can return here (not likely) or jump here directly
	! from anywhere in it to go straight back to the monitor
_arch_real_exit:
	! Reset SR
	mov.l	old_sr,r0
	ldc	r0,sr

	! Disable MMU, invalidate TLB
	mov	#4,r0
	mov.l	mmu_addr,r1
	mov.l	r0,@r1

	! Wait (just in case)
	nop				! 1
	nop				! 2
	nop				! 3
	nop				! 4
	nop				! 5
	nop				! 6
	nop				! 7
	nop				! 8
	
	! Restore VBR
	mov.l	old_vbr,r0
	ldc	r0,vbr

	! If we're working under dcload, call its EXIT syscall
	mov.l	dcload_magic_addr,r0
	mov.l	@r0,r0
	mov.l	dcload_magic_value,r1
	cmp/eq	r0,r1
	bf	normal_exit

	mov.l	dcload_syscall,r0
	mov.l	@r0,r0
	jsr	@r0
	mov	#15,r4

	! Set back the stack and return (presumably to a serial debug)
normal_exit:
	mov.l	old_stack,r15
	lds.l	@r15+,pr
	rts
	nop

! Misc variables
	.align	2
dcload_magic_addr:
	.long	0x8c004004
dcload_magic_value:
	.long	0xdeadbeef
dcload_syscall:
	.long	0x8c004008
__arch_old_sr:
old_sr:
	.long	0
__arch_old_vbr:
old_vbr:
	.long	0
__arch_old_fpscr:
old_fpscr:
	.long	0
init_sr:
	.long	0x500000f0
old_sr_addr:
	.long	old_sr
old_vbr_addr:
	.long	old_vbr
old_fpscr_addr:
	.long	old_fpscr
old_stack_addr:
	.long	old_stack
__arch_old_stack:
old_stack:
	.long	0
new_stack:
	.long	0x8d000000
p2_mask:
	.long	0xa0000000
setup_cache_addr:
	.long	setup_cache
init_addr:
	.long	init
main_addr:
	.long	_arch_main
mmu_addr:
	.long	0xff000010
fpscr_addr:
	.long	___set_fpscr	! in libgcc
kos_init_flags_addr:
	.long	___kos_init_flags
ccr_addr:
	.long	0xff00001c
ccr_data:
	.word	0x090d
ccr_data_ocram:
	.word	0x092d

	.ident  "KOS $Id: startup.s,v 1.9 2002/09/13 04:40:52 bardtx Exp $"

