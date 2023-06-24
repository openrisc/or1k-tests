//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2011 Authors and OPENCORES.ORG                 ////
////                                                              ////
//// This source file may be used and distributed without         ////
//// restriction provided that this copyright statement is not    ////
//// removed from the file and that any derivative work contains  ////
//// the original copyright notice and the associated disclaimer. ////
////                                                              ////
//// This source file is free software; you can redistribute it   ////
//// and/or modify it under the terms of the GNU Lesser General   ////
//// Public License as published by the Free Software Foundation; ////
//// either version 2.1 of the License, or (at your option) any   ////
//// later version.                                               ////
////                                                              ////
//// This source is distributed in the hope that it will be       ////
//// useful, but WITHOUT ANY WARRANTY; without even the implied   ////
//// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      ////
//// PURPOSE.  See the GNU Lesser General Public License for more ////
//// details.                                                     ////
////                                                              ////
//// You should have received a copy of the GNU Lesser General    ////
//// Public License along with this source; if not, download it   ////
//// from http://www.opencores.org/lgpl.shtml                     ////
////                                                              ////
//////////////////////////////////////////////////////////////////////

/**
 *
 * OR1K FPU exceptions test
 *
 * Check that FPU exception occur with the correct side effects.
 *
 *  - Check FPCSR flags
 *  - CHeck correct EPCR is set
 *  - Check return from handler returns at the correct location
 *
 * The gpr1 counts each time we enter the handler.
 *
 * Stafford Horne <shorne@gmail.com>
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
	OR1K_DELAYED_NOP(l.jr    r4)

	/**
         * Floating point exception handler.
         *
	 * EPCR should be set to the next instruction after the FPU exception
         * insgtruction.  Doing l.rfe should allow to continue.
	 */
	.org 0xd00

	l.mfspr	r3,r0,OR1K_SPR_SYS_EPCR_BASE	/* Get PC of bad instruction */
	l.nop 	0x2

	/* gpr8 has the expected flag, check if its set and clear it.  */
	l.mfspr	r3,r0,OR1K_SPR_SYS_FPCSR_ADDR
	l.and	r8,r3,r8
	l.sfeq	r8,r0
        OR1K_DELAYED_NOP(l.bf    bad_fpcsr)

	/* Confirm FPEE is set. */
	l.andi	r8,r3,OR1K_SPR_SYS_FPCSR_FPEE_MASK
	l.sfeq	r8,r0
        OR1K_DELAYED_NOP(l.bf    bad_fpcsr)

	l.addi	r1,r1,1		/* Passed, increment exception counter */

bad_fpcsr:
	/*
	 * Clear the set flags leaving just FPEE.
	 * The mor1kx core has to FPEE bugs
         *  1. it clears the FPEE upon FP exception, it should keep it enabled
	 *  2. it sets EPCR to the address of the failed instruction, which
	 *     would cause and infinite loop if we re-enable exceptions as
         *     the commented line below does
	 * The marocchino core doesnt have this bug
         */
	l.andi	r3,r3,OR1K_SPR_SYS_FPCSR_FPEE_MASK
	//l.ori	r3,r0,OR1K_SPR_SYS_FPCSR_FPEE_MASK
	l.mtspr r0,r3,OR1K_SPR_SYS_FPCSR_ADDR
	l.rfe

/* =================================================== [ data ] === */
	.section .rodata

	.align 4
pi:
	.float 3.141592653589793
zero:
	.float 0.0
flt_max:
	.float 0f3.40282347e+38

/* =================================================== [ text ] === */
	.section .text

/* =================================================== [ start ] === */

	.global _start
_start:
	// Kick off test
	OR1K_DELAYED_NOP(l.jal   _main)

/* =================================================== [ main ] === */

	.global _main
_main:
	l.movhi	r1, 0	/* clear r1 - used as exception handler counter */
	l.movhi	r2, 0	/* clear r2 - used as expected exception counter */

        l.mfspr r3, r0, OR1K_SPR_SYS_CPUCFGR_ADDR
        l.andi  r3, r3, OR1K_SPR_SYS_CPUCFGR_OF32S_MASK
        l.sfne  r3, r0
	/* If no FPU we just pass the test */
        OR1K_DELAYED_NOP(l.bnf    test_ok)

have_of32s:
	/* Enable FPE Exceptions */
	l.mfspr	r3,r0,OR1K_SPR_SYS_FPCSR_ADDR
	l.ori	r3,r3,OR1K_SPR_SYS_FPCSR_FPEE_MASK
	l.mtspr r0,r3,OR1K_SPR_SYS_FPCSR_ADDR

	l.movhi	r5, ha(pi)
	l.lwz	r5, lo(pi)(r5)
	l.movhi	r6, ha(zero)
	l.lwz	r6, lo(zero)(r6)
	l.movhi	r7, ha(flt_max)
	l.lwz	r7, lo(flt_max)(r7)

	/* Test DZF (divide by zero) */
	l.ori	r8, r0, OR1K_SPR_SYS_FPCSR_DZF_MASK
	lf.div.s r4, r5, r6
	l.addi	r2, r2, 1

	// Note mor1kx clears FPEE on floating point exection!

	/* Test OVF (overflow) */
	l.ori	r8, r0, OR1K_SPR_SYS_FPCSR_OVF_MASK
	lf.mul.s r4, r5, r7
	l.addi	r2, r2, 1

	l.sfeq	r1,r2
	OR1K_DELAYED_NOP(l.bf	test_ok)

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


