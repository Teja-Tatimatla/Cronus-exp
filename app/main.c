#include "cache.h"
#include "cronus/config.h"
#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <uart.h>
#include <cronus/tasks.h>
#include <cronus/critical.h>
#include <cronus/sem.h>
#include <cronus/mutex.h>
#include <cronus/queue.h>
#include <cronus/timer.h>
#include <cronus/device.h>
#include <cronus/irq.h>
#include <cronus/pinctrl.h>
#include <gpio.h>
#include <sensor.h>
#include <gpt.h>
#include <cm7.h>
#include <mpu.h>
#include <fpu.h>
#include <dwt.h>
#include <lpm.h>
#include <systick.h>

#define DEMO 1

#define GPC_FIX 1

#define STACK_SIZE 256
static uint32_t stack_a[STACK_SIZE]    __attribute__((aligned(32)));
static uint32_t stack_b[STACK_SIZE]    __attribute__((aligned(32)));
static uint32_t stack_mon[STACK_SIZE]  __attribute__((aligned(32)));
static uint32_t stack_init[STACK_SIZE] __attribute__((aligned(32)));
static uint32_t stack_td[CONFIG_TIMER_TASK_STACK] __attribute__((aligned(32)));

static void devinit_task(void) {
  int failed = init_device(DEVICE_LEVEL_POST_KERNEL);
  uart_printf("post-kernel init done, %d failed/skipped\r\n", failed);
  task_delete(get_task_handle());
}

static void monitor_task(void) {
  while (1) {
    task_reporter();
    task_delay(1000);
  }
}

#if DEMO == 6 || DEMO == 7

#define GPT2_BASE          0x401F0000u
#define GPT2_CR            (*(volatile uint32_t *)(GPT2_BASE + 0x00))
#define GPT2_PR            (*(volatile uint32_t *)(GPT2_BASE + 0x04))
#define GPT2_SR            (*(volatile uint32_t *)(GPT2_BASE + 0x08))
#define GPT2_IR            (*(volatile uint32_t *)(GPT2_BASE + 0x0C))
#define GPT2_OCR1          (*(volatile uint32_t *)(GPT2_BASE + 0x10))
#define GPT2_CNT           (*(volatile uint32_t *)(GPT2_BASE + 0x24))
#define GPT2_IRQ           101u

static void gpt2_hw_init(void) {
  CCM_CCGR0 |= (3u << 24) | (3u << 26);

  GPT2_CR = GPT_CR_SWR;
  while(GPT2_CR & GPT_CR_SWR) {
    __asm volatile ("nop");
  }
  GPT2_PR = 0;
  GPT2_CR = GPT_CR_EN_24M | GPT_CR_CLKSRC_24M | GPT_CR_ENMOD | GPT_CR_WAITEN
          | GPT_CR_DOZEEN | GPT_CR_STOPEN | GPT_CR_FRR;
  GPT2_SR = GPT_SR_OF1;
  GPT2_CR |= GPT_CR_EN;
}

#endif

#if DEMO == 1

static void led_task(void) {
  const struct device* led = get_device("gpio2");
  gpio_pin_configure(led, 3, GPIO_DIR_OUTPUT);
  uart_printf("blinking LED through the class API, zero register writes here\r\n");
  for(int i = 0; i < 10; i++) {
    gpio_pin_toggle(led, 3);
    task_delay(250);
  }
  uart_printf("LED demo done, state=%d\r\n", gpio_pin_get(led, 3));
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 2

static void vtable_task(void) {
  const struct device* a = get_device("gpio2");
  const struct device* b = get_device("gpio6");
  uart_printf("gpio2 api %u, gpio6 api %u, %s\r\n",
              (unsigned)(uintptr_t)a->api, (unsigned)(uintptr_t)b->api,
              (a->api == b->api) ? "SHARED" : "DIFFERENT");
  uart_printf("gpio2 cfg %u, gpio6 cfg %u, %s\r\n",
              (unsigned)(uintptr_t)a->config, (unsigned)(uintptr_t)b->config,
              (a->config == b->config) ? "shared?!" : "distinct");
  gpio_pin_configure(b, 23, GPIO_DIR_OUTPUT);
  for(int i = 0; i < 10; i++) {
    gpio_pin_toggle(b, 23);
    task_delay(100);
  }
  uart_printf("gpio6 pin 23 toggled 10x through the SAME vtable\r\n");
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 3

static void inventory_task(void) {
  task_delay(100);
  device_reporter();
  const struct device* hit = get_device("gpio2");
  const struct device* miss = get_device("nonesuch");
  uart_printf("binding gpio2: %s\r\n", hit ? hit->name : "NULL");
  uart_printf("binding nonesuch: %s\r\n", miss ? miss->name : "NULL");
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 4

static void order_task(void) {
  task_delay(100);
  uart_printf("file order led with the fast gpio block\r\n");
  uart_printf("printed order above must be gpio1..gpio5 then gpio6..gpio9, then bad,fake,dep\r\n");
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 5

static void failure_task(void) {
  task_delay(100);
  const struct device* bad = get_device("sensor_bad");
  const struct device* dep = get_device("depchk");
  uart_printf("sensor_bad ready=%d\r\n", device_is_ready(bad));
  uart_printf("fetch on sensor_bad rc=%d (refused at the API edge)\r\n",
              sensor_sample_fetch(bad));
  uart_printf("depchk ready=%d, init_res=%d (skipped: dep not ready)\r\n",
              device_is_ready(dep), (int)dep->state->init_res);
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 6

typedef struct {
  uint32_t seq;
} d6_ctx_t;

static d6_ctx_t d6_ctx;

static void d6_handler(void* arg) {
  GPT2_SR = GPT_SR_OF1;
  GPT2_OCR1 = GPT2_CNT + (GPT_CLK_HZ / 2u);
  d6_ctx_t* ctx = arg;
  ctx->seq++;
  uart_printf("evt %u, arg %s, dispatch overhead %u cycles\r\n",
              (unsigned)ctx->seq,
              (arg == &d6_ctx) ? "ok" : "WRONG");
}

static void d6_task(void) {
  driver_irq_connect(GPT2_IRQ, d6_handler, &d6_ctx);
  driver_irq_enable(GPT2_IRQ, 0x80);
  GPT2_OCR1 = GPT2_CNT + (GPT_CLK_HZ / 2u);
  GPT2_IR = GPT_IR_OF1IE;
  uart_printf("GPT2 routed through irq_dispatch, instance ctx attached\r\n");
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 7

static semaphore_t d7_sem;
static volatile uint32_t d7_target;
static volatile uint32_t d7_cnt_at_isr;
static volatile uint32_t d7_defer;
static volatile uint32_t d7_dwt_isr;
static volatile uint8_t  d7_fired;

static void d7_handler(void* arg) {
  GPIO6_DR |= SCOPE_PIN_16;
  GPT2_SR = GPT_SR_OF1;
  GPT2_IR = 0;
  d7_cnt_at_isr = GPT2_CNT;
  d7_defer = d7_cnt_at_isr - d7_target;
  d7_dwt_isr = DWT_CYCCNT;
  d7_fired = 1;
  if(sem_give_from_isr(&d7_sem) == 1) {
    SCB_ICSR = ICSR_PENDSVSET;
  }
}

static void d7_worker(void) {
  sem_take(&d7_sem, WAIT_FOREVER);
  uint32_t now = DWT_CYCCNT;
  GPIO6_DR &= ~SCOPE_PIN_16;
  uart_printf("fired=%u defer=%u counts = %u us\r\n",
              (unsigned)d7_fired, (unsigned)d7_defer,
              (unsigned)(d7_defer / 24u));
  uart_printf("ISR-to-wake %u cycles = %u us\r\n",
              (unsigned)(now - d7_dwt_isr),
              (unsigned)((now - d7_dwt_isr) / 600u));
  uart_printf("ticks=%u\r\n", (unsigned)get_tick_count());
  while(1) {
    task_delay(60000);
  }
}

static void d7_sleeper(void) {
  while(1) {
    task_delay(60000);
  }
}

static void d7_arm_task(void) {
  driver_irq_connect(GPT2_IRQ, d7_handler, (void*)0);
#if GPC_FIX
  uart_printf("build: driver_irq_enable (both gates: NVIC take + GPC wake)\r\n");
  driver_irq_enable(GPT2_IRQ, 0x80);
#else
  uart_printf("build: NVIC-only enable (GPC wake masked)\r\n");
  NVIC_IPR(GPT2_IRQ) = 0x80;
  NVIC_ISER(GPT2_IRQ / 32u) = (1u << (GPT2_IRQ % 32u));
#endif
  d7_target = GPT2_CNT + 8u * GPT_CLK_HZ;
  GPT2_OCR1 = d7_target;
  GPT2_IR = GPT_IR_OF1IE;
  uart_printf("GPT2 asserts in 8s, system going idle\r\n");
  task_delete(get_task_handle());
}

#elif DEMO == 8

static void power_task(void) {
  task_delay(300);
  const struct device* s = get_device("sensor0");
  const struct device* led = get_device("gpio2");
  int32_t v;
  for(int i = 0; i < 3; i++) {
    sensor_sample_fetch(s);
    sensor_channel_get(s, &v);
    uart_printf("sample %d\r\n", (int)v);
  }
  uart_printf("== suspend all ==\r\n");
  device_power_all(1);
  uart_printf("fetch while suspended rc=%d\r\n", sensor_sample_fetch(s));
  uart_printf("toggle while suspended rc=%d\r\n", gpio_pin_toggle(led, 3));
  task_delay(500);
  uart_printf("== resume all ==\r\n");
  device_power_all(0);
  sensor_sample_fetch(s);
  sensor_channel_get(s, &v);
  uart_printf("sample after resume %d\r\n", (int)v);
  while(1) {
    task_delay(10000);
  }
}

#endif

int main(void) {
  switch_clock_600mhz();
  init_fpu();
  init_uart();
  init_dwt();
  init_mpu();
  icache_enable();
  dcache_enable();

  lpm_init();

  pinctrl_apply(board_pins, board_pin_count);
  GPIO6_GDIR |= SCOPE_PIN_14 | SCOPE_PIN_16 | SCOPE_PIN_17;
  uart_printf("Cronus online - P2-6 drivers, DEMO %d\r\n", DEMO);

  int pre_failed = init_device(DEVICE_LEVEL_PRE_KERNEL);
  uart_printf("pre-kernel init done, %d failed/skipped\r\n", pre_failed);

  init_timer_daemon(stack_td);
  create_task(stack_init, devinit_task, STACK_SIZE, 9, "devinit");

#if DEMO == 1
  create_task(stack_a, led_task, STACK_SIZE, 4, "led");
#elif DEMO == 2
  create_task(stack_a, vtable_task, STACK_SIZE, 4, "vtab");
#elif DEMO == 3
  create_task(stack_a, inventory_task, STACK_SIZE, 4, "inv");
#elif DEMO == 4
  create_task(stack_a, order_task, STACK_SIZE, 4, "order");
#elif DEMO == 5
  create_task(stack_a, failure_task, STACK_SIZE, 4, "fail");
#elif DEMO == 6
  create_task(stack_a, d6_task, STACK_SIZE, 4, "d6");
#elif DEMO == 7
  create_task(stack_a, d7_arm_task, STACK_SIZE, 4, "arm");
  create_task(stack_b, d7_worker, STACK_SIZE, 3, "worker");
  sem_init(&d7_sem, 0, 1);
#elif DEMO == 8
  create_task(stack_a, power_task, STACK_SIZE, 4, "power");
#endif

#if DEMO == 7
  create_task(stack_mon, d7_sleeper, STACK_SIZE, 2, "sleeper");
#else
  create_task(stack_mon, monitor_task, STACK_SIZE, 1, "monitor");
#endif

  gpt_init();
#if DEMO == 6 || DEMO == 7
  gpt2_hw_init();
#endif
  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
