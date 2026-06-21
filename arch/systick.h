#pragma once

#include <stdint.h>

void init_sys_tick(void);
uint32_t get_tick_count(void);

void systick_set_tickless(uint8_t on);
void advance_tick_count(uint32_t ticks);
