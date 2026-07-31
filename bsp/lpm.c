#include <stdint.h>
#include <imxrt1062.h>
#include <cm7.h>                       /* GPC_IMR(n) */
#include <lpm.h>
#include <gpt.h>
#include <board.h>


#define CCM_CGPR                  (*(volatile uint32_t *)0x400FC064u)
#define CCM_CGPR_INT_MEM_CLK_LPM  (1u << 17)

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

  /* Wake-source whitelist (NXP discipline): mask everything, then unmask
   * exactly the registered wake sources. Currently: GPT1 only.
  */
  GPC_IMR(0) = 0xFFFFFFFFu;                          /* mask ALL GPC wake sources   */
  GPC_IMR(1) = 0xFFFFFFFFu;
  GPC_IMR(2) = 0xFFFFFFFFu;
  GPC_IMR(3) = 0xFFFFFFFFu;
  GPC_IMR(GPT1_IRQ / 32u) &= ~(1u << (GPT1_IRQ % 32u));         /* GPT1 back in — the wake */
  GPC_IMR(LPUART6_IRQ / 32u) &= ~(1u << (LPUART6_IRQ % 32u));   /* LPUART6 back in — the wake */

  IOMUXC_GPR_GPR1 |= IOMUXC_GPR_GPR1_GINT;              /* ERR050143 step 1 */
}

void lpm_wait_config(void) {
  GPC_IMR(GPR_IRQ / 32u) &= ~(1u << (GPR_IRQ % 32u));
  uint32_t clpcr = CCM_CLPCR & ~(CLPCR_LPM_MASK | CLPCR_ARM_CLK_DIS_ON_LPM);
  CCM_CLPCR = clpcr | CLPCR_LPM_WAIT
            | CLPCR_MASK_SCU_IDLE | CLPCR_MASK_L2CC_IDLE
            | CLPCR_STBY_COUNT_MASK
            | CLPCR_BYPASS_LPM_HS0 | CLPCR_BYPASS_LPM_HS1;

  /* ARM_CLK_DIS_ON_LPM deliberately ABSENT: gating the ARM clock domain from
     * 600 MHz overdrive wedges the exit (bisected 2026-07-11; NXP's reference
     * downshifts to LowSpeedRun before ever setting it). Deferred: downshift
     * subproject / EVKB investigation.
  */
  GPC_IMR(GPR_IRQ / 32u) |= (1u << (GPR_IRQ % 32u));    /* bracket close */
}

void lpm_run_config(void) {
  CCM_CLPCR &= ~(CLPCR_LPM_MASK | CLPCR_ARM_CLK_DIS_ON_LPM);
}
