#include <stdio.h>
#include <stdint.h>
#include <or1k-sprs.h>
#include <or1k-support.h>

#define INTGEN_BASE         0xe1000000
#define INTGEN_IRQ                  19

static void write_intgen_reg(uint32_t reg, int value)
{
	*((uint8_t *)INTGEN_BASE + reg) = value;
}

void intgen_isr(void *data)
{
	int *valPtr = data;
	/* Write 0 to address 1 clears interrupt */
	write_intgen_reg(1, 0);
	printf("got irq\r\n");

        *valPtr = 42;
}

int main(void)
{
        int val = 0;
        int count = 0;

	/* Install interrupt handler and enable interrupt */
	or1k_interrupt_handler_add(INTGEN_IRQ, intgen_isr, &val);
	or1k_interrupt_enable(INTGEN_IRQ);
	or1k_mtspr(OR1K_SPR_SYS_SR_ADDR, or1k_mfspr(OR1K_SPR_SYS_SR_ADDR) | OR1K_SPR_SYS_SR_IEE_MASK);
	/* Generate an interrupt */
	write_intgen_reg(0, 10);

	while ((val == 0) && (count++ < 10)) ;

	printf ("got val %d\n", val);
        if (val) {
		return 0;
        } else {
		return 1;
	}
}
