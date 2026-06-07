#pragma once

#include <stdint.h>

typedef struct {
  uint32_t* sp;
} tcb_t;

void start_schedule(void);
void create_task(uint32_t* stack, void (*entry)(void), uint32_t stack_size);
