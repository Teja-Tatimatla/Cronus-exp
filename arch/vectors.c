#include <stdint.h>
/* ARM exception/interrupt vector table (the .isr_vector the core dispatches
   through). Distinct from the i.MX boot IVT in bsp/boot.c.
   NOTE: weak aliases must reference a symbol defined in THIS translation unit,
   so default_handler lives here alongside them. */

extern uint32_t _estack;
void reset_handler(void);          /* arch/startup.c */
void default_handler(void);

void NMI_Handler(void)       __attribute__((weak, alias("default_handler")));
void HardFault_Handler(void) __attribute__((weak, alias("default_handler")));
void SVC_Handler(void)       __attribute__((weak, alias("default_handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("default_handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("default_handler")));

__attribute__((section(".isr_vector"), used))
const void *vector_table[] = {
  (void *)&_estack,   /* [0]  initial MSP */
  reset_handler,      /* [1]  reset        */
  NMI_Handler,        /* [2]               */
  HardFault_Handler,  /* [3]               */
  0, 0, 0, 0, 0, 0, 0,
  SVC_Handler,        /* [11] */
  0, 0,
  PendSV_Handler,     /* [14] */
  SysTick_Handler,    /* [15] */
};

void default_handler(void) {
  while (1);
}
