#pragma once

#include "cronus/config.h"
#include <stdint.h>

typedef uint32_t task_handle_t;
#define NULL_TASK_HANDLE ((task_handle_t)0)

typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_SLEEP,
  TASK_BLOCKED,
  TASK_SUSPEND,
  TASK_DEAD
} task_state_e;

typedef enum {
  NOTIFY_IDLE = 0,
  NOTIFY_WAITING,
  NOTIFY_PENDING
} notify_state_e;

typedef enum {
  NOTIFY_NO_ACTION = 0,
  NOTIFY_SET_BITS,
  NOTIFY_INCREMENT,
  NOTIFY_SET_VALUE_OVERWRITE,
  NOTIFY_SET_VALUE_NO_OVERWRITE
} notify_action_e;

#define TASK_WAIT_FOREVER 0xFFFFFFFFu

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
  struct tcb*  ev_next;
  struct tcb*  ev_prev;
  struct tcb** wait_head;
  int          wait_result;
  uint32_t     notify_value[CONFIG_NOTIFY_SLOTS];
  uint8_t      notify_state[CONFIG_NOTIFY_SLOTS];
} tcb_t;


void start_scheduler(void);
task_handle_t create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size, uint8_t priority, const char* name);
void kernel_tick(uint32_t now);
void task_delay(uint32_t ms);
void task_delay_until(uint32_t* last_wake, uint32_t ms);
void task_reporter();
void yield(void);

void unlink_task_from_list(tcb_t* t);
void insert_delayed_task(tcb_t* t, uint32_t now);
void insert_ready_task(tcb_t* t);

task_handle_t get_task_handle(void);
void suspend_task(task_handle_t h);
void resume_task(task_handle_t h);
int resume_task_isr(task_handle_t h);
void suspend_all_tasks(void);
void resume_all_tasks(void);
void task_delete(task_handle_t h);
void task_return(void);

// task_notications
  // indexed
uint32_t notify_take_indexed(uint32_t slot, uint32_t time_out, int clear_on_exit);
uint32_t notify_wait_indexed(uint32_t slot, int clear_on_entry, int clear_on_exit, uint32_t* out, uint32_t time_out);
int notify_indexed(task_handle_t h, uint32_t slot, uint32_t value, notify_action_e action);
uint32_t notify_indexed_from_isr(task_handle_t h, uint32_t slot, uint32_t value, notify_action_e action);
  // non-indexed
static inline uint32_t notify_take(uint32_t time_out, int clear_on_exit) {
  return notify_take_indexed(0, time_out, clear_on_exit);
}

static inline uint32_t notify_wait(int clear_on_entry, int clear_on_exit, uint32_t* out, uint32_t time_out) {
  return notify_wait_indexed(0, clear_on_entry, clear_on_exit, out, time_out);
}

static inline int notify(task_handle_t h, uint32_t value, notify_action_e action) {
  return notify_indexed(h, 0, value, action);
}

static inline uint32_t notify_from_isr(task_handle_t h, uint32_t value, notify_action_e action) {
  return notify_indexed_from_isr(h, 0, value, action);
}
