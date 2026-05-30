#include <stdint.h>

#include <cm7.h>
/* C runtime bring-up: copy .data, zero .bss, call main. */

extern uint32_t _sdata, _edata, _sidata, _sbss, _ebss;
extern const void *vector_table[];
int main(void);

void reset_handler(void) {
  uint32_t *src  = &_sidata;
  uint32_t *dest = &_sdata;
  while (dest < &_edata) {
    *dest++ = *src++;
  }

  dest = &_sbss;
  while (dest < &_ebss) {
    *dest++ = 0;
  }

  SCB_VTOR = (uint32_t)vector_table;

  main();

  while (1);
}
