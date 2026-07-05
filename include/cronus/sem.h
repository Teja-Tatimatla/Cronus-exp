#pragma once
#include <stdint.h>
#include <cronus/tasks.h>

#define SEM_WAIT_FOREVER 0xFFFFFFFFu

typedef struct semaphore {
  volatile int32_t count;
  int32_t          max_count;
  tcb_t*           wait_list;
} semaphore_t;


void sem_init(semaphore_t* s, int32_t initial, int32_t max_count);
void sem_give(semaphore_t* s);
int sem_take(semaphore_t* s, uint32_t timeout_ms);
