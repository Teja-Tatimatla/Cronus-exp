#include <stdint.h>
#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <dwt.h>
#include <systick.h>
#include <uart.h>

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
  init_uart();
  uart_puts("Cronus online\n");
  while (1) {
    int c = uart_getc();
    if(c >= 0) {
      uart_putc((char)c);
    }
  }
}
