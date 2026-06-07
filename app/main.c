#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <dwt.h>
#include <systick.h>
#include <uart.h>
#include <context.h>
#include <cronus/tasks.h>

#define STACK_SIZE 256
uint32_t stack_a[STACK_SIZE]     __attribute__((aligned(8)));
uint32_t stack_b[STACK_SIZE]     __attribute__((aligned(8)));
uint32_t stack_c[STACK_SIZE]     __attribute__((aligned(8)));


static inline void critical_enter(void) {
  __asm volatile ("cpsid i" ::: "memory");
}

static inline void critical_exit(void) {
  __asm volatile ("cpsie i" ::: "memory");
}

static void init_led(void) {
  CCM_CCGR0 |= (3u << 30);
  IOMUXC_MUX_B0_03 = 5;
  IOMUXC_PAD_B0_03 = 0x10B0;
  GPIO2_GDIR |= LED_PIN;
}

static void init_scope_pins(void) {
  /* Pin 14 */
  IOMUXC_GPR_GPR26 |= SCOPE_PIN_14;
  IOMUXC_MUX_AD_B1_02 = 5;
  IOMUXC_PAD_AD_B1_02 = 0x10B0;
  GPIO6_GDIR |= SCOPE_PIN_14;

  /* Pin 16 */
  IOMUXC_GPR_GPR26 |= SCOPE_PIN_16;
  IOMUXC_MUX_AD_B1_07 = 5;
  IOMUXC_PAD_AD_B1_07 = 0x10B0;
  GPIO6_GDIR |= SCOPE_PIN_16;

  /* Pin 17 */
  IOMUXC_GPR_GPR26 |= SCOPE_PIN_17;
  IOMUXC_MUX_AD_B1_06 = 5;
  IOMUXC_PAD_AD_B1_06 = 0x10B0;
  GPIO6_GDIR |= SCOPE_PIN_17;
}

static void task_a(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_14;
    n++;
    critical_enter();
    uart_printf("A:%d\r\n", n);
    critical_exit();
    GPIO6_DR ^= SCOPE_PIN_14;
  }
}

static void task_b(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_16;
    n++;
    critical_enter();
    uart_printf("B:%d\r\n", n);
    critical_exit();
    GPIO6_DR ^= SCOPE_PIN_16;
  }
}

static void task_c(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_17;
    n++;
    critical_enter();
    uart_printf("C:%d\r\n", n);
    critical_exit();
    GPIO6_DR ^= SCOPE_PIN_17;
  }
}

int main(void) {
  switch_clock_600mhz();
  init_uart();
  init_scope_pins();
  uart_printf("Cronus online\r\n");
  //uart_printf("dec=%d hex=%X neg=%d char=%c str=%s pct=%%\r\n", 42, 0xABCD, -7, 'Q', "hello");

  create_task(stack_a, task_a, STACK_SIZE, 9);
  create_task(stack_b, task_b, STACK_SIZE, 9);
  create_task(stack_c, task_c, STACK_SIZE, 9);

  init_sys_tick();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
