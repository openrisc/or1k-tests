#include <or1k-asm.h>
#include <or1k-sprs.h>
#include "board.h"
	
/*

	User IRQ and multicycle instruction test
	
	Within the test we'll use following global variables:

	r10 irq interrupt counter
	r12 intgen's base address


	The test does the following:
	Uses the intgen module to schedule interrupts which should occur
	during instructions which will largely stall the pipeline, ie.
	load/stores, hardware divides.
	We will have two values, one incremented in the interrupt handler
	and one which is calculated as a result of the successful multi-
	cycle instructions. We will check both at the end.
	
	Julius Baxter, juliusbaxter@gmail.com
*/
//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2013 Authors and OPENCORES.ORG                 ////
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
	/* Init the stack */
	.global stack
	l.movhi r1, hi(stack)
	l.ori 	r1, r1, lo(stack)
	l.addi  r2, r0, -3
	l.and   r1, r1, r2
	/* Jump to program initialisation code */
	.global _start
	l.movhi r4, hi(_start)
	l.ori 	r4, r4, lo(_start)
	l.jr    r4
	l.nop

	
/* =================================================== [ User interrupt ] === */
	.org 0x800
	.global _user_irq_handler
_user_irq_handler:	
	l.addi r10, r10, 1
	/* Report values , 0x00000800 == user interrupt report*/
	l.ori r3, r0, 0x0800
	l.nop 2
	l.or r3, r0, r10
	l.nop 2
	/* TODO - propably confirm it was intgen's IRQ that caused this */
	/* Clear interrupt source */
	l.ori	r7, r12, 0x1 	/* intgen IRQ clear address */
	l.sb	0(r7), r0 	/* Any write clears the bit */
	/* Clear OR1200 PICSR */
	l.mfspr r7, r0, OR1K_SPR_PIC_PICSR_ADDR
	l.mtspr r0, r7, OR1K_SPR_PIC_PICSR_ADDR

	l.rfe

/* =================================================== [ text section ] === */
	.section  .text

/* =================================================== [ start ] === */	

	.global _start
_start:	
	// Kick off test
	l.jal   _main
	l.nop

/* =================================================== [ main ] === */	
.global _main
_main:
	
	#
	# unmask (enable) all ints
	#
	l.movhi r5,0xffff
	l.ori   r5,r5,0xffff
	l.mtspr r0,r5,OR1K_SPR_PIC_PICMR_ADDR		# set PICMR

	/* Enable Interrupts */
        l.mfspr r6,r0,OR1K_SPR_SYS_SR_ADDR
        l.ori   r6,r6,OR1K_SPR_SYS_SR_IEE_MASK
        l.mtspr r0,r6,OR1K_SPR_SYS_SR_ADDR

	l.movhi r10, 0 // register to store number of IRQs
	l.movhi	r11, 0 // register to store accumulated value
	l.movhi	r13, 0 // register to store number of rounds
	
	// set up r12 to hold intgen's base address
	l.movhi	r12,hi(INTGEN_BASE)

load_store_tests:
	// This test macro loads the countdown timer in the interrupt block
	// it then writes the value passed to us into memory, and reads it
	// back. The idea is at some point during those writes the interrupt
	// will go off, and the test is for the pipeline to gracefully
	// handle the interrupt and continue whatever it was doing.
	// The l.nops after are for when the loads/stores may execute very
	// quickly, and we proceeed to the next test too soon. We could have
	// a loop but then when something happens and we miss the interrupt
	// then we'll loop forever.
#define INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(x)	\
	l.ori	r5,r0,lo(x)					;\
	l.sb	0(r12),r5					;\
	l.sw	0(r0),r5					;\
	l.lwz	r8, 0(r0)					;\
	l.add	r11,r11,r8					;\
	l.addi	r13,r13,1					;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							;\
	l.nop							

	/* Test begin */
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(2)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(3)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(4)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(5)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(6)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(7)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(8)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(9)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(10)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(11)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(12)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(13)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(14)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(15)
	
	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(16)

	INTGEN_LOAD_AND_ACCUMULATE_DELAY_VIA_LOADSTORE(17)
	
	// check load/stores went to plan
	l.sfnei	r11,182 /* Should equal 182 */
	l.ori	r3,r11,0
	l.nop	0x2
	l.bf 	_fail
	l.nop
	
	// check interrupts were serviced correctly
	l.sfne	r10,r13 // These should be equal
	l.ori	r3,r10,0
	l.nop	0x2
	l.ori	r3,r13,0
	l.nop	0x2
	
	l.bf 	_fail
	l.nop	


divide_tests:	


	l.movhi r10, 0 // register to store number of IRQs
	l.movhi	r11, 0 // register to store accumulated value
	l.movhi	r13, 0 // register to store number of rounds
	
#define INTGEN_LOAD_AND_CAUSE_DIV(n,d,x)	\
	l.ori	r14,r0,lo(x)					;\
	l.movhi	r5,hi(n)					;\
	l.ori	r5,r5,lo(n)					;\
	l.movhi	r6,hi(d)					;\
	l.ori	r6,r6,lo(d)					;\
	l.sb	0(r12),r14					;\
	l.div	r5,r5,r6         				;\
	l.add	r11,r11,r5					;\
	l.addi	r13,r13,1

	// 0x13370000,0x0000babe = 6743
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 2)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 3)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 4)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 5)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 6)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 7)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 8)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 9)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 10)
	
	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 11)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 12)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 13)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 14)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 15)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 16)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 17)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 18)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 19)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 20)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 21)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 22)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 23)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 24)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 25)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 26)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 27)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 28)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 29)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 30)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 31)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 32)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 33)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 34)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 35)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 36)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 37)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 38)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 39)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 40)

	INTGEN_LOAD_AND_CAUSE_DIV(0x13370000,0x0000babe, 41)

	
#define DIV_TOTAL (6743*40)
	
	// check load/stores went to plan
	l.movhi	r4,hi(DIV_TOTAL)
	l.ori	r4,r4,lo(DIV_TOTAL)
	l.sfne	r11,r4
	l.ori	r3,r11,0
	l.nop	0x2
	l.bf 	_fail
	l.nop
	
	// check interrupts were serviced correctly
	l.sfne	r10,r13 // These should be equal
	l.bf 	_fail
	l.nop	

success:	
	l.movhi r3, hi(0x8000000d)
	l.ori r3, r3, lo(0x8000000d)
	l.nop 2
	l.ori r3, r0, 0
	l.nop 1
	
_fail:
	l.movhi r3, hi(0xbaaaaaad)
	l.ori r3, r3, lo(0xbaaaaaad)
	l.nop 1
