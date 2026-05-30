#pragma once
//#include "imxrt1062.h"
/* Teensy 4.1 board pin map. */

#define LED_PIN   (1u << 3)   /* pin 13 = GPIO2_IO03 */

/* Logic-analyzer scope pins, reserved for later exercises:
   14 = SysTick, 15 = PendSV, 16 = Task A, 17 = Task B. */
#define SCOPE_PIN (1u << 18)
