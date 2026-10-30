#include <stdint.h>

#include <cm7.h>
#include <cache.h>

void icache_enable(void) {
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
  SCB_ICIALLU = 0;
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
  SCB_CCR |= SCB_CCR_IC;
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
}

void dcache_enable(void) {
  SCB_CSSELR = 0;
  __asm volatile ("dsb" ::: "memory");
  uint32_t sets = CCSIDR_SETS(SCB_CCSIDR);

  for (uint32_t s = 0; s <= sets; s++) {
    for(uint32_t w = 0; w < DCACHE_WAYS; w++) {
      SCB_DCISW = (w << DCISW_WAY_POS) | (s << DCISW_SET_POS);
    }
  }

  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
  SCB_CCR |= SCB_CCR_DC;
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
}
