#include <dwt.h>
#include <cm7.h>
#include <cronus/config.h>

void init_dwt(void) {
  DEMCR |= DEMCR_TRCENA;
  DWT_CYCCNT = 0;
  DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

void delay_cycles(uint32_t cycles) {
  uint32_t start = DWT_CYCCNT;
  while ((DWT_CYCCNT - start) < cycles) {
    __asm volatile ("nop");
  }
}

void delay_us(uint32_t us) {
  delay_cycles(us * (CORE_HZ / 1000000u));
}

void delay_ms(uint32_t ms) {
  delay_cycles(ms * (CORE_HZ / 1000u));
}
