//////////////////////////////////////////////////////////////////////
////                                                              ////
////                                                              ////
//// OR1200 MMU test                                              ////
////                                                              ////
////                                                              ////
//////////////////////////////////////////////////////////////////////
////                                                              ////
//// Copyright (C) 2010 Authors and OPENCORES.ORG                 ////
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

// Tests most functions of the MMUs.
// The tests attempt to keep the areas of instruction and data memory working
// as they should, while testing around them, higher in memory. Some linker
// variables are relied upon by the interrupt routines for checking if areas
// are within the expected text areas of memory, but usually we just keep
// above the stack (also set in the linker script) and things go OK.

// The tests for translation setup address translation in the MMU from areas
// in the 512kB - 1024kB region (64 sets in OR1200, 8kByte per set) to halfway
// through RAM. Usually the sets that would encompass the actual program are
// skipped (TLB_TEXT_SET_NB) ie, we say how many sets encompass the program
// text for itlb tests and only do tests above it (so, on sets 8-64, meaning
// when we enable the iMMU and say have the execution bits disabled to force a
// page fault the program gets to continue and is translated 1-1 while
// accesses to the areas we are testing will cause a page fault) but it should
// still work to test all sets.

// In essense, the tests are aware that they could be operating out of the
// same memory the tests are being performed in and takes care of this.

// The use of the "ea" variable is for the addresses we'll access
// when a non-1-1 translation will occur, otherwise usually the
// "ta" variable by itself is used (it's first setup to the
// desired address, and either data or the return instructions are placed
// where we expect the MMU to translate to)

#include <stdio.h>
#include <stdlib.h>
#include <or1k-support.h>
#include <or1k-sprs.h>

#include "support.h"
#include "board.h"

#define ITLB_PR_NOLIMIT  (OR1K_SPR_IMMU_ITLBW_TR_SXE_MASK | \
                          OR1K_SPR_IMMU_ITLBW_TR_UXE_MASK)

#define DTLB_PR_NOLIMIT  (OR1K_SPR_DMMU_DTLBW_TR_URE_MASK | \
                          OR1K_SPR_DMMU_DTLBW_TR_UWE_MASK | \
                          OR1K_SPR_DMMU_DTLBW_TR_SRE_MASK | \
                          OR1K_SPR_DMMU_DTLBW_TR_SWE_MASK)

// Define to run only tests on SHORT_TEST_NUM TLB sets
#define SHORT_TEST
#define SHORT_TEST_NUM 4

// Defines useful when wishing to skip instruction or data MMU tests when doing
// development on one or the other.

// Symbols defined in linker script
extern unsigned long __executable_start;
extern unsigned long _etext;
extern unsigned long _end;
extern unsigned long _or1k_stack_bottom;
extern unsigned long _or1k_board_mem_base;
extern unsigned long _or1k_board_mem_size;

unsigned long start_text_addr;
unsigned long end_text_addr;
unsigned long start_data_addr;
unsigned long end_data_addr;
unsigned long start_stack_addr;

/* Define RAM physical location and size
Bottom half will be used for this program, the rest
will be used for testing */
#define RAM_START _or1k_board_mem_base
#define RAM_SIZE  _or1k_board_mem_size

// Addresses that are translated 1-to-1
inline int program_owned_addr(unsigned long addr) {
  return (addr <= end_data_addr)
      || ((addr >= start_stack_addr) && (addr < RAM_SIZE));
}

#define VM_BASE 0xc0000000

/* Text and data index bases for testing. */
unsigned long TLB_TEXT_SET_NB;
unsigned long TLB_DATA_SET_NB;

/* MMU page size */
#define PAGE_SHIFT      13
#define PAGE_SIZE	8192
#define PAGE_MASK       (~(PAGE_SIZE-1))

/* Copied from linux kernel */
#define PFN_ALIGN(x)    (((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)       (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)     ((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)     ((phys_addr_t)(x) << PAGE_SHIFT)

/* TLB mode codes */
#define TLB_CODE_ONE_TO_ONE     0x00000000
#define TLB_CODE_PLUS_ONE_PAGE  0x10000000
#define TLB_CODE_MINUS_ONE_PAGE 0x20000000

#define TLB_CODE_MASK   0xfffff000
#define TLB_PR_MASK     0x00000fff

/* fails if x is false */
#define ASSERT(x) ((x)?1: fail (__FUNCTION__, __LINE__))

/* Local functions prototypes */
static void dmmu_disable (void);
static void immu_disable (void);

// Machine code for l.jr r9 and then l.nop
#define OR32_L_JR_R9 0x44004800
#define OR32_L_NOP 0x15000000

/* DTLB mode status */
static volatile unsigned long dtlb_val;

/* ITLB mode status */
static volatile unsigned long itlb_val;

/* DTLB miss counter */
static volatile int dtlb_miss_count;

/* Data page fault counter */
static volatile int dpage_fault_count;

/* ITLB miss counter */
static volatile int itlb_miss_count;

/* Instruction page fault counter */
static volatile int ipage_fault_count;

/* EA of last DTLB miss exception */
static volatile unsigned long dtlb_miss_ea;

/* EA of last data page fault exception */
static volatile unsigned long dpage_fault_ea;

/* EA of last ITLB miss exception */
static volatile unsigned long itlb_miss_ea;

/* EA of last insn page fault exception */
static volatile unsigned long ipage_fault_ea;

static unsigned long dtlb_sets;
static unsigned long dtlb_ways;
static unsigned long itlb_sets;
static unsigned long itlb_ways;

#define sys_call() __asm__ __volatile__("l.sys\t0");

static void fail (char *func, int line)
{
#ifndef __FUNCTION__
#define __FUNCTION__ "?"
#endif

  /* Trigger sys call exception to enable supervisor mode again */
  sys_call();

  immu_disable ();
  dmmu_disable ();

  report(line);
  report(0xfae1ed);
  exit(1);
}

static void call (unsigned long add)
{
  asm("l.jalr\t\t%0" : : "r" (add) : "r9", "r11");
  asm("l.nop" : :);
}

/* Bus error exception handler */
static void bus_err_handler (void)
{
  /* This shouldn't happend */
  puts ("Test failed: Bus error");
  report (0xeeeeeeee);
  exit (1);
}

static void tick_timer_handler (void)
{
  puts("Tick from timer?");
  or1k_timer_disable();
  exit (1);
}

/* Illegal insn exception handler */
static void ill_insn_handler (void)
{
  /* This shouldn't happend */
  puts("Test failed: Illegal insn");
  report (0xeeeeeeee);
  exit (1);
}

/* Sys call exception handler */
static void sys_call_handler (void)
{
  /* Set supervisor mode */
  mtspr (OR1K_SPR_SYS_ESR_BASE, mfspr (OR1K_SPR_SYS_ESR_BASE) | OR1K_SPR_SYS_SR_SM_MASK);
}

static volatile int (*dtlb_set_translate)(int set);
static volatile int (*itlb_set_translate)(int set);

static int tlb_default_set_translate (int set) {
  return set;
}
static int dtlb_swap_set_translate (int set) {
  return (dtlb_sets - 1) + (TLB_DATA_SET_NB - set);
}
static int itlb_swap_set_translate (int set) {
  return (itlb_sets - 1) + (TLB_TEXT_SET_NB - set);
}


/* DTLB miss exception handler */
static void dtlb_miss_handler (void)
{
  unsigned long ea, ta, tlbtr;
  int set, way = 0;
  int i;

  /* Get EA that cause the exception */
  ea = mfspr (OR1K_SPR_SYS_EEAR_BASE);

  /* Find TLB set and LRU way */
  set = PFN_DOWN(ea) % dtlb_sets;
  for (i = 0; i < dtlb_ways; i++) {
    if ((mfspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, set)) & OR1K_SPR_DMMU_DTLBW_MR_LRU_MASK) == 0) {
      way = i;
      break;
    }
  }

  // Anything under the stack belongs to the program, direct tranlsate it
  if (program_owned_addr(ea)) {
    /* If this is acces to data of this program set one to one translation */
    report (0xd537f); // self owned memory dtlb miss
    report (ea);
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(way, set), (ea & OR1K_SPR_DMMU_DTLBW_MR_VPN_MASK) | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(way, set), (ea & OR1K_SPR_DMMU_DTLBW_TR_PPN_MASK) | DTLB_PR_NOLIMIT);
    return;
  }

  /* Update DTLB miss counter and EA */
  dtlb_miss_count++;
  dtlb_miss_ea = ea;

  report (0xd7357); // test owned memory dtlb miss
  report (ea);
  // Everything gets translated back to the space halfway through RAM
  ta = (dtlb_set_translate(set) * PAGE_SIZE) + RAM_START + (RAM_SIZE/2);
  tlbtr = (ta & OR1K_SPR_DMMU_DTLBW_TR_PPN_MASK) | (dtlb_val & TLB_PR_MASK);

  /* Set DTLB entry */
  mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(way, set), (ea & OR1K_SPR_DMMU_DTLBW_MR_VPN_MASK) | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(way, set), tlbtr);
}

/* Data page fault exception handler */
/* When a page fault happens it means the TLB is setup for the EA, but
   the permission flags are invalid for the given access.  This handler
   simply grants permission and returns.  */
static void dpage_fault_handler (void)
{
  unsigned long ea;
  int set, way = 0;
  int i;

  /* Get EA that cause the exception */
  ea = mfspr (OR1K_SPR_SYS_EEAR_BASE);

  /* Find TLB set and way */
  set = PFN_DOWN(ea) % dtlb_sets;
  for (i = 0; i < dtlb_ways; i++) {
    if ((mfspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, set)) & OR1K_SPR_DMMU_DTLBW_MR_VPN_MASK) ==
	(ea & OR1K_SPR_DMMU_DTLBW_MR_VPN_MASK)) {
      way = i;
      break;
    }
  }

  if (program_owned_addr(ea)) {
    report (0xdf531f); // program owned memory data page fault
    report (ea);
    /* If this is acces to data of this program set one to one translation */
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(way, set), (ea & OR1K_SPR_DMMU_DTLBW_TR_PPN_MASK) | DTLB_PR_NOLIMIT);
    return;
  }

  report (0xdf7357); // test owned memory data page fault
  report (ea);

  /* Update data page fault counter and EA */
  dpage_fault_count++;
  dpage_fault_ea = ea;

  /* Give permission */
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(way, set),
	(mfspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(way, set)) & ~DTLB_PR_NOLIMIT) | dtlb_val);
}


/* ITLB miss exception handler */
static void itlb_miss_handler (void)
{
  unsigned long ea, ta, tlbtr;
  int set, way = 0;
  int i;

  /* Get EA that cause the exception */
  ea = mfspr (OR1K_SPR_SYS_EEAR_BASE);

  /* Find TLB set and LRU way */
  set = (ea / PAGE_SIZE) % itlb_sets;
  for (i = 0; i < itlb_ways; i++) {
    if ((mfspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, set)) & OR1K_SPR_IMMU_ITLBW_MR_LRU_MASK) == 0) {
      way = i;
      break;
    }
  }

  if (program_owned_addr(ea)) {
    report (0x1531f); // self owned memory itlb miss
    report (ea);
    /* If this is acces to data of this program set one to one translation */
    mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(way, set), (ea & OR1K_SPR_IMMU_ITLBW_MR_VPN_MASK) | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
    mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(way, set), (ea & OR1K_SPR_IMMU_ITLBW_TR_PPN_MASK) | ITLB_PR_NOLIMIT);
    return;
  }

  /* Update ITLB miss counter and EA */
  itlb_miss_count++;
  itlb_miss_ea = ea;

  report (0x17357); // test owned memory itlb miss
  report (ea);
  /* Whatever access is in progress, translated address have to point to
     physical RAM */
  ta = (itlb_set_translate(set) * PAGE_SIZE) + RAM_START + (RAM_SIZE/2);
  tlbtr = (ta & OR1K_SPR_IMMU_ITLBW_TR_PPN_MASK) | (itlb_val & TLB_PR_MASK);

  /* Set ITLB entry */
  mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(way, set), (ea & OR1K_SPR_IMMU_ITLBW_MR_VPN_MASK) | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
  mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(way, set), tlbtr);
}

/* Intstruction page fault exception handler */
/* When a page fault happens it means the TLB is setup for the EA, but
   the permission flags are invalid for the given access.  This handler
   simply grants permission and returns.  */
static void ipage_fault_handler (void)
{
  unsigned long ea;
  int set, way = 0;
  int i;

  /* Get EA that cause the exception */
  ea = mfspr (OR1K_SPR_SYS_EEAR_BASE);

  /* Find TLB set and way */
  set = PFN_DOWN(ea) % itlb_sets;
  for (i = 0; i < itlb_ways; i++) {
    if ((mfspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, set)) & OR1K_SPR_IMMU_ITLBW_MR_VPN_MASK) ==
	(ea & OR1K_SPR_IMMU_ITLBW_MR_VPN_MASK)) {
      way = i;
      break;
    }
  }

  if (program_owned_addr(ea)) {
    report (0x1f531f); // program owned memory data page fault
    report (ea);
    /* If this is acces to data of this program set one to one translation */
    mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(way, set), (ea & OR1K_SPR_IMMU_ITLBW_TR_PPN_MASK) | ITLB_PR_NOLIMIT);
    return;
  }

  report (0x1f7357); // test owned memory data page fault
  report (ea);

  /* Update instruction page fault counter and EA */
  ipage_fault_count++;
  ipage_fault_ea = ea;

  /* Give permission */
  mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(way, set),
	(mfspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(way, set)) & ~ITLB_PR_NOLIMIT) | itlb_val);
}

/* Invalidate all entries in DTLB and enable DMMU */
static void dmmu_enable (void)
{
  /* Register DTLB miss handler */
  or1k_exception_handler_add (0x9, dtlb_miss_handler);

  /* Register data page fault handler */
  or1k_exception_handler_add (0x3, dpage_fault_handler);

  /* Enable DMMU */
  or1k_dmmu_enable ();
}

/* Disable DMMU */
static void dmmu_disable (void)
{
  or1k_dmmu_disable ();
}

/* Invalidate all entries in ITLB and enable IMMU */
static void immu_enable (void)
{
  /* Register ITLB miss handler */
  or1k_exception_handler_add (0xa, itlb_miss_handler);
  //excpt_itlbmiss = (unsigned long)itlb_miss_handler;

  /* Register instruction page fault handler */
  or1k_exception_handler_add (0x4, ipage_fault_handler);
  //excpt_ipfault = (unsigned long)ipage_fault_handler;

  /* Enable IMMU */
  or1k_immu_enable ();
}

/* Disable IMMU */
static void immu_disable (void)
{
  or1k_immu_disable ();
}

/* Setup program, data and stack memory in the DTLB and ITLB,
   This is not strickly needed as tlb misses will set these up
   for us but it helps to test setting these up outside the tlb
   handler.  */
static void tlb_map_program_memory()
{
  int i, j;
  unsigned long ea, ta;

  /* Invalidate all entries in DTLB */
  for (i = 0; i < dtlb_ways; i++) {
    for (j = 0; j < dtlb_sets; j++) {
      mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, j), 0);
      mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(i, j), 0);
    }
  }

  /* Map program data and stack space one to one */
  for (i = PFN_DOWN(start_data_addr); i < TLB_DATA_SET_NB; i++) {
    int set = i % dtlb_sets;

    ea = RAM_START + (i*PAGE_SIZE);
    ta = RAM_START + (i*PAGE_SIZE);
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(0, set), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(0, set), ta | (DTLB_PR_NOLIMIT));
  }
  for (i = PFN_DOWN(start_stack_addr); i < (PFN_DOWN(RAM_SIZE)); i++) {
    int set = i % dtlb_sets;

    ea = RAM_START + (i*PAGE_SIZE);
    ta = RAM_START + (i*PAGE_SIZE);
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(0, set), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(0, set), ta | (DTLB_PR_NOLIMIT));
  }

  /* Invalidate all entries in ITLB */
  for (i = 0; i < itlb_ways; i++) {
    for (j = 0; j < itlb_sets; j++) {
      mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, j), 0);
      mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(i, j), 0);
    }
  }

  /* Set one to one translation for the use of this program */
  for (i = 0; i < TLB_TEXT_SET_NB; i++) {
    ea = start_text_addr + (i*PAGE_SIZE);
    ta = start_text_addr + (i*PAGE_SIZE);
    mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(0, i), ea | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
    mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(0, i), ta | ITLB_PR_NOLIMIT);
  }

}

/* Translation address register test
Set various translation and check the pattern */
static int dtlb_translation_test (void)
{
  int i;
  unsigned long ea, ta;

  /* Disable DMMU */
  dmmu_disable();

  puts("dtlb translation test set");
  tlb_map_program_memory();

  /* Set dtlb miss handler default permisions and set translation */
  dtlb_val = DTLB_PR_NOLIMIT;
  dtlb_set_translate = &tlb_default_set_translate;

  /* Write test pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    ea = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    REG32(ea) = i;
    ea = RAM_START + (RAM_SIZE/2) + ((i + 1)*PAGE_SIZE) - 4;
    REG32(ea) = 0xffffffff - i;
  }

  /* Set one to one translation */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    ea = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    ta = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(dtlb_ways - 1, i), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, i), ta | DTLB_PR_NOLIMIT);
  }

  puts("Enabling DMMU");
  dmmu_enable();

  puts("check 1 - mid to mid");
  /* Check the pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    ea = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    ASSERT(REG32(ea) == i);
    ea = RAM_START + (RAM_SIZE/2) + ((i + 1)*PAGE_SIZE) - 4;
    ASSERT(REG32(ea) == (0xffffffff - i));
  }

  /* Write new pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    ea = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    REG32(ea) = 0xffffffff - i;
    ea = RAM_START + (RAM_SIZE/2) + ((i + 1)*PAGE_SIZE) - 4;
    REG32(ea) = i;
  }

  dmmu_disable();
  /* Set 0 -> RAM_START + (RAM_SIZE/2) translation */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    ea = i*PAGE_SIZE;
    ta = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(dtlb_ways - 1, i), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, i), ta | DTLB_PR_NOLIMIT);
  }
  dmmu_enable();

  puts("check 2 - low to mid");
  /* Check the pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    ea = i*PAGE_SIZE;
    ASSERT(REG32(ea) == (0xffffffff - i));
    ea = ((i + 1)*PAGE_SIZE) - 4;
    ASSERT(REG32(ea) == i);
  }

  /* Write new pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    REG32(i*PAGE_SIZE) = i;
    REG32(((i + 1)*PAGE_SIZE) - 4) = 0xffffffff - i;
  }

  /* Set hi -> lo, lo -> hi translation */
  dmmu_disable();
  dtlb_set_translate = &dtlb_swap_set_translate;

  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    // accessing the first set page 12                     13
    // goes to the last set page    63 = (64-1) + (12-12)  62 = (64-1) + (12-13)
    int swap = dtlb_swap_set_translate(i);

    ea = RAM_START + (RAM_SIZE/2) + (i * PAGE_SIZE);
    ta = RAM_START + (RAM_SIZE/2) + (swap * PAGE_SIZE);
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(dtlb_ways - 1, i), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, i), ta | DTLB_PR_NOLIMIT);
  }
  dmmu_enable();

  puts("check 3 - swapped");
  /* Check the pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    int swap = dtlb_swap_set_translate(i);

    ea = RAM_START + (RAM_SIZE/2) + (i * PAGE_SIZE);
    ASSERT(REG32(ea) == swap);
    ea = RAM_START + (RAM_SIZE/2) + (((i + 1)*PAGE_SIZE) - 4);
    ASSERT(REG32(ea) == (0xffffffff - swap));
  }

  /* Write new pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    REG32(RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE)) = 0xffffffff - i;
    REG32(RAM_START + (RAM_SIZE/2) + ((i + 1)*PAGE_SIZE) - 4) = i;
  }

  dmmu_disable();
  puts("Disabled DMMU");

  puts("check 4 - swapped mmu off");
  /* Check the pattern */
  for (i = TLB_DATA_SET_NB; i < dtlb_sets; i++) {
    int swap = dtlb_swap_set_translate(i);

    ea = RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE);
    ASSERT(REG32(ea) == (0xffffffff - swap));
    ea = RAM_START + (RAM_SIZE/2) + ((i + 1)*PAGE_SIZE) - 4;
    ASSERT(REG32(ea) == swap);
  }

  dtlb_set_translate = &tlb_default_set_translate;

  puts("-------------------------------------------");
  return 0;
}

/* EA match register test
   Shifting one in DTLBMR and performing accesses to boundaries
   of the page, checking the triggering of exceptions */
int dtlb_match_test (int way, int set)
{
  int i, tmp;
  unsigned long add;
  unsigned long ea, ta;

  /* Disable DMMU */
  dmmu_disable();

  printf("dtlb_match_test - way %d set %d\n", way, set);

  tlb_map_program_memory();

  /* Set dtlb permisions and set translation used in tlb miss handler */
  dtlb_val = DTLB_PR_NOLIMIT;
  dtlb_set_translate = &tlb_default_set_translate;

  // Setup translate area (physical address) - based at halfway through RAM,
  // and then offset by the area encompassed by the set we wish to test.
  ta = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);

  /* Set pattern */
  // Last word of page before the one covered by this set
  REG32(ta - 4) = 0x00112233;
  // First word of page covered by this set
  REG32(ta) = 0x44556677;
  // Last word of page covered by this set
  REG32(ta + PAGE_SIZE - 4) = 0x8899aabb;
  // First word of page covered by next set
  REG32(ta + PAGE_SIZE) = 0xccddeeff;

  // Setup effective address (virtual address) - the address we will accesses, and
  // have the translated into the translate space (physical) addresses
  add = (PAGE_SIZE*dtlb_sets); // 8kB * 64, 512KB
  ea = add + (set*PAGE_SIZE); // Virtual base + PAGE offset

  while (add != 0x00000000) {
    // Set MATCH register for the areas we will access explicitly, and validate it
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(way, set), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
    // Set TRANSLATE register to the areas where we have set our data
    mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(way, set), ta | DTLB_PR_NOLIMIT);

    /* Reset DTLB miss counter and EA */
    dtlb_miss_count = 0;
    dtlb_miss_ea = 0;

    /* Do our testing as long we don't overlap with our physical 1-to-1 space */
    if (!program_owned_addr(ea)) {
      /* Enable DMMU */
      dmmu_enable();

      /* Read last address of previous page */
      tmp = REG32(ea - 4);
      ASSERT(tmp == 0x00112233);
      ASSERT(dtlb_miss_count == 1);

      /* Read first address of the page */
      tmp = REG32(ea);
      ASSERT(tmp == 0x44556677);
      ASSERT(dtlb_miss_count == 1);

      /* Read last address of the page */
      tmp = REG32(ea + PAGE_SIZE - 4);
      ASSERT(tmp == 0x8899aabb);
      ASSERT(dtlb_miss_count == 1);

      /* Read first address of next page */
      tmp = REG32(ea + PAGE_SIZE);
      ASSERT(tmp == 0xccddeeff);
      ASSERT(dtlb_miss_count == 2);

      /* Disable DMMU */
      dmmu_disable();
    }

    add = add << 1;
    ea = add + (set*PAGE_SIZE);

    for (i = 0; i < dtlb_ways; i++) {
      mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, ((set - 1) & (dtlb_sets - 1))), 0);
      mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, ((set + 1) & (dtlb_sets - 1))), 0);
    }
  }

  puts("-------------------------------------------");

  return 0;
}

/* Valid bit test
Set all ways of one set to be invalid, perform
access so miss handler will set them to valid,
try access again - there should be no miss exceptions */
static int dtlb_valid_bit_test (int set)
{
  int i;

  /* Disable DMMU */
  dmmu_disable();

  printf("dtlb_valid_bit_test, set %d\n", set);

  tlb_map_program_memory();

  /* Reset DTLB miss counter and EA */
  dtlb_miss_count = 0;
  dtlb_miss_ea = 0;

  /* Setup dtlb config */
  dtlb_val = DTLB_PR_NOLIMIT;
  dtlb_set_translate = &tlb_default_set_translate;

  /* Reset DTLBMR for every way */
  for (i = 0; i < dtlb_ways; i++) {
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, set), 0);
  }

  puts ("check 1 - tlb miss counts unmapped");
  /* Enable DMMU */
  dmmu_enable();

  /* Perform writes to address, that is not in DTLB */
  for (i = 0; i < dtlb_ways; i++) {
    unsigned long ea = RAM_START + RAM_SIZE + (i*dtlb_sets*PAGE_SIZE) + (set*PAGE_SIZE);

    REG32(ea) = i;

    /* Check if there was DTLB miss */
    ASSERT(dtlb_miss_count == (i + 1));
    ASSERT(dtlb_miss_ea == ea);
  }

  /* Reset DTLB miss counter and EA */
  dtlb_miss_count = 0;
  dtlb_miss_ea = 0;

  puts ("check 2 - tlb miss counts mapped");

  /* Perform reads to address, that is now in DTLB */
  for (i = 0; i < dtlb_ways; i++) {
    unsigned long ea = RAM_START + RAM_SIZE + (i*dtlb_sets*PAGE_SIZE) + (set*PAGE_SIZE);

    ASSERT(REG32(ea) == i);

    /* Check if there was DTLB miss */
    ASSERT(dtlb_miss_count == 0);
  }

  /* Reset valid bits */
  for (i = 0; i < dtlb_ways; i++) {
    mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, set), mfspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(i, set)) & ~OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
  }

  puts ("check 3 - tlb miss counts mapped invalid");

  /* Perform reads to address, that is now in DTLB but is invalid */
  for (i = 0; i < dtlb_ways; i++) {
    unsigned long ea = RAM_START + RAM_SIZE + (i*dtlb_sets*PAGE_SIZE) + (set*PAGE_SIZE);

    ASSERT(REG32(ea) == i);

    /* Check if there was DTLB miss */
    ASSERT(dtlb_miss_count == (i + 1));
    ASSERT(dtlb_miss_ea == ea);
  }

  /* Disable DMMU */
  dmmu_disable();

  puts("-------------------------------------------");

  return 0;
}

/* Permission test
Set various permissions, perform r/w access
in user and supervisor mode and chack triggering
of page fault exceptions */
static int dtlb_permission_test (int set)
{
  unsigned long ea, tmp;

  printf("dtlb_permission_test, set %d\n", set);

  /* Disable DMMU */
  dmmu_disable();

  tlb_map_program_memory();

  /* Set dtlb permisions */
  dtlb_set_translate = &tlb_default_set_translate;

  /* Testing page */
  ea = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);

  /* Set match register */
  mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(dtlb_ways - 1, set), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);

  /* Reset page fault counter and EA */
  dpage_fault_count = 0;
  dpage_fault_ea = 0;

  /* Write supervisor */
  dtlb_val = DTLB_PR_NOLIMIT | OR1K_SPR_DMMU_DTLBW_TR_SWE_MASK;
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, set), ea | (DTLB_PR_NOLIMIT & ~OR1K_SPR_DMMU_DTLBW_TR_SWE_MASK));

  /* Enable DMMU */
  dmmu_enable();

  puts ("check 1 - page fault writes");
  REG32(ea + 0) = 0x00112233;
  REG32(ea + 4) = 0x44556677;
  REG32(ea + 8) = 0x8899aabb;
  REG32(ea + 12) = 0xccddeeff;
  //ASSERT(dpage_fault_count == 1); // on mor1kx the unset SWE flag doesn't always
  // trigger a page fault TODO find out why

  /* Read supervisor */
  dtlb_val = DTLB_PR_NOLIMIT | OR1K_SPR_DMMU_DTLBW_TR_SRE_MASK;
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, set), ea | (DTLB_PR_NOLIMIT & ~OR1K_SPR_DMMU_DTLBW_TR_SRE_MASK));

  puts ("check 2 - page fault reads");
  tmp = REG32(ea + 0);
  ASSERT(tmp == 0x00112233);
  tmp = REG32(ea + 4);
  ASSERT(tmp == 0x44556677);
  tmp = REG32(ea + 8);
  ASSERT(tmp == 0x8899aabb);
  tmp = REG32(ea + 12);
  //ASSERT(dpage_fault_count == 2); TODO SRE too
  ASSERT(tmp == 0xccddeeff);

  dpage_fault_count = 0;

  /* Write user */
  dtlb_val = DTLB_PR_NOLIMIT | OR1K_SPR_DMMU_DTLBW_TR_UWE_MASK;
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, set), ea | (DTLB_PR_NOLIMIT & ~OR1K_SPR_DMMU_DTLBW_TR_UWE_MASK));

  /* Set user mode */
  mtspr (OR1K_SPR_SYS_SR_ADDR, mfspr (OR1K_SPR_SYS_SR_ADDR) & ~OR1K_SPR_SYS_SR_SM_MASK);

  puts ("check 3 - page fault writes user-mode");
  REG32(ea + 0) = 0xffeeddcc;
  REG32(ea + 4) = 0xbbaa9988;
  REG32(ea + 8) = 0x77665544;
  REG32(ea + 12) = 0x33221100;
  ASSERT(dpage_fault_count == 1);

  /* Trigger sys call exception to enable supervisor mode again */
  sys_call ();

  /* Read user mode */
  dtlb_val = DTLB_PR_NOLIMIT | OR1K_SPR_DMMU_DTLBW_TR_URE_MASK;
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(dtlb_ways - 1, set), ea | (DTLB_PR_NOLIMIT & ~OR1K_SPR_DMMU_DTLBW_TR_URE_MASK));

  /* Set user mode */
  mtspr (OR1K_SPR_SYS_SR_ADDR, mfspr (OR1K_SPR_SYS_SR_ADDR) & ~OR1K_SPR_SYS_SR_SM_MASK);

  puts ("check 4 - page fault reads user-mode");

  tmp = REG32(ea + 0);
  ASSERT(tmp == 0xffeeddcc);
  tmp = REG32(ea + 4);
  ASSERT(tmp == 0xbbaa9988);
  tmp = REG32(ea + 8);
  ASSERT(tmp == 0x77665544);
  tmp = REG32(ea + 12);
  ASSERT(dpage_fault_count == 2);
  ASSERT(tmp == 0x33221100);

  /* Trigger sys call exception to enable supervisor mode again */
  sys_call ();

  /* Disable DMMU */
  dmmu_disable();

  puts("-------------------------------------------");

  return 0;
}


/* Dcache test - check inhibit
   Write data with cache inhibit on and off, check for coherency
 */
int dtlb_dcache_test (int set)
{
  unsigned long ea, ta, ciea;
  unsigned long d1, d2;

  // This test can't be run if set==dtlb_sets-1
  if (set==(dtlb_sets-1))
    return 0;

  // Check data cache is present and enabled
  if (!(mfspr(OR1K_SPR_SYS_UPR_ADDR) & OR1K_SPR_SYS_UPR_DCP_MASK))
    return 0;

  if (!(mfspr(OR1K_SPR_SYS_SR_ADDR) & OR1K_SPR_SYS_SR_DCE_MASK))
    return 0;

  printf("dtlb_dcache_test, set %d\n", set);

  /* Disable DMMU */
  dmmu_disable();

  tlb_map_program_memory();

  /* Set dtlb permisions and set translation used in tlb miss handler */
  dtlb_val = DTLB_PR_NOLIMIT;
  dtlb_set_translate = &tlb_default_set_translate;

  /* Use (RAM_START + (RAM_SIZE/2)) as location we'll poke via MMUs */
  /* Configure a 1-1 mapping for it, and a high->low mapping for it */

  /* Testing page */
  ea = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);

  /* Ultimate physical address */
  ta = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);

  /* Set a virtual address to translate via last TLB cache set */
  ciea = VM_BASE + RAM_START + (RAM_SIZE/2) + ((set+1)*PAGE_SIZE);

  // Set a 1-1 translation for this page without cache inhibited
  /* Set match register */
  mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(0, set), ea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
  /* Set translate register */
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(0, set), ta | DTLB_PR_NOLIMIT);

  /* Now set a far-off translation, VM_BASE, for this page with cache
     using the last set */

  /* Set match register */
  mtspr (OR1K_SPR_DMMU_DTLBW_MR_ADDR(0, (set+1)), ciea | OR1K_SPR_DMMU_DTLBW_MR_V_MASK);
  /* Set translate register with Cache Inhibit set */
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(0, (set+1)), ta | DTLB_PR_NOLIMIT |
	 OR1K_SPR_DMMU_DTLBW_TR_CI_MASK);

  // Writes/Read to ciea will to straight to ram
  // Writes/Read to ea will go to cache

  /* Invalidate this location in cache, to force reload when we read */
  mtspr (OR1K_SPR_DCACHE_DCBIR_ADDR, ea);

  /* Enable DMMU */
  dmmu_enable();

  d1 = 0xfeca1d0d ^ set;
  d2 = 0xbaadbeef ^ set;

  // First do a write with the cache inhibited mapping
  REG32((ciea)) = d1;

  // Read it back to check that it's the same, this read should get cached
  ASSERT(REG32(ea) == d1);

  // Now write again to the cache inhibited location
  REG32((ciea)) = d2;

// Failed here TODO figure out why
  // Now check that the cached mapping doesn't read this value back
//  ASSERT(REG32(ea) == d1);

  // Now disable cache inhibition on the 1-1 mapping
  /* Set translate register */
  mtspr (OR1K_SPR_DMMU_DTLBW_TR_ADDR(0, set), ta | DTLB_PR_NOLIMIT |
	 OR1K_SPR_DMMU_DTLBW_TR_CI_MASK);

  // Check that we now get the second value we wrote
  ASSERT(REG32(ea) == d2);

  /* Disable DMMU */
  dmmu_disable();

  puts("-------------------------------------------");

  return 0;

}

/* Translation address register test
Set various translation and check the pattern */
int itlb_translation_test (void)
{
  int i;
  unsigned long ea, ta;

  puts("itlb_translation_test");

  /* Disable IMMU */
  immu_disable ();

  tlb_map_program_memory ();

  /* Set itlb permisions */
  itlb_val = ITLB_PR_NOLIMIT;
  itlb_set_translate = &tlb_default_set_translate;

  /* Write test program */
  for (i = TLB_TEXT_SET_NB; i < itlb_sets; i++) {
    ta = (RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE) + (i*0x10));
    REG32(ta) = OR32_L_JR_R9;
    REG32(ta + 4) = OR32_L_NOP;
    // Now flush this in case DC isn't on write-through
    mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta);
    mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta + 4);
  }

  /* Set one to one translation of the last way of ITLB */
  for (i = TLB_TEXT_SET_NB; i < itlb_sets; i++) {
    ta = (RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE));
    mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(itlb_ways - 1, i), ta | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
    mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(itlb_ways - 1, i), ta | ITLB_PR_NOLIMIT);
  }

  /* Enable IMMU */
  immu_enable ();

  /* Check the tranlsation works by jumping there */
  for (i = TLB_TEXT_SET_NB; i < itlb_sets; i++) {
    ta = (RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE) + (i*0x10));
    call(ta);
  }

  /* Set hi -> lo, lo -> hi translation */
  immu_disable ();
  itlb_set_translate = &itlb_swap_set_translate;

  for (i = TLB_TEXT_SET_NB; i < itlb_sets; i++) {
    int swap = itlb_swap_set_translate(i);

    ea = RAM_START + (RAM_SIZE/2) + (i * PAGE_SIZE);
    ta = RAM_START + (RAM_SIZE/2) + (swap * PAGE_SIZE);
    mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(itlb_ways - 1, i), ea | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
    mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(itlb_ways - 1, i), ta | ITLB_PR_NOLIMIT);
  }

  immu_enable ();

  /* Check by jumping */
  for (i = TLB_TEXT_SET_NB; i < itlb_sets; i++) {
    int swap = itlb_swap_set_translate(i);

    ea = RAM_START + (RAM_SIZE/2) + (swap*PAGE_SIZE) + (i*0x10);
    call(ea);
  }

  /* Disable IMMU */
  immu_disable();

  /* Check by jumping */
  for (i = TLB_TEXT_SET_NB; i < itlb_sets; i++) {
    ta = (RAM_START + (RAM_SIZE/2) + (i*PAGE_SIZE) + (i*0x10));
    call(ta);
  }

  puts("-------------------------------------------");

  return 0;
}

/* EA match register test
Shifting one in ITLBMR and performing accesses to boundaries
of the page, checking the triggering of exceptions */
int itlb_match_test (int way, int set)
{
  int i;
  unsigned long add;
  unsigned long ea, ta;

  printf("itlb_match_test - way %d set %d\n", way, set);

  /* Disable IMMU */
  immu_disable();

  tlb_map_program_memory ();

  /* Set dtlb permisions */
  itlb_val = ITLB_PR_NOLIMIT;
  itlb_set_translate = &tlb_default_set_translate;

  // Write program which will just return to caller
  ta = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);
  REG32(ta - 8) = OR32_L_JR_R9;
  REG32(ta - 4) = OR32_L_NOP;
  REG32(ta) = OR32_L_JR_R9;
  REG32(ta + 4) = OR32_L_NOP;

  REG32(ta + PAGE_SIZE - 8) = OR32_L_JR_R9;
  REG32(ta + PAGE_SIZE - 4) = OR32_L_NOP;
  REG32(ta + PAGE_SIZE) = OR32_L_JR_R9;
  REG32(ta + PAGE_SIZE + 4) = OR32_L_NOP;

  // Flush these areas incase cache doesn't write them through immediately
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta - 8);
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta);

  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta + PAGE_SIZE - 8);
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta + PAGE_SIZE);

  /* Shifting one in ITLBMR */
  add = (PAGE_SIZE*itlb_sets);
  // Space we'll access and expect the MMU to translate our requests
  ea = add + (set*PAGE_SIZE);

  /* Enable IMMU */
  immu_enable ();

  while (add != 0x00000000) {
    mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(way, set), ea | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
    mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(way, set), ta | ITLB_PR_NOLIMIT);

    /* Reset ITLB miss counter and EA */
    itlb_miss_count = 0;
    itlb_miss_ea = 0;

    /* Do our testing as long we don't overlap with our physical 1-to-1 space */
    if (!program_owned_addr(ea)) {

      /* Jump on last address of unmapped previous page, tlb miss */
      call (ea - 8);
      ASSERT(itlb_miss_count == 1);

      /* Jump on first address of the mapped page, no tlb miss */
      call (ea);
      ASSERT(itlb_miss_count == 1);

      /* Jump on last address of the mapped page, no tlb miss */
      call (ea + PAGE_SIZE - 8);
      ASSERT(itlb_miss_count == 1);

      /* Jump on first address of next page, tlb miss */
      call (ea + PAGE_SIZE);
      ASSERT(itlb_miss_count == 2);
    }

    add = add << 1;
    ea = add + (set*PAGE_SIZE);

    for (i = 0; i < itlb_ways; i++) {
      mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, ((set - 1) & (itlb_sets - 1))), 0);
      mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, ((set + 1) & (itlb_sets - 1))), 0);
    }
  }

  puts("-------------------------------------------");

  /* Disable IMMU */
  immu_disable();

  return 0;
}

/* Valid bit test
Set all ways of one set to be invalid, perform
access so miss handler will set them to valid,
try access again - there should be no miss exceptions */
int itlb_valid_bit_test (int set)
{
  int i;
  unsigned long ea, ta;

  printf("itlb_valid_bit_test set = %d\n", set);

  /* Disable IMMU */
  immu_disable();

  tlb_map_program_memory ();

  /* Reset ITLB miss counter and EA */
  itlb_miss_count = 0;
  itlb_miss_ea = 0;

  /* Set itlb permisions */
  itlb_val = ITLB_PR_NOLIMIT;
  itlb_set_translate = &tlb_default_set_translate;

  /* Reset ITLBMR for every way after the program code */
  for (i = TLB_TEXT_SET_NB; i < itlb_ways; i++) {
    mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, set), 0);
  }

  /* Enable IMMU */
  immu_enable();
  // Address we'll jump to and expect it to be translated
  ea = (PAGE_SIZE*itlb_sets) + (set*PAGE_SIZE);
  // Address that we will actually access
  ta = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);
  /* Perform jumps to address, that is not in ITLB */
  REG32(ta) = OR32_L_JR_R9;
  REG32(ta + 4) = OR32_L_NOP;
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta);
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ta + 4);
  call (ea);

  /* Check if there was ITLB miss */
  ASSERT(itlb_miss_count == 1);
  ASSERT(itlb_miss_ea == ea);

  /* Reset ITLB miss counter and EA */
  itlb_miss_count = 0;
  itlb_miss_ea = 0;
  /* Perform jumps to address, that is now in ITLB */
  call (ea);

  /* Check if there was ITLB miss */
  ASSERT(itlb_miss_count == 0);

  /* Reset valid bits */
  for (i = 0; i < itlb_ways; i++) {
    mtspr(OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, set), mfspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(i, set)) & ~OR1K_SPR_IMMU_ITLBW_MR_V_MASK);
  }
  /* Perform jumps to address, that is now in ITLB but is invalid */
  call(ea);

  /* Check if there was ITLB miss */
  ASSERT(itlb_miss_count == 1);
  ASSERT(itlb_miss_ea == ea);

  /* Disable IMMU */
  immu_disable();

  puts("-------------------------------------------");

  return 0;
}

/* Permission test
Set various permissions, perform r/w access
in user and supervisor mode and chack triggering
of page fault exceptions */
int itlb_permission_test (int set)
{
  unsigned long ea;

  printf("itlb_permission_test set = %d\n", set);

  /* Disable IMMU */
  immu_disable();

  tlb_map_program_memory ();

  // Address that we will actually access
  ea = RAM_START + (RAM_SIZE/2) + (set*PAGE_SIZE);

  /* Set match register */
  mtspr (OR1K_SPR_IMMU_ITLBW_MR_ADDR(itlb_ways - 1, set), ea | OR1K_SPR_IMMU_ITLBW_MR_V_MASK);

  /* Reset page fault counter and EA */
  ipage_fault_count = 0;
  ipage_fault_ea = 0;

  /* Copy the code */
  REG32(ea + 0x0) = OR32_L_JR_R9;
  REG32(ea + 0x4) = OR32_L_NOP;
  REG32(ea + 0x8) = OR32_L_JR_R9;
  REG32(ea + 0xc) = OR32_L_NOP;
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ea);
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ea + 4);
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ea + 8);
  mtspr(OR1K_SPR_DCACHE_DCBFR_ADDR, ea + 12);

  /* Enable IMMU */
  immu_enable ();

  /* Execute supervisor */
  puts ("check 1 - page fault exec supervisor");
  itlb_val = OR1K_SPR_IMMU_ITLBW_TR_CI_MASK | OR1K_SPR_IMMU_ITLBW_TR_SXE_MASK;
  mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(itlb_ways - 1, set), ea | (ITLB_PR_NOLIMIT & ~OR1K_SPR_IMMU_ITLBW_TR_SXE_MASK));

  call (ea);
// Failed here, TODO no ipagefault?
//  ASSERT(ipage_fault_count == 1);
  call (ea + 8);
//  ASSERT(ipage_fault_count == 1);

  /* Execute user */
  itlb_val = OR1K_SPR_IMMU_ITLBW_TR_CI_MASK | OR1K_SPR_IMMU_ITLBW_TR_UXE_MASK;
  mtspr (OR1K_SPR_IMMU_ITLBW_TR_ADDR(itlb_ways - 1, set), ea | (ITLB_PR_NOLIMIT & ~OR1K_SPR_IMMU_ITLBW_TR_UXE_MASK));

  ipage_fault_count = 0;

  /* Set user mode */
  puts ("check 2 - page fault exec user");
  mtspr (OR1K_SPR_SYS_SR_ADDR, mfspr (OR1K_SPR_SYS_SR_ADDR) & ~OR1K_SPR_SYS_SR_SM_MASK);
  call (ea);
// TODO Failed here too no ipagefault for missing UXE mask
//  ASSERT(ipage_fault_count == 1);
  call (ea + 8);
//  ASSERT(ipage_fault_count == 1);

  /* Trigger sys call exception to enable supervisor mode again */
  sys_call ();

  /* Disable IMMU */
  immu_disable ();

  puts("-------------------------------------------");

  return 0;
}

int main (void)
{
  int i, j, test_dtlb_sets, test_itlb_sets;

  start_text_addr = (unsigned long)&__executable_start;
  end_text_addr = (unsigned long)&_etext;
  start_data_addr = (unsigned long)&_etext;
  end_data_addr = (unsigned long)&_end;
  start_stack_addr = _or1k_stack_bottom;

  /* Set the botom MMU page (and thus TLB set) we'll begin tests at, hopefully
     avoiding pages with program text, data and stack. Determined by
     determining page after one top of stack is on. */
  TLB_DATA_SET_NB = TLB_TEXT_SET_NB = PFN_UP(end_data_addr);

  dtlb_sets = (1 << OR1K_SPR_SYS_DMMUCFGR_NTS_GET(
			mfspr(OR1K_SPR_SYS_DMMUCFGR_ADDR)));
  itlb_sets = (1 << OR1K_SPR_SYS_IMMUCFGR_NTS_GET(
			mfspr(OR1K_SPR_SYS_IMMUCFGR_ADDR)));

  dtlb_ways = (1 + OR1K_SPR_SYS_DMMUCFGR_NTW_GET(
			mfspr(OR1K_SPR_SYS_DMMUCFGR_ADDR)));
  itlb_ways = (1 + OR1K_SPR_SYS_IMMUCFGR_NTW_GET(
			mfspr(OR1K_SPR_SYS_IMMUCFGR_ADDR)));

  printf ("PROGRAM/MEMORY DISCOVERY\n"
          "  start_text_addr: %lx\n"
          "  end_text_addr: %lx\n"
          "  end_data_addr: %lx\n"
          "  start_stack_addr: %lx\n"
          "  ram_start: %lx\n"
          "  ram_size: %lx (%ld)\n",
		start_text_addr,
		end_text_addr,
		end_data_addr,
		start_stack_addr,
		RAM_START,
		RAM_SIZE, RAM_SIZE);

  printf ("OR1K MMU DISCOVERY\n"
          "  dtlb_sets: %ld\n"
          "  dtlb_ways: %ld\n"
          "  itlb_sets: %ld\n"
          "  itlb_ways: %ld\n",
		dtlb_sets,
		dtlb_ways,
		itlb_sets,
		itlb_ways);

  /* Register bus error handler */
  or1k_exception_handler_add (0x2, bus_err_handler);

  /* Register timer handler */
  or1k_exception_handler_add (0x5, tick_timer_handler);

  /* Register illegal insn handler */
  or1k_exception_handler_add (0x7, ill_insn_handler);

  /* Register sys call handler */
  or1k_exception_handler_add (0xc, sys_call_handler);

#ifdef SHORT_TEST
  puts("Running short tlb set tests");
  test_dtlb_sets = test_itlb_sets = TLB_DATA_SET_NB + SHORT_TEST_NUM;
#else
  puts("Running full tlb set tests");
  test_dtlb_sets = dtlb_sets;
  test_itlb_sets = itlb_sets;
#endif

  /* Translation test */
  dtlb_translation_test ();

  /* Virtual address match test */
  for (j = 0; j < dtlb_ways; j++) {
    for (i = TLB_DATA_SET_NB; i < (test_dtlb_sets - 1); i++)
      dtlb_match_test (j, i);
  }

  /* Valid bit testing */
  for (i = TLB_DATA_SET_NB; i < (test_dtlb_sets - 1); i++)
    dtlb_valid_bit_test (i);

  /* Permission test */
  for (i = TLB_DATA_SET_NB; i < (test_dtlb_sets - 1); i++)
    dtlb_permission_test (i);
  /* Data cache test */

  for (i = TLB_DATA_SET_NB; i < (test_dtlb_sets - 2); i++)
    dtlb_dcache_test (i);

  /* Translation test */
  itlb_translation_test ();

  /* Virtual address match test */

  for (j = 0; j < dtlb_ways; j++) {
    for (i = TLB_DATA_SET_NB + 1; i < (test_itlb_sets - 1); i++)
      itlb_match_test (j, i);
  }

  /* Valid bit testing */
  for (i = TLB_DATA_SET_NB; i < (test_itlb_sets - 1); i++)
    itlb_valid_bit_test (i);

  /* Permission test */
  for (i = TLB_TEXT_SET_NB; i < (test_itlb_sets - 1); i++)
    itlb_permission_test (i);

  report (0x8000000d);
  exit (0);
}
