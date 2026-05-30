# Thin convenience wrapper over cmake + ninja + teensy_loader_cli.
BUILD   := build
TOOLCHAIN := cmake/toolchain-arm-none-eabi.cmake
HEX     := $(BUILD)/cronus.hex
# macOS: /dev/cu.usbmodem*  |  Linux: /dev/ttyACM0
SERIAL  := /dev/cu.usbmodem*

.PHONY: build flash monitor clean

build:
	cmake -S . -B $(BUILD) -G Ninja -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN)
	ninja -C $(BUILD)

flash: build
	teensy_loader_cli --mcu=TEENSY41 -w -v $(HEX)

# Serial monitor — only useful once UART lands (Ex4); nothing to see before then.
monitor:
	picocom -b 115200 $(SERIAL)

clean:
	rm -rf $(BUILD)
