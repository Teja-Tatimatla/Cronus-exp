#pragma once

#include <stdint.h>

struct timer;
typedef void (*timer_cb_t)(struct timer* t);
typedef void (*timer_pendfn_t)(void* arg1, uint32_t arg2);

typedef struct timer {
  const char*   name;
  void*         user_data;
  timer_cb_t    callback;
  uint32_t      expiry;
  uint32_t      period;
  uint32_t      fire_count;
  uint32_t      max_late;
  uint8_t       active;
  uint8_t       auto_reload;
  uint8_t       catch_up;
  uint16_t      generation;
  struct timer* next;
} timer_t;

void init_timer(timer_t* t, const char* name, uint32_t period, int auto_reload, int catch_up, timer_cb_t cb, void* user_data);
void init_timer_daemon(uint32_t* stack);
int timer_start(timer_t* t, uint32_t timeout);
int timer_stop(timer_t* t, uint32_t timeout);
int timer_reset(timer_t* t, uint32_t timeout);
int timer_delete(timer_t* t, uint32_t timeout);
int timer_reporter(uint32_t timeout);
int timer_change_period(timer_t* t, uint32_t new_period, uint32_t timeout);
int timer_pend_function(timer_pendfn_t fn, void* parg, uint32_t arg, uint32_t timeout);
int timer_start_from_isr(timer_t* t);
int timer_pend_function_from_isr(timer_pendfn_t fn, void* parg, uint32_t arg);
