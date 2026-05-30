#include <clock.h>
#include <imxrt1062.h>

void switch_clock_600mhz(void) {
  CCM_CBCDR |= PERIPH_CLK_SEL;
  /* DIV_SELECT = first 7 bits (valid 54-108); 100 -> PLL 1200 -> /2 -> 600 MHz */
  CCM_ANALOG_PLL_ARM = ((CCM_ANALOG_PLL_ARM & ~0x7Fu) | (100u & 0x7Fu));
  /* Wait for PLL lock (bit 31, set by hardware once phase-locked). */
  while ((CCM_ANALOG_PLL_ARM & (1u << 31)) == 0) {
    __asm volatile ("nop");
  }
  CCM_CBCDR &= ~PERIPH_CLK_SEL;
}
