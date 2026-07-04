#include <stdint.h>


#define CPACR           (*(volatile uint32_t *)0xE000ED88)
#define FPCCR           (*(volatile uint32_t *)0xE000EF34)

#define FPCCR_LSPEN     (1u << 30)    // Enabled by default.


/* arch/fpu.c — enable the Cortex-M7 FPU (CP10/CP11) before the scheduler starts. */

void init_fpu(void) {
  CPACR |= (0xFu << 20);  /* CP10 & CP11 = full access -> FPU enabled */
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
}
