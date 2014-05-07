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

/* For writing into SPR. */
void mtspr(unsigned long spr, unsigned long value)
{
  asm("l.mtspr\t\t%0,%1,0": : "r" (spr), "r" (value));
}

/* For reading SPR. */
unsigned long mfspr(unsigned long spr)
{
  unsigned long value;
  asm("l.mfspr\t\t%0,%1,0" : "=r" (value) : "r" (spr));
  return value;
}

/* Print out a character via simulator */
void sim_putc(unsigned char c)
{
  asm("l.addi\tr3,%0,0": :"r" (c));
  asm("l.nop %0": :"K" (NOP_PUTC));
}

/* print long */
void report(unsigned long value)
{
  asm("l.addi\tr3,%0,0": :"r" (value));
  asm("l.nop %0": :"K" (NOP_REPORT));
}

/* Loops/exits simulation */
void exit (int i)
{
  asm("l.add r3,r0,%0": : "r" (i));
  asm("l.nop %0": :"K" (NOP_EXIT));
  while (1);
}
