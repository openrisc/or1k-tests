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

/* Start function */
extern void reset ();

/* Return a value by making a syscall */
extern void exit (int i) __attribute__ ((__noreturn__));

/* Version of putchar that works with Or1ksim */
extern int putchar (int c);

/* Prints out a value */
extern void report (unsigned long int value);

/* Read the simulator timer */
extern unsigned long int read_timer ();

/* For writing into SPR. */
extern void  mtspr (unsigned long int spr,
		    unsigned long int value);

/* For reading SPR. */
extern unsigned long int mfspr (unsigned long int spr);

#endif
