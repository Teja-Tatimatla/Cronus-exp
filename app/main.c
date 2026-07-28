#include "cronus/config.h"
#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <clock.h>
#include <uart.h>
#include <cronus/tasks.h>
#include <cronus/critical.h>
#include <cronus/sem.h>
#include <cronus/queue.h>
#include <cronus/timer.h>
#include <gpt.h>
#include <cm7.h>
#include <mpu.h>
#include <fpu.h>
#include <dwt.h>
#include <lpm.h>
#include <systick.h>

#define DEMO 8

#define MON_PERIOD_MS ((DEMO == 6) ? 5000u : 1000u)

#define STACK_SIZE 256
static uint32_t stack_a[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_b[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_c[STACK_SIZE]   __attribute__((aligned(32)));
static uint32_t stack_mon[STACK_SIZE] __attribute__((aligned(32)));
static uint32_t stack_td[CONFIG_TIMER_TASK_STACK] __attribute__((aligned(32)));

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
    task_delay(MON_PERIOD_MS);
  }
}

#if DEMO == 8

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

static timer_t one_shot;
static timer_t per50;
static uint32_t fires50;

static void cb_once(timer_t* t) {
  uart_printf("one-shot: fired @ %u\r\n", (unsigned)get_tick_count());
}

static void cb_50(timer_t* t) {
  GPIO6_DR ^= SCOPE_PIN_16;
  uint32_t* n = (uint32_t*)t->user_data;
  (*n)++;
}

static void starter_task(void) {
  uart_printf("arming one-shot(200) + per50\r\n");
  timer_start(&one_shot, WAIT_FOREVER);
  timer_start(&per50, WAIT_FOREVER);
  while(1) {
    task_delay(1000);
    uart_printf("per50 fires so far: %u\r\n", (unsigned)fires50);
  }
}

#elif DEMO == 2

static timer_t per10;
static uint32_t fires10;
static uint32_t start_tick;

static void cb_10(timer_t* t) {
  if(fires10 == 0) {
    start_tick = t->expiry - t->period;
  }
  fires10++;
  if(fires10 % 1000u == 0) {
    uint32_t elapsed = get_tick_count() - start_tick;
    uint32_t ideal = (fires10 - 1u) * 10u;
    int32_t drift = (int32_t)(elapsed - ideal);
    uart_printf("fires=%u elapsed=%u ideal=%u drift=%d\r\n",
                (unsigned)fires10, (unsigned)elapsed,
                (unsigned)ideal, (int)drift);
  }
}

static void starter_task(void) {
  timer_start(&per10, WAIT_FOREVER);
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 3

static timer_t tmr;
static timer_t kami;

static void cb_named(timer_t* t) {
  uart_printf("%s: fired @ %u\r\n", t->name, (unsigned)get_tick_count());
}

static void cb_kami(timer_t* t) {
  int rc = timer_stop(t, 0);
  uart_printf("%s: fired once, self-stop rc=%d\r\n", t->name, rc);
}

static void narrator_task(void) {
  uart_printf("== self-stop contract ==\r\n");
  timer_start(&kami, WAIT_FOREVER);
  task_delay(200);

  uart_printf("== start ==\r\n");
  timer_start(&tmr, WAIT_FOREVER);
  task_delay(350);

  uart_printf("== stop ==\r\n");
  timer_stop(&tmr, WAIT_FOREVER);
  task_delay(300);

  uart_printf("== change_period(50) ==\r\n");
  timer_change_period(&tmr, 50, WAIT_FOREVER);
  task_delay(220);

  uart_printf("== delete, then a start that must go stale ==\r\n");
  timer_delete(&tmr, WAIT_FOREVER);
  timer_start(&tmr, WAIT_FOREVER);
  task_delay(50);

  uart_printf("== reporter ==\r\n");
  timer_reporter(WAIT_FOREVER);
  while(1) {
    task_delay(5000);
  }
}

#elif DEMO == 4

static timer_t shot30;
static volatile uint32_t isr_n;

static void cb_shot(timer_t* t) {
  uart_printf("shot30: fired @ %u\r\n", (unsigned)get_tick_count());
}

static void pfn_deferred(void* arg1, uint32_t arg2) {
  uart_printf("deferred: n=%u @ %u\r\n", (unsigned)arg2, (unsigned)get_tick_count());
}

void GPT2_Handler(void) {
  GPT2_SR = GPT_SR_OF1;
  GPT2_OCR1 = GPT2_CNT + GPT2_PERIOD_CLKS;

  isr_n++;
  int p;
  if(isr_n & 1u) {
    p = timer_start_from_isr(&shot30);
  } else {
    p = timer_pend_function_from_isr(pfn_deferred, (void*)0, isr_n);
  }
  if(p == 1) {
    SCB_ICSR = ICSR_PENDSVSET;
  }
}

static void starter_task(void) {
  uart_printf("GPT2 arms shot30 / pends work, alternating every 100ms\r\n");
  while(1) {
    task_delay(5000);
  }
}

#elif DEMO == 5

static timer_t ta;
static timer_t tb;
static timer_t tc;

static void cb_named(timer_t* t) {
  uart_printf("%s @ %u\r\n", t->name, (unsigned)get_tick_count());
}

static void starter_task(void) {
  timer_start(&ta, WAIT_FOREVER);
  timer_start(&tb, WAIT_FOREVER);
  timer_start(&tc, WAIT_FOREVER);
  while(1) {
    task_delay(5000);
  }
}

#elif DEMO == 6

static timer_t per500;

static void cb_500(timer_t* t) {
  GPIO6_DR ^= SCOPE_PIN_16;
  uart_printf("per500 @ %u\r\n", (unsigned)get_tick_count());
}

static void starter_task(void) {
  uart_printf("only a 500ms timer armed; watch PIN_17 for tickless windows\r\n");
  timer_start(&per500, WAIT_FOREVER);
  while(1) {
    task_delay(60000);
  }
}

#elif DEMO == 7

#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define JIT_N 1000u

static timer_t per50;
static uint32_t deltas[JIT_N];
static uint32_t nD;
static uint32_t lastC;
static volatile uint32_t doneD;

static void cb_jit(timer_t* t) {
  GPIO6_DR ^= SCOPE_PIN_16;
  uint32_t c = DWT_CYCCNT;
  if(lastC != 0 && nD < JIT_N) {
    deltas[nD++] = c - lastC;
    if(nD == JIT_N) {
      doneD = 1;
    }
  }
  lastC = c;
}

static void hog_task(void) {
  while(1) {
    delay_ms(20);
    task_delay(30);
  }
}

static void crunch_task(void) {
  uart_printf("daemon prio %u, hog @5: sampling %u periods of 50ms\r\n",
              (unsigned)CONFIG_TIMER_TASK_PRIORITY, (unsigned)JIT_N);
  timer_start(&per50, WAIT_FOREVER);
  while(!doneD) {
    task_delay(500);
  }

  for(uint32_t i = 1; i < JIT_N; i++) {
    uint32_t k = deltas[i];
    uint32_t j = i;
    while(j > 0 && deltas[j - 1] > k) {
      deltas[j] = deltas[j - 1];
      j--;
    }
    deltas[j] = k;
  }

  uint32_t mn = deltas[0];
  uint32_t md = deltas[JIT_N / 2u];
  uint32_t p99 = deltas[(JIT_N * 99u) / 100u];
  uint32_t mx = deltas[JIT_N - 1u];
  uart_printf("inter-arrival cycles: min=%u med=%u p99=%u max=%u\r\n",
              (unsigned)mn, (unsigned)md, (unsigned)p99, (unsigned)mx);
  uart_printf("as us (cyc/600):      min=%u med=%u p99=%u max=%u (nominal 50000)\r\n",
              (unsigned)(mn / 600u), (unsigned)(md / 600u),
              (unsigned)(p99 / 600u), (unsigned)(mx / 600u));
  while(1) {
    task_delay(10000);
  }
}

#elif DEMO == 8

static timer_t tk;
static timer_t ts;
static uint32_t last_tk;
static uint32_t last_ts;

static void cb_pol(timer_t* t) {
  uint32_t now = get_tick_count();
  uint32_t* last = (uint32_t*)t->user_data;
  uart_printf("%s: +%u (tick%%50=%u)\r\n", t->name,
              (unsigned)(now - *last), (unsigned)(now % 50u));
  *last = now;
}

static void starver_task(void) {
  timer_start(&tk, WAIT_FOREVER);
  timer_start(&ts, WAIT_FOREVER);
  task_delay(1000);
  uart_printf("== starving the daemon for 300ms ==\r\n");
  delay_ms(300);
  uart_printf("== starvation over ==\r\n");
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
  init_scope_pins();
  uart_printf("Cronus online - P2-5 timers, DEMO %d\r\n", DEMO);

  init_timer_daemon(stack_td);

#if DEMO == 1
  init_timer(&one_shot, "once", 200, 0, 0, cb_once, 0);
  init_timer(&per50, "per50", 50, 1, 1, cb_50, &fires50);
  create_task(stack_a, starter_task, STACK_SIZE, 4, "starter");
#elif DEMO == 2
  init_timer(&per10, "per10", 10, 1, 1, cb_10, 0);
  create_task(stack_a, starter_task, STACK_SIZE, 4, "starter");
#elif DEMO == 3
  init_timer(&tmr, "tmr", 100, 1, 1, cb_named, 0);
  init_timer(&kami, "kami", 80, 1, 1, cb_kami, 0);
  create_task(stack_a, narrator_task, STACK_SIZE, 8, "narrator");
#elif DEMO == 4
  init_timer(&shot30, "shot30", 30, 0, 0, cb_shot, 0);
  create_task(stack_a, starter_task, STACK_SIZE, 4, "starter");
#elif DEMO == 5
  init_timer(&ta, "A30", 30, 1, 1, cb_named, 0);
  init_timer(&tb, "B50", 50, 1, 1, cb_named, 0);
  init_timer(&tc, "C70", 70, 1, 1, cb_named, 0);
  create_task(stack_a, starter_task, STACK_SIZE, 4, "starter");
#elif DEMO == 6
  init_timer(&per500, "per500", 500, 1, 1, cb_500, 0);
  create_task(stack_a, starter_task, STACK_SIZE, 4, "starter");
#elif DEMO == 7
  init_timer(&per50, "jit50", 50, 1, 1, cb_jit, 0);
  create_task(stack_a, hog_task,    STACK_SIZE, 5, "hog");
  create_task(stack_b, crunch_task, STACK_SIZE, 2, "crunch");
#elif DEMO == 8
  init_timer(&tk, "catch", 50, 1, 1, cb_pol, &last_tk);
  init_timer(&ts, "skip",  50, 1, 0, cb_pol, &last_ts);
  create_task(stack_a, starver_task, STACK_SIZE, 8, "starver");
#endif
  create_task(stack_mon, monitor_task, STACK_SIZE, 1, "monitor");

  gpt_init();
  lpm_init();
#if DEMO == 4
  gpt2_init();
#endif
  init_mpu();
  start_scheduler();

  while (1) {
    __asm volatile ("nop");
  }
}
