/* bsp/lpm.h */
#pragma once

void lpm_init(void);         /* once at boot: assert GINT, park it GPC-masked   */
void lpm_wait_config(void);  /* immediately before wfi: ERR050143-bracketed WAIT */
void lpm_run_config(void);   /* immediately after wfi: back to RUN               */
