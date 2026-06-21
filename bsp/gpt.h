#pragma once
#include <stdint.h>

/* GPT1 - i.MX RT1062 General Purpose Timer. Clocked from the always-on 24 MHz
   crystal and kept alive in WAIT/STOP via WAITEN/STOPEN/DOZEEN, so it can wake
   the core from a low-power mode - which a core-local timer (SysTick) cannot. */

#define GPT1_BASE          0x401EC000u
#define GPT1_CR            (*(volatile uint32_t *)(GPT1_BASE + 0x00))
#define GPT1_PR            (*(volatile uint32_t *)(GPT1_BASE + 0x04))
#define GPT1_SR            (*(volatile uint32_t *)(GPT1_BASE + 0x08))
#define GPT1_IR            (*(volatile uint32_t *)(GPT1_BASE + 0x0C))
#define GPT1_OCR1          (*(volatile uint32_t *)(GPT1_BASE + 0x10))
#define GPT1_CNT           (*(volatile uint32_t *)(GPT1_BASE + 0x24))

// Control Register
#define GPT_CR_EN         (1u << 0)
#define GPT_CR_ENMOD      (1u << 1)
#define GPT_CR_WAITEN     (1u << 3)
#define GPT_CR_DOZEEN     (1u << 4)
#define GPT_CR_STOPEN     (1u << 5)
#define GPT_CR_CLKSRC_24M (0x5u << 6)
#define GPT_CR_FRR        (1u << 9)
#define GPT_CR_EN_24M     (1u << 10)
#define GPT_CR_SWR        (1u << 15)

// Status Register
#define GPT_SR_OF1        (1u << 0)

// Interrupt Register
#define GPT_IR_OF1IE      (1u << 0)

#define GPT_CLK_HZ        24000000u
#define GPT1_IRQ           100u

void gpt_init(void);
void gpt_start(uint32_t clocks);
uint32_t gpt_stop_return_elapsed(void);
