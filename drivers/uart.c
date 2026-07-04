#include <stdint.h>
#include <stdarg.h>

#include <imxrt1062.h>
#include <board.h>
#include <cm7.h>
#include <sys/_intsup.h>

#define RXBUF_SIZE 256u
static volatile uint8_t rx_buff[RXBUF_SIZE];
static volatile uint32_t rx_head = 0;
static volatile uint32_t rx_tail = 0;

static void uart_printf_int(int v);
static void uart_printf_uint(unsigned int v, unsigned int base, int upper);
static void uart_printf_double(double v, unsigned int precision);


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
   * SBR = 80000000/(115200 * 16)
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

void uart_printf(const char* fmt_str, ...) {
  va_list ap;
  va_start(ap, fmt_str);

  while(*fmt_str) {
    if(*fmt_str != '%') {
      uart_putc(*fmt_str++);
      continue;
    }
    fmt_str++;        // -> when *fmt_str is % skip it

    switch (*fmt_str) {
      case 'd':
      case 'i':
        uart_printf_int(va_arg(ap, int));
        break;

      case 'u':
        uart_printf_uint(va_arg(ap, unsigned), 10, 0);
        break;

      case 'f':
      case 'F':
        uart_printf_double(va_arg(ap, double), 6);
        break;

      case 'x':
        uart_printf_uint(va_arg(ap, unsigned), 16, 0);
        break;

      case 'X':
        uart_printf_uint(va_arg(ap, unsigned), 16, 1);
        break;

      case 'p':
        uart_putc('0');
        uart_putc('x');
        uart_printf_uint((unsigned)(uintptr_t)va_arg(ap, void *), 16, 0);
        break;

      case 'c':
        uart_putc((char)va_arg(ap, int));
        break;

      case 's': {
        const unsigned char* s = va_arg(ap, unsigned char *);
        while(*s) {
          uart_putc(*s++);
        }
        break;
      }

      case '%':
        uart_putc('%');
        break;

      default:
        uart_putc('%');
        if(*fmt_str) {
          uart_putc(*fmt_str);
        }
        break;
    }

    if(*fmt_str) {
      fmt_str++;
    }
  }
  va_end(ap);
}

static void uart_printf_int(int v) {
  unsigned int u;
  if(v < 0) {
    uart_putc('-');
    /*
     * Worked for v = -7:
     (unsigned)(-7) = -7 + 2³² = 4294967289 (0xFFFFFFF9)
     -(4294967289) mod 2³² = 2³² − 4294967289 = 7 ✓ — the magnitude.

     * And the case that breaks -v, v = INT_MIN:
     (unsigned)INT_MIN = -2147483648 + 2³² = 2147483648 (0x80000000)
     -(2147483648) mod 2³² = 2³² − 2147483648 = 2147483648 ✓
     */
    u = -(unsigned int)v;
  } else {
    u = (unsigned int)v;
  }
  uart_printf_uint(u, 10, 0);
}

static void uart_printf_uint(unsigned int v, unsigned int base, int upper) {
  char buff[32];
  const char* lt = upper ? "0123456789ABCDEF" : "0123456789abcdef";

  /*
   * The do { buf[i++] = d[v % base]; v /= base; } while (v) loop
   * pulls digits out least-significant first: v % base is the lowest digit,
   * d[...] maps it to a char, v /= base strips it, repeat until v is 0.
   * It's a do-while, not a while, for one reason: if v starts at 0,
   * a while (v) body never runs and you'd print nothing — do-while
   * runs once first, so 0 correctly prints "0" with no special case.
   * For 123 you end with buf = ['3','2','1'], reversed, because you
   * generated low digit first.
   */
  int i = 0;
  do {
    buff[i++] = lt[v % base];
    v /= base;
  } while (v);

  while(i) {
    uart_putc(buff[--i]);
  }
}

static void uart_printf_double(double v, unsigned int precision) {
  /* varargs promote float->double, so this is always handed a double (see the call site). */
  if(__builtin_isnan(v)) {    /* prints unsigned, like glibc */
    uart_putc('n');
    uart_putc('a');
    uart_putc('n');
    return;
  }

  if(__builtin_signbit(v)) {  /* correct for -inf and -0.0 too */
    uart_putc('-');
    v = -v;
  }

  if(__builtin_isinf(v)) {
    uart_putc('i');
    uart_putc('n');
    uart_putc('f');
    return;
  }

  /* split into integer and fractional parts */
  uint32_t ipart = (uint32_t)v;     /* NOTE: integer part must fit in 32 bits */
  double frac = v - (double)ipart;

  /* scale the fraction to an integer, round-to-nearest */
  uint32_t scale = 1u;
  for (unsigned int i = 0; i < precision; i++) {    /* precision <= 9 for u32 */
    scale *= 10u;
  }

  uint32_t fpart =  (uint32_t)(frac * (double)scale + 0.5);

  /* rounding can carry into the integer part: 1.9999996 -> "2.000000" */
  if(fpart >= scale) {
    fpart -= scale;
    ipart += 1u;
  }

  /* reuse your integer path */
  uart_printf_uint(ipart, 10, 0);

  if(precision > 0) {
    uart_putc('.');
    /* print fpart as exactly `precision` digits, zero-padded on the left */
    char buff[16];
    int i = 0;
    uint32_t f = fpart;
    do {
      buff[i++] = (char)('0' + (int)(f % 10u));
      f /= 10u;
    } while (f);

    while(i < (int)precision) {
      buff[i++] = '0';
    }

    while(i) {
      uart_putc(buff[--i]);
    }
  }
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
