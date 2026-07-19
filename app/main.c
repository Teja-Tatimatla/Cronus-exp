#include "cronus/config.h"
#include "cronus/mutex.h"
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
#include <dwt.h>
#include <cronus/sem.h>
#include <lpm.h>

#define DEMO 1
#define DEADLOCK_ORDERED 0

#define STACK_SIZE 256
static uint32_t stack_a[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_b[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_c[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_d[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_mon[STACK_SIZE] __attribute__((aligned(32)));

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

#if DEMO == 1

static mutex_t m;

static void low_task(void) {
  while (1) {
    mutex_lock(&m, MUTEX_WAIT_FOREVER);
    GPIO6_DR |= SCOPE_PIN_16;
    uart_printf("low: in CS at prio %u\r\n", (unsigned)task_priority_self());
    delay_ms(40);
    uart_printf("low: leaving CS at prio %u\r\n", (unsigned)task_priority_self());
    GPIO6_DR &= ~SCOPE_PIN_16;
    mutex_unlock(&m);
    task_delay(160);
  }
}

static void med_task(void) {
  task_delay(15);
  while (1) {
    GPIO6_DR |= SCOPE_PIN_14;
    delay_ms(120);
    GPIO6_DR &= ~SCOPE_PIN_14;
    task_delay(80);
  }
}

static void high_task(void) {
  task_delay(10);
  while (1) {
    GPIO6_DR |= SCOPE_PIN_17;
    mutex_lock(&m, MUTEX_WAIT_FOREVER);
    GPIO6_DR &= ~SCOPE_PIN_17;
    uart_printf("high: got the lock\r\n");
    mutex_unlock(&m);
    task_delay(200);
  }
}

#elif DEMO == 2

static mutex_t m;
static mutex_t r;

static void helper(void) {
  int rc = mutex_lock(&r, MUTEX_WAIT_FOREVER);
  uart_printf("helper: re-lock of r -> %d (depth 2)\r\n", rc);
  mutex_unlock(&r);
  uart_printf("helper: unwound one level, still owner\r\n");
}

static void owner_task(void) {
  mutex_lock(&m, MUTEX_WAIT_FOREVER);
  uart_printf("owner: holds m\r\n");
  int self = mutex_lock(&m, MUTEX_WAIT_FOREVER);
  uart_printf("owner: self-relock of plain m -> %d\r\n", self);
  task_delay(50);
  mutex_unlock(&m);
  uart_printf("owner: released m\r\n");

  mutex_lock(&r, MUTEX_WAIT_FOREVER);
  uart_printf("owner: holds recursive r (depth 1)\r\n");
  helper();
  mutex_unlock(&r);
  uart_printf("owner: r fully released\r\n");
  int free_unlock = mutex_unlock(&r);
  uart_printf("owner: unlock of free r -> %d\r\n", free_unlock);
  while (1) {
    task_delay(1000);
  }
}

static void thief_task(void) {
  task_delay(10);
  int rc = mutex_unlock(&m);
  uart_printf("thief: unlock of owner's m -> %d\r\n", rc);
  rc = mutex_lock(&m, 0);
  uart_printf("thief: poll of held m -> %d\r\n", rc);
  rc = mutex_lock(&m, 200);
  uart_printf("thief: timed lock -> %d\r\n", rc);
  if (rc == 1) {
    mutex_unlock(&m);
    uart_printf("thief: released m\r\n");
  }
  while (1) {
    task_delay(1000);
  }
}

#elif DEMO == 3

static mutex_t m;
static semaphore_t s;

static void low3_task(void) {
  mutex_lock(&m, MUTEX_WAIT_FOREVER);
  uart_printf("low: holds m, blocking on sem at prio %u\r\n", (unsigned)task_priority_self());
  sem_take(&s, SEM_WAIT_FOREVER);
  uart_printf("low: popped FIRST at prio %u\r\n", (unsigned)task_priority_self());
  mutex_unlock(&m);
  uart_printf("low: released m, back at prio %u\r\n", (unsigned)task_priority_self());
  while (1) {
    task_delay(1000);
  }
}

static void mid3_task(void) {
  task_delay(5);
  uart_printf("mid: blocking on sem\r\n");
  sem_take(&s, SEM_WAIT_FOREVER);
  uart_printf("mid: popped second\r\n");
  while (1) {
    task_delay(1000);
  }
}

static void high3_task(void) {
  task_delay(10);
  uart_printf("high: locking m (its owner is blocked on the sem)\r\n");
  mutex_lock(&m, MUTEX_WAIT_FOREVER);
  uart_printf("high: got m\r\n");
  mutex_unlock(&m);
  while (1) {
    task_delay(1000);
  }
}

static void giver3_task(void) {
  task_delay(20);
  uart_printf("giver: give #1\r\n");
  sem_give(&s);
  task_delay(10);
  uart_printf("giver: give #2\r\n");
  sem_give(&s);
  while (1) {
    task_delay(1000);
  }
}

#elif DEMO == 4

static mutex_t m;

static void hog_task(void) {
  mutex_lock(&m, MUTEX_WAIT_FOREVER);
  uart_printf("hog: locked m, long CS begins\r\n");
  for (int i = 1; i <= 8; i++) {
    delay_ms(100);
    uart_printf("hog: t=%u ms, prio %u\r\n", (unsigned)(i * 100), (unsigned)task_priority_self());
  }
  mutex_unlock(&m);
  uart_printf("hog: released m, prio %u\r\n", (unsigned)task_priority_self());
  while (1) {
    task_delay(1000);
  }
}

static void impatient_task(void) {
  task_delay(50);
  uart_printf("imp: timed lock on m, 300 ms budget\r\n");
  int rc = mutex_lock(&m, 300);
  uart_printf("imp: lock returned %d\r\n", rc);
  while (1) {
    task_delay(1000);
  }
}

#elif DEMO == 5

static mutex_t m1;
static mutex_t m2;

static void l2_task(void) {
  mutex_lock(&m2, MUTEX_WAIT_FOREVER);
  uart_printf("L2: holds m2, grinding\r\n");
  for (int i = 0; i < 5; i++) {
    uart_printf("L2: prio %u\r\n", (unsigned)task_priority_self());
    delay_ms(60);
  }
  mutex_unlock(&m2);
  uart_printf("L2: released m2, prio %u\r\n", (unsigned)task_priority_self());
  while (1) {
    task_delay(1000);
  }
}

static void l1_task(void) {
  task_delay(5);
  mutex_lock(&m1, MUTEX_WAIT_FOREVER);
  uart_printf("L1: holds m1, now wants m2\r\n");
  mutex_lock(&m2, MUTEX_WAIT_FOREVER);
  uart_printf("L1: got m2 at prio %u\r\n", (unsigned)task_priority_self());
  mutex_unlock(&m2);
  mutex_unlock(&m1);
  uart_printf("L1: all released, prio %u\r\n", (unsigned)task_priority_self());
  while (1) {
    task_delay(1000);
  }
}

static void h5_task(void) {
  task_delay(15);
  uart_printf("H: locking m1 (chain: H -> m1@L1 -> m2@L2)\r\n");
  mutex_lock(&m1, MUTEX_WAIT_FOREVER);
  uart_printf("H: got m1\r\n");
  mutex_unlock(&m1);
  while (1) {
    task_delay(1000);
  }
}

static void mspin_task(void) {
  task_delay(20);
  while (1) {
    delay_ms(50);
    task_delay(50);
  }
}

#elif DEMO == 6

static mutex_t m1;
static mutex_t m2;

static void a_task(void) {
  uart_printf("A: lock m1\r\n");
  mutex_lock(&m1, MUTEX_WAIT_FOREVER);
  task_delay(20);
  uart_printf("A: lock m2\r\n");
  mutex_lock(&m2, MUTEX_WAIT_FOREVER);
  uart_printf("A: got both\r\n");
  mutex_unlock(&m2);
  mutex_unlock(&m1);
  while (1) {
    uart_printf("A: alive\r\n");
    task_delay(1000);
  }
}

static void b_task(void) {
#if DEADLOCK_ORDERED
  uart_printf("B: lock m1 (global order)\r\n");
  mutex_lock(&m1, MUTEX_WAIT_FOREVER);
  task_delay(20);
  uart_printf("B: lock m2\r\n");
  mutex_lock(&m2, MUTEX_WAIT_FOREVER);
#else
  uart_printf("B: lock m2 (opposite order)\r\n");
  mutex_lock(&m2, MUTEX_WAIT_FOREVER);
  task_delay(20);
  uart_printf("B: lock m1\r\n");
  mutex_lock(&m1, MUTEX_WAIT_FOREVER);
#endif
  uart_printf("B: got both\r\n");
  mutex_unlock(&m2);
  mutex_unlock(&m1);
  while (1) {
    uart_printf("B: alive\r\n");
    task_delay(1000);
  }
}

#endif

int main(void) {
  switch_clock_600mhz();
  init_fpu();
  init_uart();
  init_dwt();
  init_scope_pins();
  uart_printf("Cronus online - P2-3 mutex, DEMO %d, PI=%d\r\n", DEMO, CONFIG_MUTEX_PRIORITY_INHERITANCE);

#if DEMO == 1
  init_mutex(&m);
  create_task(stack_a, low_task,  STACK_SIZE, 2, "low");
  create_task(stack_b, med_task,  STACK_SIZE, 5, "med");
  create_task(stack_c, high_task, STACK_SIZE, 8, "high");
#elif DEMO == 2
  init_mutex(&m);
  init_mutex_recursive(&r);
  create_task(stack_a, owner_task, STACK_SIZE, 4, "owner");
  create_task(stack_b, thief_task, STACK_SIZE, 3, "thief");
#elif DEMO == 3
  init_mutex(&m);
  sem_init(&s, 0, 10);
  create_task(stack_a, low3_task,  STACK_SIZE, 2, "low");
  create_task(stack_b, mid3_task,  STACK_SIZE, 4, "mid");
  create_task(stack_c, high3_task, STACK_SIZE, 8, "high");
  create_task(stack_d, giver3_task, STACK_SIZE, 6, "giver");
#elif DEMO == 4
  init_mutex(&m);
  create_task(stack_a, hog_task,       STACK_SIZE, 2, "hog");
  create_task(stack_b, impatient_task, STACK_SIZE, 8, "imp");
#elif DEMO == 5
  init_mutex(&m1);
  init_mutex(&m2);
  create_task(stack_a, l2_task,    STACK_SIZE, 1, "L2");
  create_task(stack_b, l1_task,    STACK_SIZE, 3, "L1");
  create_task(stack_c, h5_task,    STACK_SIZE, 8, "H");
  create_task(stack_d, mspin_task, STACK_SIZE, 5, "M");
#elif DEMO == 6
  init_mutex(&m1);
  init_mutex(&m2);
  create_task(stack_a, a_task, STACK_SIZE, 4, "A");
  create_task(stack_b, b_task, STACK_SIZE, 3, "B");
#endif
  create_task(stack_mon, monitor_task, STACK_SIZE, 1, "monitor");

  gpt_init();
  lpm_init();
  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
