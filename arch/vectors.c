#include <stdint.h>
/* ARM exception/interrupt vector table (the .isr_vector the core dispatches
   through). Distinct from the i.MX boot IVT in bsp/boot.c.
   NOTE: weak aliases must reference a symbol defined in THIS translation unit,
   so default_handler lives here alongside them. */

extern uint32_t _estack;
void Reset_Handler(void);          /* arch/startup.c */
void Default_Handler(void);

void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void LPUART6_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void GPT1_Handler(void)      __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
const void *vector_table[] = {
  (void *)&_estack,       /* [0]  initial MSP */
  Reset_Handler,          /* [1]  reset        */
  NMI_Handler,            /* [2]               */
  HardFault_Handler,      /* [3]               */
  0, 0, 0, 0, 0, 0, 0,
  SVC_Handler,            /* [11] */
  0, 0,
  PendSV_Handler,         /* [14] */
  SysTick_Handler,        /* [15] */
  0, 0, 0, 0, 0, 0, 0, 0, /*  [16 - 23] */
  0, 0, 0, 0, 0, 0, 0, 0, /*  [24 - 31] */
  0, 0, 0, 0, 0, 0, 0, 0, /*  [32 - 39] */
  0,                      /*  [40] */
  LPUART6_Handler,        /*  [41] */
  [116] = GPT1_Handler    /*  IRQ 100 (GPT1) -> vector 116 */
};

void Default_Handler(void) {
  while (1);
}
