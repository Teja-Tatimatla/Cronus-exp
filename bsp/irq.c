#include <stdint.h>

#include <cronus/irq.h>
#include <cm7.h>

typedef struct {
  irq_handler_fn handler;
  void*          arg;
} irq_slot_t;

static volatile irq_slot_t irq_table[NUM_IRQS];

void Default_Handler(void);

void driver_irq_enable(uint32_t irq, uint8_t priority) {
  if(irq >= NUM_IRQS) {
    return;
  }

  NVIC_IPR(irq) = priority;
  GPC_IMR(irq / 32u) &= ~(1u << (irq % 32u));
  NVIC_ISER(irq / 32u) = (1u << (irq % 32u));
}

void driver_irq_disable(uint32_t irq) {
  if(irq >= NUM_IRQS) {
    return;
  }

  NVIC_ICER(irq / 32u) = (1u << (irq % 32u));
  GPC_IMR(irq / 32u) |= (1u << (irq % 32u));
}

int driver_irq_connect(uint32_t irq, irq_handler_fn handler, void *arg) {
  if(irq >= NUM_IRQS) {
    return 0;
  }

  irq_table[irq].arg = arg;
  __asm volatile ("dmb" ::: "memory");
  irq_table[irq].handler = handler;
  return 1;
}

__itcm void irq_dispatch(void) {
  uint32_t ipsr;
  __asm volatile ("mrs %0, ipsr" : "=r"(ipsr));
  uint32_t irq = ipsr - 16u;
  if(irq < NUM_IRQS) {
    irq_handler_fn handler = irq_table[irq].handler;
    void* arg = irq_table[irq].arg;
    if(handler) {
      handler(arg);
      return;
    }
  }
  Default_Handler();
}
