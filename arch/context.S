  .syntax unified
  .cpu cortex-m7
  .thumb
  .section .text

  .global SVC_Handler
  .thumb_func
SVC_Handler:
  ldr   r0, =curr_tcb
  ldr   r0, [r0]
  ldr   r0, [r0]
  ldmia r0!, {r4-r11}
  msr   psp, r0
  movs  r1, #2
  msr   control, r1
  isb
  ldr   lr, =0xFFFFFFFD
  bx lr

  .global PendSV_Handler
  .thumb_func
PendSV_Handler:
  mrs   r0, psp
  stmdb r0!, {r4-r11}

  ldr   r1, =curr_tcb
  ldr   r2, [r1]
  str   r0, [r2]

  push  {r1, lr}
  bl    schedule
  pop   {r1, lr}

  ldr   r2, [r1]
  ldr   r0, [r2]
  ldmia r0!, {r4-r11}
  msr   psp, r0
  bx lr
