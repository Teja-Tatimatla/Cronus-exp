#include <stdint.h>

#include <cm7.h>
#include <imxrt1062.h>
/* C runtime bring-up: copy .data, zero .bss, call main. */

extern uint32_t _sdata, _edata, _sidata, _sbss, _ebss, _svector, _evector, _sivector, _sitcm, _eitcm, _siitcm;
extern const void *vector_table[];
int main(void);
void Reset_Handler(void);

#define CR_STR_(x)  #x
#define CR_STR(x) CR_STR_(x)

__attribute__((section(".bootcode"), naked, used, noreturn))
void Reset_Trampoline(void) {
  __asm volatile (
    "ldr r1, =" CR_STR(IOMUXC_GPR_GPR17_ADDR) "\n"
    "ldr r2, =" CR_STR(FLEXRAM_BANK_CFG)      "\n"
    "str r2, [r1]                              \n"
    "ldr r1, =" CR_STR(IOMUXC_GPR_GPR16_ADDR) "\n"
    "ldr r2, [r1]                              \n"
    "orr r2, r2, #4                            \n"
    "str r2, [r1]                              \n"
    "dsb                                       \n"
    "isb                                       \n"
    "ldr r1, =_estack                          \n"
    "msr msp, r1                               \n"
    "isb                                       \n"
    "b Reset_Handler                           \n"
    ".ltorg                                    \n"
  );
}

__attribute__((section(".bootcode"), used))
void Reset_Handler(void) {
  volatile uint32_t *src, *dst;

  /* vectors: flash -> ITCM */
  src = (volatile uint32_t *)&_sivector;
  dst = (volatile uint32_t *)&_svector;
  while (dst < (volatile uint32_t *)&_evector) {
    *dst++ = *src++;
  }

  /* .itcm.text: flash -> ITCM */
  src = (volatile uint32_t *)&_siitcm;
  dst = (volatile uint32_t *)&_sitcm;
  while (dst < (volatile uint32_t *)&_eitcm) {
    *dst++ = *src++;
  }

  /* .data: flash -> DTCM */
  src = (volatile uint32_t *)&_sidata;
  dst = (volatile uint32_t *)&_sdata;
  while (dst < (volatile uint32_t *)&_edata) {
    *dst++ = *src++;
  }

  /* .bss: zero */
  dst = (volatile uint32_t *)&_sbss;
  while (dst < (volatile uint32_t *)&_ebss) {
    *dst++ = 0;
  }

  SCB_VTOR = (uint32_t)vector_table;

  main();

  while (1);
}
