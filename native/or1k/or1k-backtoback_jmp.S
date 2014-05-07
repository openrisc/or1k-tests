/*

	OR1K jump to jump test

	Stefan Kristiansson, stefan.kristiansson@saunalahti.fi

*/
#include <or1k-asm.h>
#include "spr-defs.h"

/* =================================================== [ exceptions ] === */
	.section .vectors, "ax"


/* ---[ 0x100: RESET exception ]----------------------------------------- */
        .org 0x100
	l.movhi r0, 0
	/* Clear status register */
	l.ori 	r1, r0, SPR_SR_SM
	l.mtspr r0, r1, SPR_SR
	/* Clear timer  */
	l.mtspr r0, r0, SPR_TTMR

	/* Jump to program initialisation code */
	.global _start
	l.movhi r4, hi(_start)
	l.ori 	r4, r4, lo(_start)
	l.jr    r4
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

test_func0:
	l.lwz	r4, 0(r2)
	l.lwz	r4, 4(r2)
	l.lwz	r4, 8(r2)
	l.lwz	r4, 12(r2)
	l.jr	r9
	 l.lwz	r4, 16(r2)
	.global _main
_main:
	l.movhi	r1, 0
	l.movhi	r2, 0
	l.jal	test_func0
	 l.addi	r2, r0, 0
	l.j	.L1
	 l.sw	16(r1), r2
	l.j	test_fail
	 l.nop
.L1:
	l.jal	.L2
	 l.addi	r1, r1, 1
	l.j	test_fail
	 l.nop
.L2:
	/*
	 * Print and check result
	 */
	l.ori	r3, r1, 0
	l.nop	2
	l.sfnei	r1, 1
	l.bf	test_fail
	 l.nop

	/*
	 * Test finished when ran through once without caches enabled
	 * and twice with.
	 */
	l.sfeqi	r16, 2
	l.bf	test_ok
	 l.addi r16, r16, 1

	l.sfeqi r16, 2
	l.bf	_main
	 l.nop

	/* Restart test with cache enabled */
	l.jal 	_cache_init
	 l.nop
	l.j     _main
	 l.nop

test_fail:
	l.movhi	r3,0xbaaa
	l.ori	r3,r3,0xaaad
	l.nop	0x1

test_ok:
	l.movhi	r3,0x8000
	l.ori	r3,r3,0x000d
	l.nop	0x2
	l.ori 	r3, r0, 0 /* Clear R3 */
	l.nop 	0x1 /* Exit simulation */
	l.nop
	l.nop
