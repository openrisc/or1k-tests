#include <or1k-asm.h>
.section .text
	.global stack
	.align	4
	.space	0x80000
stack:
