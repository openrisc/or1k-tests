/*
	OR1K msync test

	Checks that l.msync doesn't cause illegal instruction exception and
	in case dbus exceptions can be delayed, check that a l.msync creates a
	barrier.

	Stefan Kristiansson, stefan.kristiansson@saunalahti.fi
*/
#include <or1k-asm.h>
#include <or1k-sprs.h>

/* =================================================== [ exceptions ] === */
	.section .vectors, "ax"


/* ---[ 0x100: RESET exception ]----------------------------------------- */
        .org 0x100
	l.movhi r0, 0
	/* Clear status register */
	l.ori 	r1, r0, OR1K_SPR_SYS_SR_SM_MASK
	l.mtspr r0, r1, OR1K_SPR_SYS_SR_ADDR
	/* Clear timer  */
	l.mtspr r0, r0, OR1K_SPR_TICK_TTMR_ADDR

	/* Jump to program initialisation code */
	.global _start
	l.movhi r4, hi(_start)
	l.ori 	r4, r4, lo(_start)
	l.jr    r4
	l.nop

	/* Busfault exception  */
	.org 0x200
	l.ori	r3, r0, 0x200
	l.nop	0x2
	/* report 'side-effect' */
	l.ori	r3, r17, 0
	l.nop	0x2
	/* save the 'side-effect' result */
	l.ori	r19, r17, 0
	/* skip the fault insn */
	l.mfspr	r24, r0, OR1K_SPR_SYS_EPCR_BASE
	l.addi	r24, r24, 4
	l.mtspr	r0, r24, OR1K_SPR_SYS_EPCR_BASE
	/* notify the test that the exception handler has been done */
	l.ori	r18, r0, 1
	l.rfe

	/* Timer exception, clear interrupt bit, and return */
	.org 0x500
	l.mfspr	r21,r0, OR1K_SPR_TICK_TTMR_ADDR
	l.movhi	r22, hi(OR1K_SPR_TICK_TTMR_IP_MASK)
	l.xor	r21, r21, r22
	l.mtspr	r0, r21, OR1K_SPR_TICK_TTMR_ADDR
	l.addi	r23,r23,1
	l.rfe

	/* Illegal instruction exception, should not happen */
	.org 0x700
	l.ori	r3, r0, 0x700
	l.nop	0x2
	l.j	test_fail
	 l.nop

/* =================================================== [ text ] === */
	.section .text

/* =================================================== [ start ] === */

	.global _start
_start:
	/* r16 hold test loop counter */
	l.movhi r16, 0
	// Kick off test
	l.jal   _main
	l.nop

/* =================================================== [ main ] === */

	.global _main
_main:
	/* test 1, check that msync doesn't cause an illegal instruction */
	l.movhi	r3, hi(0x70000001)
	l.ori	r3, r3, lo(0x70000001)
	l.nop	0x2
	l.msync

	/* disable timer exception while doing side-effect check */
	l.mfspr	r6, r0, OR1K_SPR_SYS_SR_ADDR
	l.ori	r2, r6, OR1K_SPR_SYS_SR_TEE_MASK
	l.xori	r2, r2, OR1K_SPR_SYS_SR_TEE_MASK
	l.mtspr	r0, r2, OR1K_SPR_SYS_SR_ADDR

	/* check if the cpu does posted stores */
	l.ori	r17, r0, 0
	l.ori	r18, r0, 0
	l.movhi	r5, hi(0xdeadbeef)
	l.ori	r5,r5, lo(0xdeadbeef)
	l.movhi	r4, hi(0xe0000000)
	l.ori	r4,r4, lo(0xe0000000)
	l.sw	0(r4), r5
	l.nop
1:	l.addi	r17, r17, 1
	l.sfeqi	r18, 0
	l.bf	1b
	 l.nop

	/* restore timer exception */
	l.mtspr	r0, r6, OR1K_SPR_SYS_SR_ADDR

	/* skip test if there are no side-effects */
	l.sfeqi	r19,0
	l.bf	no_side_effects
	 l.nop

	/*
	 * test 2
	 * rerun the sequence that caused side-effects, but now with the l.nop
	 * replaced with a l.msync, which should force the store to store to be
	 * fully performed before the pipeline continues.
	 */
	l.movhi	r3, hi(0x70000002)
	l.ori	r3, r3, lo(0x70000002)
	l.nop	0x2
	l.ori	r17, r0, 0
	l.ori	r18, r0, 0
	l.movhi	r5, hi(0xdeadbeef)
	l.ori	r5,r5, lo(0xdeadbeef)
	l.movhi	r4, hi(0xe0000000)
	l.ori	r4,r4, lo(0xe0000000)
	l.sw	0(r4), r5
	l.msync
1:	l.addi	r17, r17, 1
	l.sfeqi	r18, 0
	l.bf	1b
	 l.nop

no_side_effects:
	/*
	 * Test finished when ran through once without caches enabled
	 * and twice with.
	 */
	l.ori	r3, r16, 0
	l.nop	0x2
	l.sfeqi	r16, 2
	l.bf	test_ok
	 l.addi	r16, r16, 1

	l.sfeqi	r16, 2
	l.bf	_main
	 l.nop

	/* Restart test with cache enabled */
	l.jal 	_cache_init
	 l.nop
	l.j   	 _main
	 l.nop

test_fail:
	l.movhi	r3,0xbaaa
	l.ori	r3,r3,0xaaad
	l.nop	0x1

test_ok:
	/* Rerun test with timers enabled if we have them */
	l.mfspr r2,r0,OR1K_SPR_SYS_UPR_ADDR
	l.andi 	r2,r2,OR1K_SPR_SYS_UPR_TTP_MASK
	l.sfeq	r2,r0
	l.bf	test_finish
	l.nop
	/* We do have  timers, in this case check it it's enabled yet */
	l.mfspr	r2,r0,OR1K_SPR_SYS_SR_ADDR
	l.andi	r2,r2,OR1K_SPR_SYS_SR_TEE_MASK
	l.sfeq 	r2,r0
	/* we've not run through with timers so enable them and restart */
	l.bf	enable_timers_and_restart
	l.nop

test_finish:
	l.movhi	r3,0x8000
	l.ori	r3,r3,0x000d
	l.nop	0x2
	l.ori 	r3, r0, 0 /* Clear R3 */
	l.nop 	0x1 /* Exit simulation */
	l.nop
	l.nop

enable_timers_and_restart:
	/* enable timer interrupts in SR, clear timer mode reg */
	l.mfspr	r3, r0, OR1K_SPR_SYS_SR_ADDR
	l.ori 	r3, r3, OR1K_SPR_SYS_SR_TEE_MASK
	l.mtspr r0, r0, OR1K_SPR_TICK_TTMR_ADDR
	l.mtspr	r0, r3, OR1K_SPR_SYS_SR_ADDR

	/* Setup timer mode register to run in restart mode, with interrupts */
	l.movhi r4,hi((OR1K_SPR_TICK_TTMR_MODE_RESTART << OR1K_SPR_TICK_TTMR_MODE_LSB) | OR1K_SPR_TICK_TTMR_IE_MASK)
	/* interrupt every 100 cycles */
	l.ori	r6, r0, 100

	/* load timer */
	l.or	r5,r4,r6 /* value we'll interrupt on */
	l.mtspr r0,r0,OR1K_SPR_TICK_TTCR_ADDR 		# set TTCR
	l.mtspr r0,r5,OR1K_SPR_TICK_TTMR_ADDR		# set TTMR
	l.mtspr r0,r0,OR1K_SPR_TICK_TTCR_ADDR 		# set TTCR

	/* clear r23, tick interrupt counter */
	l.movhi	r23, 0

	l.j	_start
	l.nop
