#pragma once

#include <stdint.h>

typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_SLEEP,
  TASK_BLOCKED
} task_state_e;

typedef struct tcb {
  uint32_t*    sp;
  uint8_t      priority;
  task_state_e state;
  uint32_t     wake;
  struct tcb*  next;
  struct tcb*  prev;
} tcb_t;

void start_scheduler(void);
void create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size, uint8_t priority);
void kernel_tick(uint32_t now);
void task_delay(uint32_t ms);
void task_delay_until(uint32_t* last_wake, uint32_t ms);
