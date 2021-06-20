/* support.h Support headers for testing Or1ksim.

   Copyright (C) 1999 Damjan Lampret, lampret@opencores.org
   Copyright (C) 2010 Embecosm Limited

   Contributor Damjan Lampret <lampret@opencores.org>
   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of OpenRISC 1000 Architectural Simulator.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http:  www.gnu.org/licenses/>.  */

/* ----------------------------------------------------------------------------
   This code is commented throughout for use with Doxygen.
   --------------------------------------------------------------------------*/

/* This file should is included in each C test. It calls main () function and
   add support for basic functions */

#ifndef SUPPORT_H
#define SUPPORT_H

#include <stdarg.h>
#include <stddef.h>
#include <limits.h>

#define NOP_NOP          0x0000      /* Normal nop instruction */
#define NOP_EXIT         0x0001      /* End of simulation */
#define NOP_REPORT       0x0002      /* Simple report */
/*#define NOP_PRINTF       0x0003       Simprintf instruction (obsolete)*/
#define NOP_PUTC         0x0004      /* JPB: Simputc instruction */
#define NOP_CNT_RESET    0x0005	     /* Reset statistics counters */
#define NOP_GET_TICKS    0x0006	     /* JPB: Get # ticks running */
#define NOP_GET_PS       0x0007      /* JPB: Get picosecs/cycle */
#define NOP_TRACE_ON     0x0008      /* Turn on tracing */
#define NOP_TRACE_OFF    0x0009      /* Turn off tracing */
#define NOP_RANDOM       0x000a      /* Return 4 random bytes */
#define NOP_OR1KSIM      0x000b      /* Return non-zero if this is Or1ksim */

/* Start function */
extern void reset ();

/* Return a value by making a syscall */
extern void exit (int i) __attribute__ ((__noreturn__));

/* Version of putchar that works with Or1ksim */
extern int putchar (int c);

/* Read the simulator timer */
extern unsigned long int read_timer ();

/* For writing into SPR. */
inline void mtspr(unsigned long spr, unsigned long value)
{
  asm volatile ("l.mtspr\t\t%0,%1,0; l.nop; l.nop": : "r" (spr), "r" (value));
}

/* For reading SPR. */
inline unsigned long mfspr(unsigned long spr)
{
  unsigned long value;
  asm volatile ("l.mfspr\t\t%0,%1,0" : "=r" (value) : "r" (spr));
  return value;
}

/* print long */
inline void report(unsigned long value)
{
  asm("l.addi\tr3,%0,0": :"r" (value) : "r3" );
  asm("l.nop %0": :"K" (NOP_REPORT));
}

#endif
