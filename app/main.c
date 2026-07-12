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
#include <lpm.h>
#include <cm7.h>

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
*/

static void monitor_task(void) {
  while (1) {
    enter_critical();
    task_reporter();
    exit_critical();
    task_delay(1000);     // report once a second
  }
}

#define TEST 1

#define BINARY 1

#if TEST == 1

static semaphore_t sem;
static volatile uint32_t fifo_depth = 0;

static void producer(void) {
  for(;;) {
    task_delay(1000);
    enter_critical();
      fifo_depth += 3u;
    exit_critical();
    uart_printf("\r\nproducer: 3 gives (fifo=3)\r\n");
    sem_give(&sem);
    sem_give(&sem);
    sem_give(&sem);
  }
}

static void consumer(void) {
  for(;;) {
    int got = sem_take(&sem, SEM_WAIT_FOREVER);
    GPIO6_DR ^= SCOPE_PIN_16;
    enter_critical();
      uint32_t drained = fifo_depth;
      fifo_depth = 0;
    exit_critical();
    uart_printf("consumer: woke (got=%d), drained %u\r\n", got, drained);
  }
}

static void hb(void) {
  for(;;) { GPIO6_DR ^= SCOPE_PIN_17; task_delay(100); }
}

#elif TEST == 2

static task_handle_t h_worker;

static void notifier(void) {
  for(;;) {
    task_delay(1000);
    uart_printf("notifier: notify_give\r\n");
    notify_give(h_worker);
  }
}

static void worker(void) {
  for(;;) {
    notify_take(1, WAIT_FOREVER);
    GPIO6_DR ^= SCOPE_PIN_16;
    uart_printf("worker: woke on notification\r\n");
  }
}


#elif TEST == 3

static task_handle_t h_reader;

static void writer(void) {
  uint32_t k = 0;
  for(;;) {
    task_delay(1000);
    k++;
    uart_printf("writer: send %u\r\n", k);
    notify(h_reader, k, NOTIFY_SET_VALUE_OVERWRITE);
  }
}

static void reader(void) {
  for(;;) {
    uint32_t v = 0;
    notify_wait(0, 0, &v, WAIT_FOREVER);
    GPIO6_DR ^= SCOPE_PIN_16;
    uart_printf("reader: got %u\r\n", v);
  }
}

#endif /* TEST */

#define GPC_ISR(n) (*(volatile uint32_t *)(0x400F4018u + 4u*(n)))   /* verified: 0x18 */

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

  #if TEST == 1
    #if BINARY
      sem_init_binary(&sem, 0);
      uart_printf("binary semaphore (max=1)\r\n");
    #else
      sem_init(&sem, 0, 10);
      uart_printf("counting semaphore (max=10)\r\n");
    #endif
    create_task(stack_a, producer, STACK_SIZE, 6, "prod");
    create_task(stack_b, consumer, STACK_SIZE, 5, "cons");
    //create_task(stack_c, hb, STACK_SIZE, 7, "hb");

  #elif TEST == 2
    h_worker = create_task(stack_b, worker,   STACK_SIZE, 7, "work");
                 create_task(stack_a, notifier, STACK_SIZE, 6, "notif");

  #elif TEST == 3
    h_reader = create_task(stack_b, reader, STACK_SIZE, 7, "read");
               create_task(stack_a, writer, STACK_SIZE, 6, "write");
  #endif

    create_task(task_monitor, monitor_task, STACK_SIZE, 1, "monitor");

  gpt_init();
  lpm_init();

  uart_printf("CGPR=%X\r\n", (unsigned)CCM_CGPR);
  uart_printf("IMR: %X %X %X %X\r\n", (unsigned)GPC_IMR(0), (unsigned)GPC_IMR(1),
                                      (unsigned)GPC_IMR(2), (unsigned)GPC_IMR(3));
  uart_printf("ISR: %X %X %X %X\r\n", (unsigned)GPC_ISR(0), (unsigned)GPC_ISR(1),
                                      (unsigned)GPC_ISR(2), (unsigned)GPC_ISR(3));

  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
