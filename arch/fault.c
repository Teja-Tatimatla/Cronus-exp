#include <stdint.h>
#include <uart.h>

void hard_fault_report(uint32_t *frame, uint32_t exc_return) {
  uint32_t cfsr  = *(volatile uint32_t *)0xE000ED28;
  uint32_t hfsr  = *(volatile uint32_t *)0xE000ED2C;
  uint32_t mmfar = *(volatile uint32_t *)0xE000ED34;
  uint32_t bfar  = *(volatile uint32_t *)0xE000ED38;

  uart_printf("\r\n*** HARDFAULT ***\r\n");
  uart_printf("PC=%x  LR=%x  xPSR=%x\r\n", frame[6], frame[5], frame[7]);
  uart_printf("CFSR=%x  HFSR=%x\r\n", cfsr, hfsr);
  uart_printf("MMFAR=%x  BFAR=%x  EXC_RET=%x\r\n", mmfar, bfar, exc_return);
  while (1) { }
}

__attribute__((naked))
void HardFault_Handler(void) {
  __asm volatile (
    "tst lr, #4           \n"   /* EXC_RETURN bit2: which stack was active */
    "ite eq               \n"
    "mrseq r0, msp        \n"
    "mrsne r0, psp        \n"   /* r0 = faulting stack frame */
    "mov  r1, lr          \n"   /* r1 = EXC_RETURN */
    "b    hard_fault_report \n"
  );
}
