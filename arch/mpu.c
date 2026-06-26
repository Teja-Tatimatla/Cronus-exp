#include <stdint.h>

#include <cm7.h>

#define MPU_TYPE            (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL            (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR             (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR            (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR            (*(volatile uint32_t *)0xE000EDA0)

#define MPU_TYPE_DREGION    (0xFFu << 8)

#define MPU_CTRL_PRIVDEFENA (1u << 2)
#define MPU_CTL_ENABLE      (1u << 0)

#define MPU_RNR_REGION(n)   (((uint32_t)(n) & 0xFu) << 0)

#define MPU_RASR_XN         (1u << 28)
#define MPU_RASR_AP(ap)     ((uint32_t)(ap) << 24)
#define MPU_RASR_SRD(srd)   (((uint32_t)(srd) & 0xFFu) << 8)
#define MPU_RASR_SIZE(n)    (((uint32_t)(n) & 0x1Fu) << 1)
#define MPU_RASR_ENABLE     (1u << 0)

#define SHCSR_MEMFAULTENA   (1u << 16)

#define GUARD_BYTES         32u

void init_mpu() {
  if((MPU_TYPE & MPU_TYPE_DREGION) == 0u) {
    return;
  }

  MPU_CTRL = 0;
  SHCSR |= SHCSR_MEMFAULTENA;
  MPU_CTRL = MPU_CTRL_PRIVDEFENA | MPU_CTL_ENABLE;

  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
}

void set_mpu_stack_guard(const uint32_t* base) {
  uint32_t addr = (uint32_t)base & ~(GUARD_BYTES - 1u);

  MPU_RNR = MPU_RNR_REGION(0);
  MPU_RBAR = addr;

  MPU_RASR = MPU_RASR_XN | MPU_RASR_AP(0) | MPU_RASR_SRD(0xFEu)
            | MPU_RASR_SIZE(4) | MPU_RASR_ENABLE;

  __asm volatile ("dsb" ::: "memory");
  __asm volatile ("isb" ::: "memory");
}
