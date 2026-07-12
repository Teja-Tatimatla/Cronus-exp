#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <uart.h>
#include <cronus/tasks.h>
#include <cronus/critical.h>
#include <gpt.h>
#include <mpu.h>
#include <fpu.h>
#include <cronus/sem.h>
#include <lpm.h>

#define STACK_SIZE 256
uint32_t stack_a[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t stack_b[STACK_SIZE]      __attribute__((aligned(32)));
uint32_t task_monitor[STACK_SIZE] __attribute__((aligned(32)));

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


static void monitor_task(void) {
  while (1) {
    enter_critical();
    task_reporter();
    exit_critical();
    task_delay(1000);     // report once a second
  }
}

#define TEST 3

#define BINARY 1

#if TEST == 2

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
    notify_take(1, TASK_WAIT_FOREVER);
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
    notify_wait(0, 0, &v, TASK_WAIT_FOREVER);
    GPIO6_DR ^= SCOPE_PIN_16;
    uart_printf("reader: got %u\r\n", v);
  }
}

#endif /* TEST */

int main(void) {
  switch_clock_600mhz();
  init_fpu();
  init_uart();
  init_scope_pins();
  uart_printf("Cronus online\r\n");

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

  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
