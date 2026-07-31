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

int cr_strcmp(const char *a, const char *b) {
  if(a == NULL || b == NULL) {
    return (a == b) ? 1 : 0;
  }

  while(*a && (*a == *b)) {
    a++;
    b++;
  }

  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
