#pragma once

#include <stdint.h>

#define NUM_IRQS 160u

typedef void (*irq_handler_fn)(void* arg);

void driver_irq_enable(uint32_t irq, uint8_t priority);
void driver_irq_disable(uint32_t irq);
int driver_irq_connect(uint32_t irq, irq_handler_fn handler, void* arg);
void irq_dispatch(void);
