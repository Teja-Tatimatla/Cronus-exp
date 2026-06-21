#include <stdint.h>

#include <cm7.h>

#define MAX_SYSCALL_PRIO 0x20u
volatile uint32_t critical_nesting = 0;

void enter_critical(void) {
  arch_set_basepri(MAX_SYSCALL_PRIO);
  critical_nesting++;
}

void exit_critical(void) {
  if(--critical_nesting == 0) {
    arch_set_basepri(0);
  }
}

uint32_t enter_critical_isr(void) {
  uint32_t prev_basepri = arch_get_basepri();
  arch_set_basepri(MAX_SYSCALL_PRIO);
  return prev_basepri;
}

void exit_critical_isr(uint32_t prev_basepri) {
  arch_set_basepri(prev_basepri);
}

void enter_critical_primask(void) {
  arch_disable_irq();
}

void exit_critical_primask(void) {
  arch_enable_irq();
}
