#include <stdint.h>

#include <cm7.h>
/* C runtime bring-up: copy .data, zero .bss, call main. */

extern uint32_t _sdata, _edata, _sidata, _sbss, _ebss, _stext, _etext, _sitext, _svector, _evector, _sivector;
extern const void *vector_table[];
int main(void);

__attribute__((section(".bootcode"), used))
void Reset_Handler(void) {
  volatile uint32_t *src, *dst;

  /* vectors: flash -> ITCM */
  src = (volatile uint32_t *)&_sivector;
  dst = (volatile uint32_t *)&_svector;
  while (dst < (volatile uint32_t *)&_evector) {
    *dst++ = *src++;
  }

  /* code + rodata: flash -> ITCM */
  src = (volatile uint32_t *)&_sitext;
  dst = (volatile uint32_t *)&_stext;
  while (dst < (volatile uint32_t *)&_etext) {
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
