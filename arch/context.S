  .syntax unified
  .cpu cortex-m7
  .thumb
  .section .text

  .global SVC_Handler
  .thumb_func
SVC_Handler:
  ldr   r0, =tasks_sp
  ldr   r1, =current
  ldr   r1, [r1]
  ldr   r0, [r0, r1, lsl #2]
  ldmia r0!, {r4-r11}
  msr   psp, r0
  movs  r2, #2
  msr   control, r2
  isb
  ldr   lr, =0xFFFFFFFD
  bx lr

  .global PendSV_Handler
  .thumb_func
PendSV_Handler:
  mrs   r0, psp
  stmdb r0!, {r4-r11}

  ldr   r1, =tasks_sp
  ldr   r2, =current
  ldr   r3, [r2]
  str   r0, [r1, r3, lsl #2]

  eor   r3, r3, #1
  str   r3, [r2]

  ldr   r0, [r1, r3, lsl #2]
  ldmia r0!, {r4-r11}
  msr   psp, r0
  bx lr
