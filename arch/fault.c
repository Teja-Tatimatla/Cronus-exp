#include <cronus/tasks.h>
#include <stdint.h>
#include <uart.h>

#define CFSR              (*(volatile uint32_t *)0xE000ED28)
#define HFSR              (*(volatile uint32_t *)0xE000ED2C)
#define MMFAR             (*(volatile uint32_t *)0xE000ED34)
#define BFAR              (*(volatile uint32_t *)0xE000ED38)

#define MMFSR_MMARVALID   (1u << 7)
#define MMFSR_MLSPERR     (1u << 5)
#define MMFSR_MSTKERR     (1u << 4)
#define MMFSR_MUNSTKERR   (1u << 3)
#define MMFSR_DACCVIOL    (1u << 1)
#define MMFSR_IACCVIOL    (1u << 0)

extern tcb_t* curr_task;


void mem_fault_report(uint32_t* frame);

void hard_fault_report(uint32_t* frame, uint32_t exc_return) {
  uint32_t mmfsr = CFSR & 0xFFu;
  /* overflow that escalated: guard fired (DACCVIOL) + stacking re-faulted (MSTKERR) */
  if((mmfsr & MMFSR_DACCVIOL) && (mmfsr & MMFSR_MSTKERR) && (mmfsr & MMFSR_MMARVALID)) {
    mem_fault_report(frame);     /* never returns — it halts */
  }

  uart_printf("\r\n*** HARDFAULT ***\r\n");
  uart_printf("PC=%p  LR=%p  xPSR=%p\r\n", (void *)frame[6], (void *)frame[5], (void *)frame[7]);
  uart_printf("CFSR=%p  HFSR=%p\r\n", (void *)CFSR, (void *)HFSR);
  uart_printf("MMFAR=%p  BFAR=%p  EXC_RET=%p\r\n", (void *)MMFAR, (void *)BFAR, (void *)exc_return);

  for (;;) {
    __asm volatile ("nop");
  }
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

void mem_fault_report(uint32_t* frame) {
  uint32_t cfsr = CFSR;
  uint32_t mmfar = MMFAR;
  uint32_t mmfsr = cfsr & 0xFFu;

  uint32_t addr = (mmfsr & MMFSR_MMARVALID) ? mmfar : 0xFFFFFFFFu;

  uint32_t pc = frame[6];

  uart_printf("\r\n*** MemManage: task @ %p task name: %s\r\n", (void *)curr_task, curr_task->name);
  uart_printf("    faulting PC   = %p\r\n", (void *)pc);
  uart_printf("    faulting addr = %p  %s\r\n", (void *)addr,
                  (mmfsr & MMFSR_MMARVALID) ? "(MMFAR valid)" : "(no addr)");

  uart_printf("    MMFSR = %p  [%s,%s,%s,%s,%s]\r\n", (void *)mmfsr,
              (mmfsr & MMFSR_MLSPERR) ? "MLSPERR" : "",
              (mmfsr & MMFSR_MSTKERR) ? "MSTKERR" : "",
              (mmfsr & MMFSR_MUNSTKERR) ? "MUNSTKERR" : "",
              (mmfsr & MMFSR_DACCVIOL) ? "DACCVIOL" : "",
              (mmfsr & MMFSR_IACCVIOL) ? "IACCVIOL" : "");

  CFSR = cfsr;          /* write-1-to-clear so the next fault reports cleanly  */

  for (;;) {
    __asm volatile("nop");
  }
}

__attribute__((naked))
void MemManage_Handler(void) {
  __asm volatile (
    "tst lr, #4             \n"
    "ite eq                 \n"
    "mrseq r0, msp          \n"
    "mrsne r0, psp          \n"
    "b mem_fault_report     \n"
  );
}
