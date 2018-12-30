/*

	OR1K basic mul op test

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

	/* Timer exception, clear interrupt bit, and return */
	.org 0x500
	l.mfspr	r21,r0, OR1K_SPR_TICK_TTMR_ADDR
	l.movhi	r22, hi(OR1K_SPR_TICK_TTMR_IP_MASK)
	l.xor	r21, r21, r22
	l.mtspr	r0, r21, OR1K_SPR_TICK_TTMR_ADDR
	l.addi	r23,r23,1
	l.rfe

	// Alignment error handler.
	// Clobbers r3,r4, increments r1
	// Steps over exception-causing instruction
	.org 0x600

	l.mfspr	r3,r0,OR1K_SPR_SYS_EPCR_BASE	/* Get PC of bad jump instruction */
	l.nop 	0x2
	l.addi	r3,r3,4 	/* Step over error instruction */
	l.mtspr	r0,r3,OR1K_SPR_SYS_EPCR_BASE
	l.addi	r1,r1,1		/* Increment exception counter */
	l.or	r3,r1,r0	/* Move counter to r3 to report */
	l.nop 	0x2
	l.rfe


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
	l.ori	r3, r0, 2
	l.ori	r4, r0, 2
	l.mul	r5, r3, r4
	l.sfeqi	r5, 4
	l.bf	test_ok
	 l.nop

test_fail:
	l.movhi	r3,0xbaaa
	l.ori	r3,r3,0xaaad
	l.nop	0x2
	l.nop	0x1

test_ok:
	l.movhi	r3,0x8000
	l.ori	r3,r3,0x000d
	l.nop	0x2
	l.movhi	r3,0x0
	l.nop	0x1
