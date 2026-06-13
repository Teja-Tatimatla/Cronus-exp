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
