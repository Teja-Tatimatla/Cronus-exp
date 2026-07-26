#include "cronus/config.h"
#include "cronus/mutex.h"
#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <uart.h>
#include <cronus/tasks.h>
#include <cronus/critical.h>
#include <cronus/sem.h>
#include <cronus/queue.h>
#include <gpt.h>
#include <cm7.h>
#include <mpu.h>
#include <fpu.h>
#include <dwt.h>
#include <lpm.h>

#define DEMO 4

#define STACK_SIZE 256
static uint32_t stack_a[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_b[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_c[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_d[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_mon[STACK_SIZE] __attribute__((aligned(32)));

typedef struct {
  uint32_t seq;
  uint16_t a;
  uint16_t b;
} msg_t;

static void init_scope_pins(void) {
  IOMUXC_GPR_GPR26 |= SCOPE_PIN_14;
  IOMUXC_MUX_AD_B1_02 = 5;
  IOMUXC_PAD_AD_B1_02 = 0x10B0;
  GPIO6_GDIR |= SCOPE_PIN_14;

  IOMUXC_GPR_GPR26 |= SCOPE_PIN_16;
  IOMUXC_MUX_AD_B1_07 = 5;
  IOMUXC_PAD_AD_B1_07 = 0x10B0;
  GPIO6_GDIR |= SCOPE_PIN_16;

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
    task_delay(1000);
  }
}

#if DEMO == 6 || DEMO == 8

#define GPT2_BASE          0x401F0000u
#define GPT2_CR            (*(volatile uint32_t *)(GPT2_BASE + 0x00))
#define GPT2_PR            (*(volatile uint32_t *)(GPT2_BASE + 0x04))
#define GPT2_SR            (*(volatile uint32_t *)(GPT2_BASE + 0x08))
#define GPT2_IR            (*(volatile uint32_t *)(GPT2_BASE + 0x0C))
#define GPT2_OCR1          (*(volatile uint32_t *)(GPT2_BASE + 0x10))
#define GPT2_CNT           (*(volatile uint32_t *)(GPT2_BASE + 0x24))
#define GPT2_IRQ           101u
#define GPT2_PERIOD_CLKS   (GPT_CLK_HZ / 10u)

static void gpt2_init(void) {
  CCM_CCGR0 |= (3u << 24) | (3u << 26);

  GPT2_CR = GPT_CR_SWR;
  while(GPT2_CR & GPT_CR_SWR) {
    __asm volatile ("nop");
  }
  GPT2_PR = 0;
  GPT2_CR = GPT_CR_EN_24M | GPT_CR_CLKSRC_24M | GPT_CR_ENMOD | GPT_CR_WAITEN
          | GPT_CR_DOZEEN | GPT_CR_STOPEN | GPT_CR_FRR;

  NVIC_ISER(GPT2_IRQ / 32u) = (1u << (GPT2_IRQ % 32u));
  NVIC_IPR(GPT2_IRQ) = 0x80u;

  GPC_IMR(GPT2_IRQ / 32u) &= ~(1u << (GPT2_IRQ % 32u));

  GPT2_SR = GPT_SR_OF1;
  GPT2_OCR1 = GPT2_CNT + GPT2_PERIOD_CLKS;
  GPT2_IR = GPT_IR_OF1IE;
  GPT2_CR |= GPT_CR_EN;
}

#endif

#if DEMO == 1

static msg_t   q_store[4];
static queue_t q;

static void producer_task(void) {
  msg_t m;
  for(uint32_t i = 1; i <= 12; i++) {
    m.seq = i;
    m.a = (uint16_t)(i * 10u);
    m.b = (uint16_t)(i * 100u);
    queue_send(&q, &m, WAIT_FOREVER);
    uart_printf("prod: sent %u\r\n", (unsigned)m.seq);
    task_delay(20);
  }
  while(1) {
    task_delay(1000);
  }
}

static void consumer_task(void) {
  msg_t pk;
  msg_t rc;
  while(1) {
    if(queue_peek(&q, &pk) == 1) {
      uart_printf("cons: peek %u\r\n", (unsigned)pk.seq);
    }
    queue_recv(&q, &rc, WAIT_FOREVER);
    uart_printf("cons: recv %u a=%u b=%u\r\n", (unsigned)rc.seq, (unsigned)rc.a, (unsigned)rc.b);
    task_delay(30);
  }
}

#elif DEMO == 2

static msg_t   q_store[4];
static queue_t q;

static void producer_task(void) {
  msg_t m;
  m.a = 0;
  m.b = 0;
  for(uint32_t i = 1; ; i++) {
    m.seq = i;
    GPIO6_DR |= SCOPE_PIN_16;
    queue_send(&q, &m, WAIT_FOREVER);
    GPIO6_DR &= ~SCOPE_PIN_16;
    task_delay(10);
  }
}

static void urgent_task(void) {
  task_delay(120);
  msg_t m;
  m.seq = 99;
  m.a = 0;
  m.b = 0;
  uart_printf("urgent: send_to_front on a FULL queue, blocking\r\n");
  queue_send_to_front(&q, &m, WAIT_FOREVER);
  uart_printf("urgent: sent\r\n");
  while(1) {
    task_delay(1000);
  }
}

static void consumer_task(void) {
  msg_t m;
  task_delay(60);
  while(1) {
    GPIO6_DR |= SCOPE_PIN_17;
    queue_recv(&q, &m, WAIT_FOREVER);
    GPIO6_DR &= ~SCOPE_PIN_17;
    uart_printf("cons: recv %u\r\n", (unsigned)m.seq);
    task_delay(50);
  }
}

#elif DEMO == 3

static msg_t   q_store[4];
static queue_t q;

static void snapshot(const char* tag) {
  enter_critical();
  uint32_t h = q.head;
  uint32_t t = q.tail;
  uint32_t c = q.count;
  exit_critical();
  uart_printf("prod: %s h/t/c = %u/%u/%u\r\n", tag, (unsigned)h, (unsigned)t, (unsigned)c);
}

static void producer_task(void) {
  msg_t m;
  m.a = 0;
  m.b = 0;
  task_delay(200);
  for(uint32_t i = 1; ; i++) {
    m.seq = i;
    snapshot("before");
    queue_send(&q, &m, WAIT_FOREVER);
    snapshot("after ");
    task_delay(300);
  }
}

static void consumer_task(void) {
  msg_t m;
  while(1) {
    GPIO6_DR |= SCOPE_PIN_17;
    queue_recv(&q, &m, WAIT_FOREVER);
    GPIO6_DR &= ~SCOPE_PIN_17;
    uart_printf("cons: got %u\r\n", (unsigned)m.seq);
  }
}

#elif DEMO == 4

static msg_t   q_store[4];
static queue_t q;

static void worker_body(void) {
  msg_t m;
  uint8_t p = task_priority_self();
  uart_printf("rx%u: waiting\r\n", (unsigned)p);
  queue_recv(&q, &m, WAIT_FOREVER);
  uart_printf("rx%u: woke with %u\r\n", (unsigned)p, (unsigned)m.seq);

  task_delay(300);
  m.seq = p;
  queue_send(&q, &m, WAIT_FOREVER);
  uart_printf("rx%u: sent\r\n", (unsigned)p);
  while(1) {
    task_delay(1000);
  }
}

static void driver_task(void) {
  msg_t m;
  m.a = 0;
  m.b = 0;
  task_delay(50);
  for(uint32_t i = 1; i <= 3; i++) {
    m.seq = i;
    uart_printf("drv: send %u\r\n", (unsigned)i);
    queue_send(&q, &m, WAIT_FOREVER);
    task_delay(40);
  }

  for(uint32_t i = 10; i <= 13; i++) {
    m.seq = i;
    queue_send(&q, &m, WAIT_FOREVER);
  }
  uart_printf("drv: queue filled\r\n");

  task_delay(400);
  for(uint32_t i = 0; i < 3; i++) {
    queue_recv(&q, &m, WAIT_FOREVER);
    uart_printf("drv: drained %u\r\n", (unsigned)m.seq);
    task_delay(40);
  }
  while(1) {
    task_delay(1000);
  }
}

#elif DEMO == 5

static msg_t   q_store[4];
static queue_t q;

static void sender_body(uint32_t timeout) {
  msg_t m;
  uint8_t p = task_priority_self();
  m.seq = p;
  m.a = 0;
  m.b = 0;
  uart_printf("s%u: blocking send, timeout %u\r\n", (unsigned)p, (unsigned)timeout);
  int rc = queue_send(&q, &m, timeout);
  uart_printf("s%u: send rc=%d\r\n", (unsigned)p, rc);
  while(1) {
    task_delay(1000);
  }
}

static void s8_task(void) { task_delay(20); sender_body(WAIT_FOREVER); }
static void s6_task(void) { task_delay(30); sender_body(250); }
static void s4_task(void) { task_delay(40); sender_body(WAIT_FOREVER); }

static void driver_task(void) {
  msg_t m;
  m.a = 0;
  m.b = 0;
  for(uint32_t i = 1; i <= 4; i++) {
    m.seq = i;
    queue_send(&q, &m, WAIT_FOREVER);
  }
  uart_printf("drv: filled\r\n");

  task_delay(400);
  for(uint32_t i = 0; i < 6; i++) {
    queue_recv(&q, &m, WAIT_FOREVER);
    uart_printf("drv: recv %u\r\n", (unsigned)m.seq);
    task_delay(50);
  }

  int rc = queue_recv(&q, &m, 150);
  uart_printf("drv: recv on empty rc=%d\r\n", rc);
  while(1) {
    task_delay(1000);
  }
}

#elif DEMO == 6

static msg_t   q_store[8];
static queue_t q;
static volatile uint32_t isr_seq = 0;
static volatile uint32_t isr_drops = 0;

void GPT2_Handler(void) {
  GPT2_SR = GPT_SR_OF1;
  GPT2_OCR1 = GPT2_CNT + GPT2_PERIOD_CLKS;
  GPIO6_DR ^= SCOPE_PIN_14;

  msg_t m;
  m.seq = ++isr_seq;
  m.a = 0;
  m.b = 0;
  if(q.count >= 8u) {
    isr_drops++;
  }
  int p = queue_send_from_isr(&q, &m);
  if(p == 1) {
    SCB_ICSR = ICSR_PENDSVSET;
  }
}

static void worker_task(void) {
  msg_t m;
  uint32_t n = 0;
  while(1) {
    queue_recv(&q, &m, WAIT_FOREVER);
    n++;
    uart_printf("worker: seq=%u (rx=%u drops=%u)\r\n",
                (unsigned)m.seq, (unsigned)n, (unsigned)isr_drops);
  }
}

#elif DEMO == 7

typedef struct {
  uint32_t seq;
  uint8_t  payload[64];
} frame_t;

static frame_t  pool[3];
static frame_t* free_store[3];
static frame_t* data_store[3];
static queue_t  free_q;
static queue_t  data_q;

static void producer_task(void) {
  frame_t* f;
  for(uint32_t i = 1; ; i++) {
    if(queue_count(&free_q) == 0) {
      uart_printf("prod: pool dry, blocking\r\n");
    }
    queue_recv(&free_q, &f, WAIT_FOREVER);
    f->seq = i;
    f->payload[0] = (uint8_t)i;
    queue_send(&data_q, &f, WAIT_FOREVER);
    task_delay(40);
  }
}

static void consumer_task(void) {
  frame_t* f;
  task_delay(100);
  while(1) {
    queue_recv(&data_q, &f, WAIT_FOREVER);
    uart_printf("cons: seq=%u buf=%p ok=%u\r\n",
                (unsigned)f->seq, (void*)f, (unsigned)(f->payload[0] == (uint8_t)f->seq));
    task_delay(90);
    queue_send(&free_q, &f, 0);
  }
}

#elif DEMO == 8

static msg_t    qa_store[4];
static msg_t    qb_store[4];
static queue_t* set_store[8];
static queue_t  qa;
static queue_t  qb;
static queue_t  setq;
static volatile uint32_t isr_seq = 2000;

void GPT2_Handler(void) {
  GPT2_SR = GPT_SR_OF1;
  GPT2_OCR1 = GPT2_CNT + GPT2_PERIOD_CLKS;

  msg_t m;
  m.seq = ++isr_seq;
  m.a = 0;
  m.b = 0;
  int p = queue_send_from_isr(&qb, &m);
  if(p == 1) {
    SCB_ICSR = ICSR_PENDSVSET;
  }
}

static void feeder_task(void) {
  msg_t m;
  m.a = 0;
  m.b = 0;
  for(uint32_t i = 1000; ; i++) {
    m.seq = i;
    queue_send(&qa, &m, WAIT_FOREVER);
    task_delay(70);
  }
}

static void dispatcher_task(void) {
  msg_t m;
  while(1) {
    queue_t* r = queue_select(&setq, WAIT_FOREVER);
    if(r == NULL) {
      continue;
    }
    if(queue_recv(r, &m, 0) == 1) {
      uart_printf("disp: %s seq=%u\r\n", (r == &qa) ? "qa" : "qb", (unsigned)m.seq);
    } else {
      uart_printf("disp: stale handle for %s\r\n", (r == &qa) ? "qa" : "qb");
    }
  }
}

#endif

int main(void) {
  switch_clock_600mhz();
  init_fpu();
  init_uart();
  init_dwt();
  init_scope_pins();
  uart_printf("Cronus online - P2-4 queue, DEMO %d\r\n", DEMO);

#if DEMO == 1
  init_queue(&q, q_store, 4, sizeof(msg_t));
  create_task(stack_a, producer_task, STACK_SIZE, 3, "prod");
  create_task(stack_b, consumer_task, STACK_SIZE, 5, "cons");
#elif DEMO == 2
  init_queue(&q, q_store, 4, sizeof(msg_t));
  create_task(stack_a, producer_task, STACK_SIZE, 5, "prod");
  create_task(stack_b, urgent_task,   STACK_SIZE, 8, "urgent");
  create_task(stack_c, consumer_task, STACK_SIZE, 3, "cons");
#elif DEMO == 3
  init_queue(&q, q_store, 4, sizeof(msg_t));
  create_task(stack_a, producer_task, STACK_SIZE, 3, "prod");
  create_task(stack_b, consumer_task, STACK_SIZE, 6, "cons");
#elif DEMO == 4
  init_queue(&q, q_store, 4, sizeof(msg_t));
  create_task(stack_a, worker_body, STACK_SIZE, 8, "rx8");
  create_task(stack_b, worker_body, STACK_SIZE, 6, "rx6");
  create_task(stack_c, worker_body, STACK_SIZE, 4, "rx4");
  create_task(stack_d, driver_task, STACK_SIZE, 2, "drv");
#elif DEMO == 5
  init_queue(&q, q_store, 4, sizeof(msg_t));
  create_task(stack_a, s8_task,     STACK_SIZE, 8, "s8");
  create_task(stack_b, s6_task,     STACK_SIZE, 6, "s6");
  create_task(stack_c, s4_task,     STACK_SIZE, 4, "s4");
  create_task(stack_d, driver_task, STACK_SIZE, 2, "drv");
#elif DEMO == 6
  init_queue(&q, q_store, 8, sizeof(msg_t));
  create_task(stack_a, worker_task, STACK_SIZE, 5, "worker");
#elif DEMO == 7
  init_queue(&free_q, free_store, 3, sizeof(frame_t*));
  init_queue(&data_q, data_store, 3, sizeof(frame_t*));
  for(uint32_t i = 0; i < 3; i++) {
    frame_t* p = &pool[i];
    queue_send(&free_q, &p, 0);
  }
  create_task(stack_a, producer_task, STACK_SIZE, 4, "prod");
  create_task(stack_b, consumer_task, STACK_SIZE, 6, "cons");
#elif DEMO == 8
  init_queue(&qa, qa_store, 4, sizeof(msg_t));
  init_queue(&qb, qb_store, 4, sizeof(msg_t));
  init_queue(&setq, set_store, 8, sizeof(queue_t*));
  queue_set_add(&qa, &setq);
  queue_set_add(&qb, &setq);
  create_task(stack_a, feeder_task,     STACK_SIZE, 3, "feeder");
  create_task(stack_b, dispatcher_task, STACK_SIZE, 6, "disp");
#endif
  create_task(stack_mon, monitor_task, STACK_SIZE, 1, "monitor");

  gpt_init();
  lpm_init();
#if DEMO == 6 || DEMO == 8
  gpt2_init();
#endif
  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
