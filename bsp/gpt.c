#include <stdint.h>

#include <gpt.h>
#include <imxrt1062.h>
#include <cm7.h>
#include <cronus/irq.h>

static void GPT1_isr(void* arg);

void gpt_init(void) {
  /* 1. Ungate GPT1 (bus + serial clock). CCGR1 GPT1_BUS=[21:20], SERIAL=[23:22]. */
  CCM_CCGR1 |= (3u << 20) | (3u << 22);

  /* 2. Reset, then: 24 MHz crystal, no prescale, free-run, alive in WAIT/STOP. */
  GPT1_CR = GPT_CR_SWR;
  while(GPT1_CR & GPT_CR_SWR) {
    __asm volatile ("nop");
  }
  GPT1_PR = 0;
  GPT1_CR = GPT_CR_EN_24M | GPT_CR_CLKSRC_24M | GPT_CR_ENMOD | GPT_CR_WAITEN
          | GPT_CR_DOZEEN | GPT_CR_STOPEN | GPT_CR_FRR;

  /* 3. NVIC enable (so the handler runs after wake). */
  //NVIC_ISER(GPT1_IRQ / 32u) = (1u << (GPT1_IRQ % 32u));

  /* 4. Set Interrupt Priority
   * Priority 0x80 >= MAX_SYSCALL_PRIO (0x20), so BASEPRI critical sections mask it.
   */
  //NVIC_IPR(GPT1_IRQ) = 0x80u;

  /* 5. Unmask GPT1 in the GPC so it can WAKE the core from WAIT. This is the
       thing SysTick can't do. IRQ 100 -> IMR[100/32]=IMR4, bit 100%32=4, clear-to-enable
       (verified against NXP fsl_gpc.c with FSL_FEATURE_GPC_HAS_IRQ_0_31=1). */
  //GPC_IMR(GPT1_IRQ / 32u) &= ~(1u << (GPT1_IRQ % 32u));

  /* First connect and then enable else an iterrupt may fire and isr isn't connected */
  driver_irq_connect(GPT1_IRQ, GPT1_isr, 0);
  driver_irq_enable(GPT1_IRQ, 0x80);
}

__itcm void gpt_start(uint32_t clocks) {
  GPT1_CR &= ~GPT_CR_EN;
  GPT1_SR = GPT_SR_OF1;  /* clear stale compare flag */
  GPT1_OCR1 = clocks;
  GPT1_IR = GPT_IR_OF1IE;
  GPT1_CR |= GPT_CR_EN;
}

__itcm uint32_t gpt_stop_return_elapsed(void) {
  uint32_t cnt = GPT1_CNT;
  GPT1_CR &= ~GPT_CR_EN;
  GPT1_SR = GPT_SR_OF1;
  GPT1_IR = 0;
  return cnt;
}

static __itcm void GPT1_isr(void* arg) {
  (void)arg;
  GPT1_SR = GPT_SR_OF1; /* acknowledge */
}
