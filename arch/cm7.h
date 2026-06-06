#pragma once
#include <stdint.h>
/* ARM Cortex-M7 (ARMv7-M) core registers - System Control Space (0xE000_xxxx).
   Architectural: identical on any Cortex-M7, so they live in arch/, not bsp/. */

#define SCB_VTOR           (*(volatile uint32_t *)0xE000ED08)

/* Debug Exception and Monitor Control */
#define DEMCR              (*(volatile uint32_t *)0xE000EDFC)
#define DEMCR_TRCENA       (1u << 24)

/* Data Watchpoint and Trace - cycle counter */
#define DWT_CTRL           (*(volatile uint32_t *)0xE0001000)
#define DWT_CTRL_CYCCNTENA (1u << 0)
#define DWT_CYCCNT         (*(volatile uint32_t *)0xE0001004)

/* SysTick */
#define SYST_CSR           (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR           (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR           (*(volatile uint32_t *)0xE000E018)

#define SHPR3              (*(volatile uint32_t *)0xE000ED20)

#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_TICKINT   (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)

/* NVIC Interrupt Set-Enabled Registers */
#define NVIC_ISER(n)       (*(volatile uint32_t *)(0xE000E100 + 4u*(n)))

/* PendSV */
#define SCB_ICSR           (*(volatile uint32_t *)0xE000ED04)
#define ICSR_PENDSVSET     (1u << 28)
