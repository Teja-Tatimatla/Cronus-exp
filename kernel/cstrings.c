#include <stdint.h>
#include <reent.h>

void* cr_memcpy(void* dest, const void* src, unsigned long n) {
  if(dest == NULL || src == NULL || n == 0) {
    return NULL;
  }

  unsigned char* d = dest;
  const unsigned char* s = src;
  while(n--) {
    *d++ = *s++;
  }
  return d;
}
