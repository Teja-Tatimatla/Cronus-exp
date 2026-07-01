#pragma once

#include <stdint.h>

typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_SLEEP,
  TASK_BLOCKED,
  TASK_SUSPEND,
  TASK_DEAD
} task_state_e;

typedef uint32_t task_handle_t;
#define NULL_TASK_HANDLE ((task_handle_t)0)

typedef struct tcb {
  uint32_t*    sp;
  uint8_t      priority;
  uint32_t*    stack_base;
  uint32_t     stack_size;
  const char*  name;
  task_state_e state;
  uint32_t     wake;
  uint16_t     generation;
  uint8_t      resume_resleep;
  struct tcb*  next;
  struct tcb*  prev;
} tcb_t;


void start_scheduler(void);
task_handle_t create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size, uint8_t priority, const char* name);
void kernel_tick(uint32_t now);
void task_delay(uint32_t ms);
void task_delay_until(uint32_t* last_wake, uint32_t ms);
void task_reporter();
void yield(void);

task_handle_t get_task_handle(void);
void suspend_task(task_handle_t h);
void resume_task(task_handle_t h);
int resume_task_isr(task_handle_t h);
void suspend_all_tasks(void);
void resume_all_tasks(void);
void task_delete(task_handle_t h);
void task_return(void);
