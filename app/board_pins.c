#include <stdint.h>

#include <imxrt1062.h>
#include <board.h>
#include <cronus/pinctrl.h>

const struct pin_ctrl board_pins[] = {
  { .mux = &IOMUXC_MUX_B0_03,    .mux_val = 5, .pad = &IOMUXC_PAD_B0_03,    .pad_val = 0x10B0 },
  { .mux = &IOMUXC_MUX_AD_B1_02, .mux_val = 5, .pad = &IOMUXC_PAD_AD_B1_02, .pad_val = 0x10B0,
    .gpr = &IOMUXC_GPR_GPR26, .gpr_mask = SCOPE_PIN_14 },
  { .mux = &IOMUXC_MUX_AD_B1_07, .mux_val = 5, .pad = &IOMUXC_PAD_AD_B1_07, .pad_val = 0x10B0,
    .gpr = &IOMUXC_GPR_GPR26, .gpr_mask = SCOPE_PIN_16 },
  { .mux = &IOMUXC_MUX_AD_B1_06, .mux_val = 5, .pad = &IOMUXC_PAD_AD_B1_06, .pad_val = 0x10B0,
    .gpr = &IOMUXC_GPR_GPR26, .gpr_mask = SCOPE_PIN_17 },
};

const uint32_t board_pin_count = sizeof(board_pins) / sizeof(board_pins[0]);
