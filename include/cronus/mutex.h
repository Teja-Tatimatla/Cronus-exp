#pragma once

#include <stdint.h>
#include <cronus/tasks.h>

typedef struct mutex {
  tcb_t*        owner;
  tcb_t*        wait_list;
  uint32_t      recursion_count;
  uint8_t       recursive;
  struct mutex* next_held;
} mutex_t;

void init_mutex(mutex_t* m);
void init_mutex_recursive(mutex_t* m);
int mutex_lock(mutex_t* m, uint32_t time_out);
int mutex_unlock(mutex_t* m);
