  .syntax unified
  .cpu cortex-m7
  .thumb

  .section .text
  .global context_switch
  .thumb_func
context_switch:
  @ void context_switch(uint32_t** old_sp, uint32_t* new_sp);
  push  {r4-r11, lr}
  str   sp, [r0]
  mov   sp, r1
  pop   {r4-r11, pc}
