#include <stdint.h>

#include <cronus/pinctrl.h>

void pinctrl_apply(const struct pin_ctrl *pins, uint32_t num_pins) {
  for(uint32_t i = 0; i < num_pins; i++) {
    const struct pin_ctrl* pin = &pins[i];
    if(pin->mux) {
      *pin->mux = pin->mux_val;
    }

    if(pin->pad) {
      *pin->pad = pin->pad_val;
    }

    if(pin->daisy) {
      *pin->daisy = pin->daisy_val;
    }

    if(pin->gpr) {
      *pin->gpr |= pin->gpr_mask;
    }
  }
}
