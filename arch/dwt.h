#pragma once
#include <stdint.h>
/* DWT cycle-counter timing (busy-wait). Honest delays, from Ex2. */

void init_dwt(void);
void delay_cycles(uint32_t cycles);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);
