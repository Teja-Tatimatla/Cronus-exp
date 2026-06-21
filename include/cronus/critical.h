#include <stdint.h>

void enter_critical(void);
void exit_critical(void);

uint32_t enter_critical_isr(void);
void exit_critical_isr(uint32_t v);

void enter_critical_primask(void);
void exit_critical_primask(void);
