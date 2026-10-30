#include <stdint.h>

#include <cronus/irq.h>
#include <cm7.h>
/* ARM exception/interrupt vector table (the .isr_vector the core dispatches
   through). Distinct from the i.MX boot IVT in bsp/boot.c.
   NOTE: weak aliases must reference a symbol defined in THIS translation unit,
   so default_handler lives here alongside them. */

extern uint32_t _estack;
extern void Reset_Trampoline(void);          /* arch/startup.c */
void Default_Handler(void);

void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"), used))
const void *vector_table[] = {
  (void *)&_estack,       /* [0]  initial MSP */
  Reset_Trampoline,       /* [1]  reset        */
  NMI_Handler,            /* [2]               */
  HardFault_Handler,      /* [3]               */
  MemManage_Handler,
  0, 0, 0, 0, 0, 0,
  SVC_Handler,            /* [11] */
  0, 0,
  PendSV_Handler,         /* [14] */
  SysTick_Handler,        /* [15] */
  [16 ... 175] = irq_dispatch,
};

__itcm void Default_Handler(void) {
  while (1) {
    __asm volatile ("nop");
  }
}
