#include <stdint.h>

uint32_t* init_task(uint32_t* stack_top, void (*entry)(void), void (*task_return)(void)) {
  uint32_t* sp = stack_top;
  *(--sp) = 0x01000000; // xPSR
  *(--sp) = (uint32_t)entry & ~1u;  // PC
  *(--sp) = (uint32_t)task_return;  // LR
  *(--sp) = 0;  // r12
  *(--sp) = 0;  // r3
  *(--sp) = 0;  // r2
  *(--sp) = 0;  // r1
  *(--sp) = 0;  // r0

  for(int i = 0; i < 8; i++) {
    *(--sp) = 0;
  }

  return sp;
}
