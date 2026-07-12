#include <stdint.h>
#include <imxrt1062.h>
#include <cm7.h>                       /* GPC_IMR(n) */
#include <lpm.h>

/* True WAIT-mode entry for tickless idle - hardware-verified July 2026.
 * Three pieces, all mandatory; each was independently fatal when missing:
 *   1. CLPCR bit-set: bare LPM=01 wedges on SCU/L2CC/HS handshakes this
 *      single-M7 die can never answer (CCM/GPC IP inherited from i.MX 6).
 *      MASK_* and BYPASS_* skip them. (NXP lpm.c LPM_SetWaitModeConfig)
 *   2. ERR050143: SoC can enter LPM before the core reaches WFI. GINT keeps
 *      a GPC-visible interrupt pending across the CLPCR write (bracket
 *      below); never NVIC-enabled, so no handler ever runs.
 *   3. CGPR[17] INT_MEM_CLK_LPM: keep TCM clocks alive when an interrupt is
 *      pending at LPM entry - GINT makes that EVERY entry, and this kernel
 *      fetches from ITCM. Without it: wake into unclocked instruction
 *      memory, hard hang. (NXP lpm.c LPM_Init line 161)
 * Config is applied per-sleep around the wfi (lpm_wait_config/
 * lpm_run_config), never latched at boot.                                   */

#define CLPCR_LPM_MASK            0x3u
#define CLPCR_LPM_WAIT            0x1u
#define CLPCR_ARM_CLK_DIS_ON_LPM  (1u << 5)
#define CLPCR_STBY_COUNT_MASK     (0x3u << 9)
#define CLPCR_BYPASS_LPM_HS1      (1u << 19)
#define CLPCR_BYPASS_LPM_HS0      (1u << 21)
#define CLPCR_MASK_SCU_IDLE       (1u << 26)   /* no SCU on this die: don't wait */
#define CLPCR_MASK_L2CC_IDLE      (1u << 27)   /* no L2CC either                 */

#define GPR_IRQ                   41u          /* vector 57 - NOT your LPUART6!  */
#define IOMUXC_GPR_GPR1           (*(volatile uint32_t *)0x400AC004u)
#define IOMUXC_GPR_GPR1_GINT      (1u << 12)

void lpm_init(void) {
  /* Keep ARM-platform memory (TCM) clocks alive when entering LPM with an
   * interrupt pending. MANDATORY companion to GINT: GINT makes an interrupt
   * pending at EVERY entry, and Cronus fetches from ITCM — waking into gated
   * TCM clocks is a hard hang. (NXP lpm.c LPM_Init, line 161; CGPR[17].)   */
  CCM_CGPR |= CCM_CGPR_INT_MEM_CLK_LPM;

  IOMUXC_GPR_GPR1 |= IOMUXC_GPR_GPR1_GINT;              /* ERR050143 step 1 */
  GPC_IMR(GPR_IRQ / 32u) |= (1u << (GPR_IRQ % 32u));    /* parked masked    */
}

void lpm_wait_config(void) {
  GPC_IMR(GPR_IRQ / 32u) &= ~(1u << (GPR_IRQ % 32u));  /* step 2: unmask   */
  uint32_t clpcr = CCM_CLPCR & ~(CLPCR_LPM_MASK | CLPCR_ARM_CLK_DIS_ON_LPM);
  CCM_CLPCR = clpcr | CLPCR_LPM_WAIT
            | CLPCR_MASK_SCU_IDLE | CLPCR_MASK_L2CC_IDLE
            | CLPCR_ARM_CLK_DIS_ON_LPM | CLPCR_STBY_COUNT_MASK
            | CLPCR_BYPASS_LPM_HS0 | CLPCR_BYPASS_LPM_HS1;
  GPC_IMR(GPR_IRQ / 32u) |= (1u << (GPR_IRQ % 32u));   /* step 3: re-mask  */
}

void lpm_run_config(void) {
  CCM_CLPCR &= ~(CLPCR_LPM_MASK | CLPCR_ARM_CLK_DIS_ON_LPM);
}
