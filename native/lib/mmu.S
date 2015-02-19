#include <or1k-sprs.h>
#include <or1k-asm.h>
        /* MMU enable functions */
.global lo_dmmu_en
lo_dmmu_en:
        l.mfspr r3,r0,OR1K_SPR_SYS_SR_ADDR
        l.ori   r3,r3,OR1K_SPR_SYS_SR_DME_MASK
        l.mtspr r0,r3,OR1K_SPR_SYS_ESR_BASE
        l.mtspr r0,r9,OR1K_SPR_SYS_EPCR_BASE
        l.rfe


.global lo_immu_en
lo_immu_en:
        l.mfspr r3,r0,OR1K_SPR_SYS_SR_ADDR
        l.ori   r3,r3,OR1K_SPR_SYS_SR_IME_MASK
        l.mtspr r0,r3,OR1K_SPR_SYS_ESR_BASE
        l.mtspr r0,r9,OR1K_SPR_SYS_EPCR_BASE
        l.rfe
