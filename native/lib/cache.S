#include <or1k-sprs.h>
#include <or1k-asm.h>

	/* Cache init. To be called during init ONLY */

	.global	_cache_init
        .type	_cache_init,@function

_cache_init:
	/* Instruction cache enable */
	/* Check if IC present and skip enabling otherwise */
	l.mfspr r3,r0,OR1K_SPR_SYS_UPR_ADDR
	l.andi  r4,r3,OR1K_SPR_SYS_UPR_ICP_MASK
	l.sfeq  r4,r0
	OR1K_DELAYED_NOP(OR1K_INST(l.bf    .L8))

	/* Disable IC */
	l.mfspr r6,r0,OR1K_SPR_SYS_SR_ADDR
	l.addi  r5,r0,-1
	l.xori  r5,r5,OR1K_SPR_SYS_SR_ICE_MASK
	l.and   r5,r6,r5
	l.mtspr r0,r5,OR1K_SPR_SYS_SR_ADDR

	/* Establish cache block size
	If BS=0, 16;
	If BS=1, 32;
	r14 contain block size
	*/
	l.mfspr r3,r0,OR1K_SPR_SYS_ICCFGR_ADDR
	l.andi  r4,r3,OR1K_SPR_SYS_ICCFGR_CBS_MASK
	l.srli  r5,r4,7
	l.ori   r6,r0,16
	l.sll   r14,r6,r5

	/* Establish number of cache sets
	r7 contains number of cache sets
	r5 contains log(# of cache sets)
	*/
	l.andi  r4,r3,OR1K_SPR_SYS_ICCFGR_NCS_MASK
	l.srli  r5,r4,3
	l.ori   r6,r0,1
	l.sll   r7,r6,r5

	/* Invalidate IC */
	l.addi  r6,r0,0
	l.sll   r5,r14,r5

.L7:
	l.mtspr r0,r6,OR1K_SPR_ICACHE_ICBIR_ADDR
	l.sfne  r6,r5
	OR1K_DELAYED(
	OR1K_INST(l.add   r6,r6,r14),
	OR1K_INST(l.bf    .L7)
	)

	/* Enable IC */
	l.mfspr r6,r0,OR1K_SPR_SYS_SR_ADDR
	l.ori   r6,r6,OR1K_SPR_SYS_SR_ICE_MASK
	l.mtspr r0,r6,OR1K_SPR_SYS_SR_ADDR
	l.nop
	l.nop
	l.nop
	l.nop
	l.nop
	l.nop
	l.nop
	l.nop

.L8:
	/* Data cache enable */
        /* Check if DC present and skip enabling otherwise */
        l.mfspr r3,r0,OR1K_SPR_SYS_UPR_ADDR
        l.andi  r4,r3,OR1K_SPR_SYS_UPR_DCP_MASK
        l.sfeq  r4,r0
        OR1K_DELAYED_NOP(l.bf    .L10)
        /* Disable DC */
        l.mfspr r6,r0,OR1K_SPR_SYS_SR_ADDR
        l.addi  r5,r0,-1
        l.xori  r5,r5,OR1K_SPR_SYS_SR_DCE_MASK
	l.and   r5,r6,r5
        l.mtspr r0,r5,OR1K_SPR_SYS_SR_ADDR
        /* Establish cache block size
           If BS=0, 16;
           If BS=1, 32;
           r14 contain block size
        */
        l.mfspr r3,r0,OR1K_SPR_SYS_DCCFGR_ADDR
        l.andi  r4,r3,OR1K_SPR_SYS_DCCFGR_CBS_MASK
        l.srli  r5,r4,7
        l.ori   r6,r0,16
        l.sll   r14,r6,r5
        /* Establish number of cache sets
           r7 contains number of cache sets
           r5 contains log(# of cache sets)
        */
	l.andi  r4,r3,OR1K_SPR_SYS_DCCFGR_NCS_MASK
	l.srli  r5,r4,3
        l.ori   r6,r0,1
        l.sll   r7,r6,r5
        /* Invalidate DC */
        l.addi  r6,r0,0
        l.sll   r5,r14,r5
.L9:
        l.mtspr r0,r6,OR1K_SPR_DCACHE_DCBIR_ADDR
        l.sfne  r6,r5
        OR1K_DELAYED(
	OR1K_INST(l.add   r6,r6,r14),
	OR1K_INST(l.bf    .L9)
	)
        /* Enable DC */
        l.mfspr r6,r0,OR1K_SPR_SYS_SR_ADDR
        l.ori   r6,r6,OR1K_SPR_SYS_SR_DCE_MASK
        l.mtspr r0,r6,OR1K_SPR_SYS_SR_ADDR

.L10:
	/* Return */
	OR1K_DELAYED_NOP(OR1K_INST(l.jr	r9))
