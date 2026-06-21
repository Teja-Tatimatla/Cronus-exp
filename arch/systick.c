#include <stdint.h>

#include <cronus/config.h>
#include <cm7.h>
#include <board.h>
#include <imxrt1062.h>
#include <cronus/tasks.h>

static volatile uint32_t tick_count = 0;
static volatile uint8_t in_tickless = 0u;

void init_sys_tick(void) {
  SYST_RVR = (SYSTICK_REF_HZ / TICK_HZ) - 1u;
  SYST_CVR = 0;
  SHPR3 = (SHPR3 & 0x00FFFFFF) | (0xFFu << 24);
  SYST_CSR = SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

uint32_t get_tick_count(void) {
  return tick_count;
}

void systick_set_tickless(uint8_t on) {
  in_tickless = on;
}

void advance_tick_count(uint32_t ticks) {
  tick_count += ticks;
}

void SysTick_Handler(void) {
  if(in_tickless) {
    (void)SYST_CSR;
    return;
  }

  tick_count++;
  kernel_tick(tick_count);
  SCB_ICSR = ICSR_PENDSVSET;
}
