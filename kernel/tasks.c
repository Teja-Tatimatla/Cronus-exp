#include <cm7.h>
#include <cronus/tasks.h>

#include <stdint.h>
#include <sys/reent.h>

#define MAX_TASKS 8
#define MAX_PRIO  32
static uint32_t num_tasks = 0;

static tcb_t tcbs[MAX_TASKS];
tcb_t* curr_task;

static uint32_t ready_bits = 0;
static tcb_t* ready_lists[MAX_PRIO] = {0};

extern uint32_t* init_task(uint32_t* stack_top, void (*entry)(void), void (*task_return)(void));

void task_return(void) {
  while(1) {
    __asm volatile ("nop");
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

__attribute__((unused))
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

void start_scheduler(void) {
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
