#pragma once

#include <stdint.h>

void init_mpu();
void set_mpu_stack_guard(const uint32_t* base);
