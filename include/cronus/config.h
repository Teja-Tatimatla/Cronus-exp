#pragma once
/* Cronus kernel configuration - the knobs. */

#include <stdint.h>

#define CORE_HZ   600000000u   /* core clock after switch_clock_600mhz() */
#define TICK_HZ   1000u
_Static_assert(CORE_HZ % TICK_HZ == 0,
               "TICK_HZ must divide CORE_HZ evenly, or the tick rate drifts");
_Static_assert((CORE_HZ / TICK_HZ) - 1u <= 0xFFFFFFu,
               "reload exceeds SysTick's 24-bit RVR");

#define CONFIG_USE_IDLE_HOOK 1
#define CONFIG_USE_TICKLESS_IDLE 1
#define TICKLESS_THRESHOLD 50u

/*
 * Tsis is from
 * Chapter 13 Clock and Power Management
 * The 24 MHz XTALOSC can be the external clock source
 * of SYSTICK. Hardware devides this down to 100KHz
 * before it reaches SYSTICK.
 */
#define SYSTICK_REF_HZ  100000u   /* 100KHz i.MX RT SysTick external ref: 24 MHz XTALOSC / 240 */

#define STACK_GUARD 1
#define SW_STACK_GUARD_MODE 2

#define CONFIG_NOTIFY_SLOTS 5
