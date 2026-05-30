#pragma once
/* Cronus kernel configuration - the knobs. */

#include <stdint.h>

#define CORE_HZ   600000000u   /* core clock after switch_clock_600mhz() */
#define TICK_HZ   1000u
_Static_assert(CORE_HZ % TICK_HZ == 0,
               "TICK_HZ must divide CORE_HZ evenly, or the tick rate drifts");
_Static_assert((CORE_HZ / TICK_HZ) - 1u <= 0xFFFFFFu,
               "reload exceeds SysTick's 24-bit RVR");
