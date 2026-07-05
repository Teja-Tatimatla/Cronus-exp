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
#include <mpu.h>
#include <fpu.h>
#include <cronus/sem.h>

#define STACK_SIZE 256
uint32_t stack_a[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t stack_b[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t stack_c[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t task_monitor[STACK_SIZE] __attribute__((aligned(32)));

/*
static void init_led(void) {
  CCM_CCGR0 |= (3u << 30);
  IOMUXC_MUX_B0_03 = 5;
  IOMUXC_PAD_B0_03 = 0x10B0;
  GPIO2_GDIR |= LED_PIN;
}
*/

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

/*
static void task_a(void) {
  int n = 0;
  float f = 0.1;

  uint32_t last_wake = get_tick_count();
  while(1) {
    enter_critical();
    GPIO6_DR ^= SCOPE_PIN_14;
    n++;
    f += 0.1;
    uart_printf("A:%d - %f\r\n", n, f);
    GPIO6_DR ^= SCOPE_PIN_14;
    exit_critical();
    task_delay_until(&last_wake, 200);
  }
}

static void task_b(void) {
  int n = 0;
  float f = 0.001;

  while(1) {
    enter_critical();
    GPIO6_DR ^= SCOPE_PIN_16;
    n++;
    f += 0.001;
    uart_printf("B:%d - %f\r\n", n, f);
    GPIO6_DR ^= SCOPE_PIN_16;
    exit_critical();
    task_delay(100);
  }
}

static void task_c(void) {
  int n = 0;
  float f = 0.00001;

  while(1) {
    enter_critical();
    GPIO6_DR ^= SCOPE_PIN_17;
    n++;
    f += 0.00001;
    uart_printf("C:%d - %f\r\n", n, f);
    GPIO6_DR ^= SCOPE_PIN_17;
    exit_critical();
    task_delay(50);
  }
}

static void monitor_task(void) {
  while (1) {
    enter_critical();
    task_reporter();
    exit_critical();
    task_delay(1000);     // report once a second
  }
}
*/

static semaphore_t sem;

uint32_t stack_prod[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t stack_cons[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t stack_probe[STACK_SIZE]     __attribute__((aligned(32)));

static void producer(void) {
  while(1) {
    task_delay(300);
    sem_give(&sem);
  }
}

static void consumer(void) {
  for(;;) {
    int got = sem_take(&sem, SEM_WAIT_FOREVER);
    GPIO6_DR ^= SCOPE_PIN_16;

    uart_printf("consumer woke (got=%d)\r\n", got);
  }
}

static void timeout_probe(void) {
  for(;;) {
    int got = sem_take(&sem, 100);
    uart_printf("probe %s\r\n", got ? "signalled" : "timed out");
  }
}

int main(void) {
  switch_clock_600mhz();
  init_fpu();
  init_uart();
  init_scope_pins();
  uart_printf("Cronus online\r\n");
  //uart_printf("dec=%d hex=%X neg=%d char=%c str=%s pct=%%\r\n", 42, 0xABCD, -7, 'Q', "hello");

  /*
  create_task(stack_a, task_a, STACK_SIZE, 9, "Task A");
  create_task(stack_b, task_b, STACK_SIZE, 8, "Task B");
  create_task(stack_c, task_c, STACK_SIZE, 7, "Task C");
  create_task(task_monitor, monitor_task, STACK_SIZE, 1, "monitor");
  */

  sem_init(&sem, 0, 10);

  create_task(stack_prod,  producer,      STACK_SIZE, 5, "prod");
  create_task(stack_cons,  consumer,      STACK_SIZE, 8, "cons");
  create_task(stack_probe, timeout_probe, STACK_SIZE, 4, "probe");

  init_sys_tick();
  gpt_init();
  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
