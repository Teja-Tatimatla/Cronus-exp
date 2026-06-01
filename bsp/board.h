#pragma once
//#include "imxrt1062.h"
/* Teensy 4.1 board pin map. */

#define LED_PIN               (1u << 3)   /* pin 13 = GPIO2_IO03 */

/* Logic-analyzer scope pins, reserved for later exercises:
   14 = SysTick, 15 = PendSV, 16 = Task A, 17 = Task B. */
#define SCOPE_PIN_14          (1u << 18)
#define SCOPE_PIN_16          (1u << 23)
#define SCOPE_PIN_17          (1u << 22)


/* LPUART6 */
#define LPUART6_BASE           0x40198000
#define LPUART6_BAUD           (*(volatile uint32_t *)(LPUART6_BASE + 0x10))
#define LPUART6_STAT           (*(volatile uint32_t *)(LPUART6_BASE + 0x14))
#define LPUART6_CTRL           (*(volatile uint32_t *)(LPUART6_BASE + 0x18))
#define LPUART6_DATA           (*(volatile uint32_t *)(LPUART6_BASE + 0x1C))

#define LPUART6_STAT_TDRE      (1u << 23)
#define LPUART6_STAT_RDRF      (1u << 21)
#define LPUART6_STAT_OR        (1u << 19)

#define LPUART6_CTRL_RIE       (1u << 21)
#define LPUART6_CTRL_TE        (1u << 19)
#define LPUART6_CTRL_RE        (1u << 18)

#define LPUART6_IRQ            25
