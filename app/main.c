#include <stdint.h>
#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <dwt.h>
#include <systick.h>

static void init_led(void) {
  CCM_CCGR0 |= (3u << 30);
  IOMUXC_MUX_B0_03 = 5;
  IOMUXC_PAD_B0_03 = 0x10B0;
  GPIO2_GDIR |= LED_PIN;
}

static void init_scope_pin(void) {
  IOMUXC_GPR_GRP26 |= SCOPE_PIN;
  IOMUXC_MUX_AD_B1_02 = 5;
  IOMUXC_PAD_AD_B1_02 = 0x10B0;
  GPIO6_GDIR |= SCOPE_PIN;
}

int main(void) {
  switch_clock_600mhz();
  init_led();
  init_scope_pin();
  init_sys_tick();

  uint32_t last_tick = 0;
  while (1) {
    if((get_tick_count() - last_tick) >= 500u) {
      GPIO2_DR ^= LED_PIN;
      last_tick += 500u;
    }
  }
}
