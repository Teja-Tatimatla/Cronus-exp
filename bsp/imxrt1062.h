#pragma once
#include <stdint.h>
/* i.MX RT1062 peripheral registers (RM addresses) - CCM / IOMUXC / GPIO.
   Chip-specific, so they live in bsp/, not arch/. */

/* CCM - clocking */
#define CCM_CCGR0           (*(volatile uint32_t *)0x400FC068)
#define CCM_CBCDR           (*(volatile uint32_t *)0x400FC014)
#define CCM_ANALOG_PLL_ARM  (*(volatile uint32_t *)0x400D8000)
#define PERIPH_CLK_SEL      (1u << 25)

#define CCM_CCGR3           (*(volatile uint32_t *)0x400FC074)
#define CCM_CCGR1           (*(volatile uint32_t *)0x400FC06C)

#define CCM_CSCDR1          (*(volatile uint32_t *)0x400FC024)

#define CCM_CLPCR           (*(volatile uint32_t *)0x400FC054)

/* IOMUX - pin mux/pad for pin 0 (GPIO_AD_B0_03)*/
#define IOMUXC_MUX_AD_B0_03 (*(volatile uint32_t *)0x401F80C8)
#define IOMUXC_PAD_AD_B0_03 (*(volatile uint32_t *)0x401F82B8)

/* IOMUX LPUART Rx select daisy register */
#define IOMUXC_LPUART6_RX_DAISY_REGISTER  (*(volatile uint32_t *)0x401F8550)
#define IOMUXC_LPUART6_SET_DAISY_AD_B0_03 (1u << 0)

/* IOMUX - pin mux/pad for pin 1 (GPIO_AD_B0_02)*/
#define IOMUXC_MUX_AD_B0_02 (*(volatile uint32_t *)0x401F80C4)
#define IOMUXC_PAD_AD_B0_02 (*(volatile uint32_t *)0x401F82B4)

/* IOMUXC - pin mux/pad for pin 13 (GPIO_B0_03) */
#define IOMUXC_MUX_B0_03    (*(volatile uint32_t *)0x401F8148)
#define IOMUXC_PAD_B0_03    (*(volatile uint32_t *)0x401F8338)

/* IOMUX - pin mux/pad fro pin 14 (GPIO_AD_B1_02) */
#define IOMUXC_MUX_AD_B1_02  (*(volatile uint32_t *)0x401F8104)
#define IOMUXC_PAD_AD_B1_02  (*(volatile uint32_t *)0x401F82F4)

/* IOMUX - pin mux/pad fro pin 16 (GPIO_AD_B1_07) */
#define IOMUXC_MUX_AD_B1_07  (*(volatile uint32_t *)0x401F8118)
#define IOMUXC_PAD_AD_B1_07  (*(volatile uint32_t *)0x401F8308)

/* IOMUX - pin mux/pad fro pin 17 (GPIO_AD_B1_06) */
#define IOMUXC_MUX_AD_B1_06  (*(volatile uint32_t *)0x401F8114)
#define IOMUXC_PAD_AD_B1_06  (*(volatile uint32_t *)0x401F8304)

/* GPIO2 */
#define GPIO2_GDIR          (*(volatile uint32_t *)0x401BC004)
#define GPIO2_DR            (*(volatile uint32_t *)0x401BC000)

/* GPIO 6 */
#define GPIO6_GDIR          (*(volatile uint32_t *)0x42000004)
#define GPIO6_DR            (*(volatile uint32_t *)0x42000000)
#define IOMUXC_GPR_GPR26    (*(volatile uint32_t *)0x400AC068)
