/* bsp/lpm.h */
#pragma once

#define CCM_CGPR                  (*(volatile uint32_t *)0x400FC064u)
#define CCM_CGPR_INT_MEM_CLK_LPM  (1u << 17)

void lpm_init(void);         /* once at boot: assert GINT, park it GPC-masked   */
void lpm_wait_config(void);  /* immediately before wfi: ERR050143-bracketed WAIT */
void lpm_run_config(void);   /* immediately after wfi: back to RUN               */
