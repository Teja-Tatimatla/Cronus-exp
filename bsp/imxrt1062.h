#pragma once
#include <stdint.h>
/* i.MX RT1062 peripheral registers (RM addresses) - CCM / IOMUXC / GPIO.
   Chip-specific, so they live in bsp/, not arch/. */

/* CCM - clocking */
#define CCM_CCGR0           (*(volatile uint32_t *)0x400FC068)
#define CCM_CBCDR           (*(volatile uint32_t *)0x400FC014)
#define CCM_ANALOG_PLL_ARM  (*(volatile uint32_t *)0x400D8000)
#define PERIPH_CLK_SEL      (1u << 25)

/* IOMUXC - pin mux/pad for pin 13 (GPIO_B0_03) */
#define IOMUXC_MUX_B0_03    (*(volatile uint32_t *)0x401F8148)
#define IOMUXC_PAD_B0_03    (*(volatile uint32_t *)0x401F8338)

/* GPIO2 */
#define GPIO2_GDIR          (*(volatile uint32_t *)0x401BC004)
#define GPIO2_DR            (*(volatile uint32_t *)0x401BC000)

/* IOMUX - pin mux/pad fro pin 14 (GPIO_AD_B1_02) */
#define IOMUXC_MUX_AD_B1_02 (*(volatile uint32_t *)0x401F8104)
#define IOMUXC_PAD_AD_B1_02  (*(volatile uint32_t *)0x401F82F4)


/* GPIO 6*/
#define GPIO6_GDIR          (*(volatile uint32_t *)0x42000004)
#define GPIO6_DR            (*(volatile uint32_t *)0x42000000)
#define IOMUXC_GPR_GRP26    (*(volatile uint32_t *)0x400AC068)
