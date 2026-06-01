#include <stdint.h>

#include <cronus/config.h>
#include <cm7.h>
#include <board.h>
#include <imxrt1062.h>

static volatile uint32_t tick_count = 0;

void init_sys_tick(void) {
  SYST_RVR = (CORE_HZ / TICK_HZ) - 1u;
  SYST_CVR = 0;
  SHPR3 = (SHPR3 & 0x00FFFFFF) | (0xFFu << 24);
  SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

uint32_t get_tick_count(void) {
  return tick_count;
}

void SysTick_Handler(void) {
  GPIO6_DR |= SCOPE_PIN_14;
  tick_count++;
  GPIO6_DR &= ~SCOPE_PIN_16;
}
