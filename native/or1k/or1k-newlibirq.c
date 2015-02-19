#include <stdio.h>
#include <stdint.h>
#include <or1k-sprs.h>
#include <or1k-support.h>
//#include "board.h"
#define INTGEN_BASE         0xe1000000
#define INTGEN_IRQ                  19

extern void or1k_interrupt_handler(void);

static void write_intgen_reg(uint32_t reg, int value)
{
	*((uint8_t *)INTGEN_BASE + reg) = value;
}

void intgen_isr(void *data)
{
	/* Write 0 to address 1 clears interrupt */
	write_intgen_reg(1, 0);
	printf("got irq\r\n");
}

int main(void)
{
	/* Install interrupt handler and enable interrupt */
	or1k_exception_handler_add(0x8, or1k_interrupt_handler);
	or1k_interrupt_handler_add(INTGEN_IRQ, intgen_isr);
	or1k_interrupt_enable(INTGEN_IRQ);
	or1k_mtspr(OR1K_SPR_SYS_SR_ADDR, or1k_mfspr(OR1K_SPR_SYS_SR_ADDR) | OR1K_SPR_SYS_SR_IEE_MASK);
	/* Generate an interrupt */
	write_intgen_reg(0, 1);

	/* Exit simulation */
	asm("l.nop 0x1");

	return 0;
}
