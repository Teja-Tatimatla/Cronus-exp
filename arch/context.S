  .syntax unified
  .cpu cortex-m7
  .thumb
  .section .text

  .global SVC_Handler
  .thumb_func
SVC_Handler:
  ldr   r0, =curr_task
  ldr   r0, [r0]
  ldr   r0, [r0]
  ldmia r0!, {r4-r11, lr}
  tst   lr, #0x10
  it    eq
  vldmiaeq r0!, {s16-s31}
  msr   psp, r0
  movs  r1, #2
  msr   control, r1
  isb
  bx lr

  .global PendSV_Handler
  .thumb_func
PendSV_Handler:
  mrs   r0, psp
  tst   lr, 0x10
  it    eq
  vstmdbeq  r0!, {s16-s31}
  stmdb r0!, {r4-r11, lr}

  ldr   r1, =curr_task
  ldr   r2, [r1]
  str   r0, [r2]

  push  {r1, lr}
  bl    scheduler
  pop   {r1, lr}

  ldr   r2, [r1]
  ldr   r0, [r2]
  ldmia r0!, {r4-r11, lr}
  tst   lr, 0x10
  it    eq
  vldmiaeq r0!, {s16-s31}
  msr   psp, r0
  bx lr
