#include "gpt.h"
#include <cm7.h>
#include <cronus/tasks.h>

#include <stdint.h>
#include <sys/reent.h>
#include <cronus/critical.h>
#include <systick.h>
#include <cronus/config.h>
#include <uart.h>
#include <mpu.h>

#define MAX_TASKS 8
#define MAX_PRIO  32
#define STACK_FILL 0xA5A5A5A5u
#define SW_CANARY_WIDTH 4u

#define HANDLE_IDX_BITS 16u
#define HANDLE_IDX_MASK ((1u << HANDLE_IDX_BITS) - 1u)

static uint32_t num_tasks = 0;

static tcb_t tcbs[MAX_TASKS];
tcb_t* curr_task;
tcb_t* idle_task_tcb;

static uint32_t ready_bits = 0;
static tcb_t* ready_lists[MAX_PRIO] = {0};

static tcb_t* delayed_list = NULL;
static tcb_t* delayed_list_overflow = NULL;

static uint32_t idle_stack[128] __attribute__((aligned(32)));
static void unlink_task_from_list(tcb_t* t);

static volatile uint32_t scheduler_suspended = 0;
static volatile uint32_t pended_ticks = 0;

static inline task_handle_t make_handle(uint16_t gen, uint32_t idx) {
  return ((task_handle_t)gen << HANDLE_IDX_BITS) | (idx & HANDLE_IDX_MASK);
}

extern uint32_t* init_task(uint32_t* stack_top, void (*entry)(void), void (*task_return)(void));

__attribute__((weak))
void idle_hook() {}

void yield(void) {
  SCB_ICSR = ICSR_PENDSVSET;
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
}

/* Resolve a handle to a LIVE TCB, or NULL if it's stale/invalid.  Three rejects:
 *   1. index out of range      -> never allocated
 *   2. slot is TASK_DEAD        -> task died, slot not yet reused
 *   3. generation mismatch      -> slot WAS reused since the handle was minted (the ABA case)
 * All three collapse "is this handle still meaningful?" into one cheap check.             */
static tcb_t* resolve_task_handle(task_handle_t h) {
  if(h == NULL_TASK_HANDLE) {
    return NULL;
  }

  uint16_t gen = (uint16_t)(h >> HANDLE_IDX_BITS);
  uint16_t idx = h & HANDLE_IDX_MASK;

  if(idx >= num_tasks) {
    return NULL;
  }

  tcb_t* t = &tcbs[idx];
  if(t->state == TASK_DEAD) {
    return NULL;
  }

  if(t->generation != gen) {
    return NULL;
  }

  return t;
}

task_handle_t get_task_handle(void) {
  if(curr_task == NULL) {
    return NULL_TASK_HANDLE;
  }

  return make_handle(curr_task->generation, (uint32_t)(curr_task - tcbs));
}

void task_delete(task_handle_t h) {
  enter_critical();
    tcb_t* t = resolve_task_handle(h);
    if(t == NULL || t == idle_task_tcb) {
      exit_critical();
      return;
    }
    unlink_task_from_list(t);
    t->state = TASK_DEAD;

    if(t == curr_task) {
      yield();
      exit_critical();
      /*
       * control never reaches here
       * just a guard
       */
      for (;;) {
        __asm volatile ("nop");
      }
    }
  exit_critical();
}

void task_return(void) {
  task_delete(get_task_handle()); // get_handle returns the current task's handle
  for(;;) {
    __asm volatile ("nop");
  }
}

#if CONFIG_USE_TICKLESS_IDLE
static void tickless_sleep(uint32_t idle_ticks) {
  uint32_t per_tick = (GPT_CLK_HZ/TICK_HZ);            /* 24000 GPT clocks / 1 ms */
  uint32_t max_ticks = 0xFFFFFFFFu / per_tick;         /* 32-bit counter headroom */

  if(idle_ticks > max_ticks) {
    idle_ticks = max_ticks;
  }
  uint32_t interval = idle_ticks * per_tick;

  systick_set_tickless(1);
  SYST_CSR = 0;             /* stop 1 ms tick; GPT keeps time */

  gpt_start(interval);
  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("wfi" ::: "memory");

  uint32_t elapsed = gpt_stop_return_elapsed();
  uint32_t slept_ticks = elapsed / per_tick;
  advance_tick_count(slept_ticks);

  /* restore 1 ms tick */
  SYST_CSR = 0;
  SYST_RVR = (SYSTICK_REF_HZ / TICK_HZ) - 1u;
  SYST_CSR = SYST_CSR_TICKINT | SYST_CSR_ENABLE;

  /* The long interval's expiry left a SysTick exception pending; drop it.
   * and run the SysTick_Handler manually wihtout the tick_count++
   * because we already advanced the right number of ticks
   */
  SCB_ICSR = ICSR_PENDSTCLR;
  systick_set_tickless(0);

  kernel_tick(get_tick_count());
  SCB_ICSR = ICSR_PENDSVSET;
}

static uint32_t get_next_wake(void) {
  if(delayed_list != NULL) {
    int32_t delta = delayed_list->wake - get_tick_count();
    if(delta > 0) {
      return (uint32_t)delta;
    } else {
      return 0u;
    }
  }

  /* Current epoch empty, but sleepers wait in the NEXT epoch — their wake
       wrapped below 'now', so they sit on the overflow list. Ticks until the
       soonest of them = (ticks until the counter wraps to 0) + (its wrapped wake). */
  if(delayed_list_overflow != NULL) {
    uint32_t delta = (uint32_t)(0u - get_tick_count()) + delayed_list_overflow->wake;
    return delta;
  }

  return 0xFFFFFFFFu;
}
#endif

__attribute__((unused))
static void task_cleaner(void) {
  /*
   * task_cleaner is just a place holder
   * for when a dynamic-stack is implemented.
   * It does nothing of value at the moment.
   */
}

void idle_task() {
  while(1) {
#if CONFIG_USE_IDLE_HOOK
    idle_hook();
#endif
#if CONFIG_USE_TICKLESS_IDLE
  if(get_next_wake() > TICKLESS_THRESHOLD) {
    /*
     * We use PRIMASK here because BASEPRI will ignore all interrups
     * that are below MAX_SYSCALL_PRIO so the system will not wake for
     * systick exception.
     *
     * When PRIMASK is set to 1 (interrupts disabled), the CPU will still
     * wake up from a WFI instruction when an eligible interrupt arrives.
     *
     * However, instead of jumping straight to the Interrupt Service Routine (ISR),
     * the CPU bypasses the handler and resumes execution at the very next line
     * of code following the WFI instruction. Which is what we want.
     * We want the code to continue execution after __asm volatile ("wfi");
     * in tickless_sleep.
     */
    enter_critical_primask();
      // because a tick may've fired
      uint32_t delta = get_next_wake();
      if(delta > TICKLESS_THRESHOLD) {
        tickless_sleep(delta);
      }
   exit_critical_primask();
  }
#else
  __asm volatile ("wfi");
#endif
  }
}

static void insert_ready_task(tcb_t* t) {
  if(t == NULL) {
    return;
  }

  uint8_t p = t->priority;
  if(ready_lists[p] == NULL) {
    ready_lists[p] = t;
    t->next = t->prev = t;
  } else {
    /* Insert t between head and tail */
    tcb_t* head = ready_lists[p];
    tcb_t* tail = head->prev;
    t->next = head;
    t->prev = tail;

    tail->next = t;
    head->prev = t;
  }

  ready_bits |= (1u << p);
}

static void remove_ready_task(tcb_t* t) {
  if(t == NULL) {
    return;
  }

  uint8_t p = t->priority;
  if(t->next == t) {
    ready_lists[p] = NULL;
    ready_bits &= ~(1u << p);
  } else {
    if(ready_lists[p] == t) {
      ready_lists[p] = t->next;
    }

    t->next->prev = t->prev;
    t->prev->next = t->next;
  }

  t->next = t->prev = NULL;
}

static void remove_delayed_task(tcb_t* t) {
  tcb_t** pp = &delayed_list;
  while(*pp && *pp != t) {
    pp = &(*pp)->next;
  }

  if(!*pp) {
    pp = &delayed_list_overflow;
    while(*pp && *pp != t) {
      pp = &(*pp)->next;
    }
  }

  if(*pp) {
    *pp = t->next;
    t->next = NULL;
  }
}

static void unlink_task_from_list(tcb_t* t) {
  switch (t->state) {
    case TASK_RUNNING:
    case TASK_READY:
      remove_ready_task(t);
      break;

    case TASK_SLEEP:
      remove_delayed_task(t);
      break;

    default:
    break;
  }
}

static tcb_t* pick_next_task() {
  if(ready_bits == 0) {
    return NULL;
  }

  uint32_t high_p = 31 - __builtin_clz(ready_bits);
  tcb_t* t = ready_lists[high_p];
  ready_lists[high_p] = t->next;

  return t;
}

static void insert_delayed_into(tcb_t** list, tcb_t* t) {
  t->next = NULL;
  if(*list == NULL || t->wake < (*list)->wake) {
    t->next = *list;
    *list = t;
    return;
  }

  tcb_t* curr = *list;
  while(curr->next != NULL && t->wake > curr->next->wake) {
    curr = curr->next;
  }

  t->next = curr->next;
  curr->next = t;
}

static void insert_delayed_task(tcb_t* t, uint32_t now) {
  if(t->wake < now) {
    insert_delayed_into(&delayed_list_overflow, t);
  } else {
    insert_delayed_into(&delayed_list, t);
  }
}

void task_delay(uint32_t ms) {
  if(ms == 0) {
    yield();
    return;
  }

  enter_critical();
    uint32_t now = get_tick_count();
    unlink_task_from_list(curr_task);
    curr_task->wake = now + ms;
    curr_task->state = TASK_SLEEP;
    insert_delayed_task(curr_task, now);
  exit_critical();

  yield();
}

/* drift resistant delay */
void task_delay_until(uint32_t* last_wake, uint32_t ms) {
  if(ms == 0) {
    yield();
    return;
  }
  if(last_wake == NULL) {
    task_delay(ms);
    return;
  }

  enter_critical();
  uint32_t now = get_tick_count();
  uint32_t wake = *last_wake + ms;
  *last_wake = wake;
  if((int32_t)(wake - now) > 0) {
    curr_task->wake = wake;
    curr_task->state = TASK_SLEEP;
    unlink_task_from_list(curr_task);
    insert_delayed_task(curr_task, now);
    exit_critical();
    yield();
  } else {
    exit_critical();
  }
}

void suspend_task(task_handle_t h) {
  enter_critical();
    tcb_t* t = resolve_task_handle(h);
    if(t == NULL || t == idle_task_tcb) {
      exit_critical();
      return;
    }
    unlink_task_from_list(t);

    if(t->state == TASK_SLEEP) {
      int32_t left = (int32_t)(t->wake - get_tick_count());
      if(left > 0) {
        t->wake = left;
        t->resume_resleep = 1;
      } else {
        t->wake = 0u;
      }
    }

    t->state = TASK_SUSPEND;
    if(t == curr_task) {
      yield();
    }
  exit_critical();
}

void resume_task(task_handle_t h) {
  enter_critical();
    tcb_t* t = resolve_task_handle(h);
    if(t == NULL || t->state != TASK_SUSPEND) {
      exit_critical();
      return;
    }

    if(t->resume_resleep) {
      t->resume_resleep = 0;
      uint32_t now = get_tick_count();
      t->wake += now;
      t->state = TASK_SLEEP;
      insert_delayed_task(t, now);
    } else {
      t->state = TASK_READY;
      insert_ready_task(t);

      if(t->priority > curr_task->priority) {
        yield();
      }
    }

  exit_critical();
}

/* Returns 1 if a higher-priority task was readied -> caller should pend PendSV.            */
int resume_task_isr(task_handle_t h) {
  uint32_t bpr = enter_critical_isr();
    int yield = 0;
    tcb_t* t = resolve_task_handle(h);
    if(t == NULL || t->state != TASK_SUSPEND) {
      exit_critical_isr(bpr);
      return yield;
    }

    t->state = TASK_READY;
    insert_ready_task(t);
    if(t->priority > curr_task->priority) {
      yield = 1;
    }
  exit_critical_isr(bpr);
  return yield;
}

void kernel_tick(uint32_t now) {
  static uint32_t last_now = 0;

  if(scheduler_suspended) {
    pended_ticks++;
    return;
  }

  if(last_now > now) {
    while(delayed_list != NULL) {
      tcb_t* t = delayed_list;
      delayed_list = t->next;
      t->next = t->prev = NULL;
      t->state = TASK_READY;
      insert_ready_task(t);
    }

    delayed_list = delayed_list_overflow;
    delayed_list_overflow = NULL;
  }

  while(delayed_list != NULL && delayed_list->wake <= now) {
    tcb_t* t = delayed_list;
    delayed_list = t->next;
    t->next = t->prev = NULL;
    t->state = TASK_READY;
    insert_ready_task(t);
  }

  last_now = now;
}

task_handle_t create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size, uint8_t priority, const char* name) {
  if(stack_size < 2 || !stack || !entry || priority >= MAX_PRIO) {
    return NULL_TASK_HANDLE;
  }

  enter_critical();

    tcb_t* t = NULL;
    uint32_t idx = 0;
    for(uint32_t i = idx; i < num_tasks; i++) {
      if(tcbs[i].state == TASK_DEAD) {
        t = &tcbs[i];
        idx = i;
        break;
      }
    }

    if(!t) {
    if(num_tasks >= MAX_TASKS) {
      exit_critical();
      return NULL_TASK_HANDLE;
    }
    idx = num_tasks++;
    t = &tcbs[idx];
    }

    t->generation++;

    for (uint32_t i = 0; i < stack_size; i++) {
      stack[i] = STACK_FILL;
    }

    t->sp = init_task(stack + stack_size, entry, task_return);
    t->priority = priority;
    t->state = TASK_READY;
    t->stack_base = stack;
    t->stack_size = stack_size;
    t->name = name;
    insert_ready_task(t);

    task_handle_t h = make_handle(t->generation, idx);
  exit_critical();
  return h;
}

__attribute__((weak))
void stack_overflow_hook(const tcb_t* t) {
  uart_printf("Stack overflowed: Task %s, Priority: %u, Task @ %p\r\n", t->name, t->priority, t);
  for(;;) {
    __asm volatile ("nop");
  }
}

__attribute__((used))
static int stack_overflowed(const tcb_t* t) {
#if SW_STACK_GUARD_MODE
  /* MODE 1: is the sentinel band disturbed? */
  for(uint32_t i = 0; i < SW_CANARY_WIDTH; i++) {
    if(t->stack_base[i] != STACK_FILL) {
      return 1;
    }
  }
#endif

#if SW_STACK_GUARD_MODE > 1
  /* MODE 2 just check if the sp passde the stack_base */
  if(t->sp < t->stack_base) {
    return 1;
  }
#endif

  return 0;
}

int task_free_words(const tcb_t* t) {
  int free_words = 0;
  for (uint32_t* p = t->stack_base; p < t->stack_base + t->stack_size; p++) {
    if (*p == STACK_FILL) {
      free_words++;
    }
    else {
      break;
    }
  }
  return free_words;
}

void task_hwm(const tcb_t* t) {
  uart_printf("Task: %s, free stack: %d words\r\n", t->name, (unsigned)task_free_words(t));
}

void task_reporter() {
  for (uint32_t i = 0; i  < num_tasks; i++) {
#if STACK_GUARD
    /*
     * Skip the current task because, on STACK_GUARD, MPU is armed
     * and reading the stake_base with task_hwm will trigger
     * stack overflow.
     */
    if(&tcbs[i] == curr_task) {
      continue;
    }
#endif
    task_hwm(&tcbs[i]);
  }
}

void suspend_all_tasks(void) {
  scheduler_suspended++;
}

void resume_all_tasks(void) {
  enter_critical();
    if (--scheduler_suspended == 0) {
      while(pended_ticks) {
        kernel_tick(get_tick_count());
        pended_ticks--;
      }
    }
  exit_critical();
  yield();
  return;
}

void scheduler(void) {
  if(scheduler_suspended) {
    return;
  }

  if(curr_task == NULL) {
    return;
  }

#if !STACK_GUARD
  if(stack_overflowed(curr_task)) {
    stack_overflow_hook(curr_task);
  }
#endif

  if(curr_task->state == TASK_RUNNING) {
   curr_task->state = TASK_READY;
  }
  curr_task = pick_next_task();
  if(curr_task == NULL) {
    curr_task = idle_task_tcb;
  }
  curr_task->state = TASK_RUNNING;

#if STACK_GUARD
  set_mpu_stack_guard(curr_task->stack_base);
#endif
}

void start_scheduler(void) {
  create_task(idle_stack, idle_task, 128, 0, "Idle");
  idle_task_tcb = &tcbs[num_tasks - 1];

  SHPR3 = (SHPR3 & ~(0xFFu << 16)) | (0xFFu << 16);
  curr_task = pick_next_task();
  if(curr_task == NULL) {
    return;
  }
  curr_task->state = TASK_RUNNING;
  __asm volatile ("svc 0");
}
