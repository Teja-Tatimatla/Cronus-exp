#include <imxrt1062.h>
#include <board.h>
#include <cm7.h>
#include <stdint.h>

#define RXBUF_SIZE 256u
static volatile uint8_t rx_buff[RXBUF_SIZE];
static volatile uint32_t rx_head = 0;
static volatile uint32_t rx_tail = 0;

void init_uart(void) {
  /* CMM_CSCDR1[UART_CLK_PODF] is [5:0] 000000 is dividied by 1
   * PLL3 is 480MHz in the clock tree this gets divided by 6 and then
   * the signal passs through CMM_CSCDR1[UART_CLK_PODF] before
   * reaching UART_CLK_ROOT
   */
  CCM_CSCDR1 &= ~0x7F;
  CCM_CCGR3 |= (3u << 6);
  IOMUXC_MUX_AD_B0_03 = IOMUXC_MUX_AD_B0_02 = 2;
  IOMUXC_PAD_AD_B0_03 = IOMUXC_PAD_AD_B0_02 = 0x10B0;
  IOMUXC_LPUART6_RX_DAISY_REGISTER |= IOMUXC_LPUART6_SET_DAISY_AD_B0_03;
  /* [baud rate = baud clock / ((OSR + 1) * SBR)] for baud rate - 115200
   * 115200 = 80 MHz / ((15 + 1) * SBR)
   * SBR = 80000000/115200 * 16
   * SBR = 43.4 -> nearest integer 43.
   */
  LPUART6_BAUD = (15u << 24) /* OSR */ | (43u & 0x1FFF); // -> SRB
  LPUART6_CTRL = LPUART6_CTRL_TE | LPUART6_CTRL_RE | LPUART6_CTRL_RIE;
  NVIC_ISER(LPUART6_IRQ / 32u) = (1u << (LPUART6_IRQ % 32u));
}


void uart_putc(char c) {
  while(!(LPUART6_STAT & LPUART6_STAT_TDRE)) {
    __asm volatile ("nop");
  }
  LPUART6_DATA = (uint8_t)c;
}

void uart_puts(const char *s) {
  while(*s) {
    if(*s == '\n') {
      uart_putc('\r');
    }
    uart_putc(*s++);
  }
}

/* Consumer */
int uart_getc(void) {
  if(rx_head == rx_tail) {
    return -1;
  }
  uint8_t c = rx_buff[rx_tail];
  rx_tail = (rx_tail + 1u) & (RXBUF_SIZE - 1u);
  return c;
}

/* Producer */
void LPUART6_Handler(void) {
  while(LPUART6_STAT & LPUART6_STAT_RDRF) {
    uint8_t c = (uint8_t)LPUART6_DATA;
    uint32_t next = (rx_head + 1u) & (RXBUF_SIZE - 1u);
    if(next != rx_tail) {
      rx_buff[rx_head] = c;
      rx_head = next;
    }
  }
}
