#pragma once

#include <stdint.h>

struct pin_ctrl {
  volatile uint32_t* mux;
  volatile uint32_t* pad;
  volatile uint32_t* daisy;
  volatile uint32_t* gpr;
  uint32_t           mux_val;
  uint32_t           pad_val;
  uint32_t           daisy_val;
  uint32_t           gpr_mask;
};

extern const struct pin_ctrl board_pins[];
extern const uint32_t board_pin_count;

void pinctrl_apply(const struct pin_ctrl* pins, uint32_t num_pins);
