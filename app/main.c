#include "cm7.h"
#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <dwt.h>
#include <systick.h>
#include <uart.h>
#include <context.h>

#define STACK_SIZE 256
static uint32_t stack_a[STACK_SIZE]     __attribute__((aligned(8)));
static uint32_t stack_b[STACK_SIZE]     __attribute__((aligned(8)));

uint32_t* tasks_sp[2];
volatile uint32_t current;

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

static void task_exit() {
  while(1) {
    uart_printf("Task exited\r\n");
  }
}

static uint32_t* setup_initial_stack(uint32_t* stack_top, void (*entry)(void)) {
  uint32_t* sp = stack_top;
  *(--sp) = 0x01000000;   //xPSR
  *(--sp) = (uint32_t)entry & ~1u;  //PC
  *(--sp) = (uint32_t)task_exit;   // LR
  *(--sp) = 0;  //r12
  *(--sp) = 0;  //r3
  *(--sp) = 0;  //r2
  *(--sp) = 0;  //r1
  *(--sp) = 0;  //r0

  *(--sp) = 0;  // r11
  *(--sp) = 0;  // r10
  *(--sp) = 0;  // r09
  *(--sp) = 0;  // r08
  *(--sp) = 0;  // r07
  *(--sp) = 0;  // r06
  *(--sp) = 0;  // r05
  *(--sp) = 0;  // r04
  return sp;
}

static void yield() {
  SCB_ICSR = ICSR_PENDSVSET;
  __asm volatile ("dsb");
  __asm volatile ("isb");
}

static void task_a(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_16;
    n++;
    uart_printf("A:%d\r\n", n);
    GPIO6_DR ^= SCOPE_PIN_16;
    yield();
  }
}

static void task_b(void) {
  int n = 0;

  while(1) {
    GPIO6_DR ^= SCOPE_PIN_17;
    n++;
    uart_printf("B:%d\r\n", n);
    GPIO6_DR ^= SCOPE_PIN_17;
    yield();
  }
}

static void start_schedule() {
  SHPR3 = ((SHPR3 & ~(0xFu << 16)) | (0xFFu << 16));
  current = 0;
  __asm volatile ("svc 0");
}

int main(void) {
  switch_clock_600mhz();
  init_uart();
  init_scope_pins();
  uart_printf("Cronus online\r\n");
  //uart_printf("dec=%d hex=%X neg=%d char=%c str=%s pct=%%\r\n", 42, 0xABCD, -7, 'Q', "hello");

  tasks_sp[0] = setup_initial_stack(&stack_a[STACK_SIZE], task_a);
  tasks_sp[1] = setup_initial_stack(&stack_b[STACK_SIZE], task_b);

  start_schedule();

  while (1) {
    __asm volatile ("nop");
  }
}
