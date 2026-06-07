#include <cm7.h>
#include <cronus/tasks.h>

#include <stdint.h>

#define MAX_TASKS 8
static uint32_t num_tasks = 0;
static uint32_t curr_index = 0;

static tcb_t tcbs[MAX_TASKS];
tcb_t* curr_tcb;

extern uint32_t* init_task(uint32_t* stack_top, void (*entry)(void), void (*task_return)(void));

void task_return(void) {
  while(1) {
    __asm volatile ("nop");
  }
}

void start_schedule(void) {
  SHPR3 = (SHPR3 & ~(0xFFu << 16)) | (0xFFu << 16);
  curr_index = 0;
  curr_tcb = &tcbs[curr_index];
  __asm volatile ("svc 0");
}

void create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size) {
  if(!stack_size || stack_size < 2 || !stack || !entry) {
    return;
  }

  tcb_t* t = &tcbs[num_tasks++];
  t->sp = init_task(stack + stack_size, entry, task_return);
}

void schedule(void) {
  curr_index = (curr_index + 1) % num_tasks;
  curr_tcb = &tcbs[curr_index];
}
