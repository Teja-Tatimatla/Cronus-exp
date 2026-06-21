#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <dwt.h>
#include <systick.h>
#include <uart.h>
#include <context.h>
#include <cronus/tasks.h>
#include <cronus/critical.h>
#include <gpt.h>

#define STACK_SIZE 256
uint32_t stack_a[STACK_SIZE]     __attribute__((aligned(8)));
uint32_t stack_b[STACK_SIZE]     __attribute__((aligned(8)));
uint32_t stack_c[STACK_SIZE]     __attribute__((aligned(8)));

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
  uint32_t last_wake = get_tick_count();
  while(1) {
    GPIO6_DR ^= SCOPE_PIN_14;
    n++;
    enter_critical();
    uart_printf("A:%d\r\n", n);
    exit_critical();
    GPIO6_DR ^= SCOPE_PIN_14;
    task_delay_until(&last_wake, 200);
  }
}

static void task_b(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_16;
    n++;
    enter_critical();
    uart_printf("B:%d\r\n", n);
    exit_critical();
    GPIO6_DR ^= SCOPE_PIN_16;
    task_delay(100);
  }
}

static void task_c(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_17;
    n++;
    enter_critical();
    uart_printf("C:%d\r\n", n);
    exit_critical();
    GPIO6_DR ^= SCOPE_PIN_17;
    task_delay(50);
  }
}

int main(void) {
  switch_clock_600mhz();
  init_uart();
  init_scope_pins();
  uart_printf("Cronus online\r\n");
  //uart_printf("dec=%d hex=%X neg=%d char=%c str=%s pct=%%\r\n", 42, 0xABCD, -7, 'Q', "hello");

  create_task(stack_a, task_a, STACK_SIZE, 9);
  create_task(stack_b, task_b, STACK_SIZE, 8);
  create_task(stack_c, task_c, STACK_SIZE, 7);

  init_sys_tick();
  gpt_init();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
