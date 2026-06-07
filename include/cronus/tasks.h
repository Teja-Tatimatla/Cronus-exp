#pragma once

#include <stdint.h>

typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED
} task_state_e;

typedef struct tcb {
  uint32_t*    sp;
  uint8_t      priority;
  task_state_e state;
  struct tcb*  next;
  struct tcb*  prev;
} tcb_t;

void start_scheduler(void);
void create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size, uint8_t priority);
