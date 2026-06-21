#include "gpt.h"
#include <cm7.h>
#include <cronus/tasks.h>

#include <stdint.h>
#include <sys/reent.h>
#include <cronus/critical.h>
#include <systick.h>
#include <cronus/config.h>

#define MAX_TASKS 8
#define MAX_PRIO  32
static uint32_t num_tasks = 0;

static tcb_t tcbs[MAX_TASKS];
tcb_t* curr_task;

static uint32_t ready_bits = 0;
static tcb_t* ready_lists[MAX_PRIO] = {0};

static tcb_t* delayed_list = NULL;
static tcb_t* delayed_list_overflow = NULL;

static uint32_t idle_stack[128] __attribute__((aligned(8)));

extern uint32_t* init_task(uint32_t* stack_top, void (*entry)(void), void (*task_return)(void));

__attribute__((weak))
void idle_hook() {}

void task_return(void) {
  while(1) {
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
  __asm volatile ("dsb");
  __asm volatile ("wfi");

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

void yield(void) {
  SCB_ICSR = ICSR_PENDSVSET;
  __asm volatile ("dsb");
  __asm volatile ("isb");
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
    curr_task->wake = now + ms;
    curr_task->state = TASK_SLEEP;
    remove_ready_task(curr_task);
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
    remove_ready_task(curr_task);
    insert_delayed_task(curr_task, now);
    exit_critical();
    yield();
  } else {
    exit_critical();
  }
}

void kernel_tick(uint32_t now) {
  static uint32_t last_now = 0;

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

void start_scheduler(void) {
  create_task(idle_stack, idle_task, 128, 0);

  SHPR3 = (SHPR3 & ~(0xFFu << 16)) | (0xFFu << 16);
  curr_task = pick_next_task();
  if(curr_task == NULL) {
    return;
  }
  curr_task->state = TASK_RUNNING;
  __asm volatile ("svc 0");
}

void create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size, uint8_t priority) {
  if(stack_size < 2 || !stack || !entry ||
    num_tasks >= MAX_TASKS || priority >= MAX_PRIO) {
    return;
  }

  tcb_t* t = &tcbs[num_tasks++];
  t->sp = init_task(stack + stack_size, entry, task_return);
  t->priority = priority;
  t->state = TASK_READY;

  insert_ready_task(t);
}

void scheduler(void) {
  if(curr_task == NULL) {
    return;
  }

  if(curr_task->state == TASK_RUNNING) {
   curr_task->state = TASK_READY;
  }
  curr_task = pick_next_task();
  if(curr_task == NULL) {
    return;
  }
  curr_task->state = TASK_RUNNING;
}
