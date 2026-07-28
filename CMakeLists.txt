cmake_minimum_required(VERSION 3.20)
project(cronus C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(cronus.elf
  arch/startup.c
  arch/vectors.c
  arch/dwt.c
  arch/systick.c
  arch/context.S
  arch/port.c
  arch/fault.c
  arch/mpu.c
  arch/fpu.c
  kernel/tasks.c
  kernel/critical.c
  kernel/sem.c
  kernel/mutex.c
  kernel/queue.c
  kernel/cstrings.c
  kernel/timer.c
  bsp/boot.c
  bsp/clock.c
  bsp/gpt.c
  bsp/lpm.c
  app/main.c
  drivers/uart.c
)

target_include_directories(cronus.elf PRIVATE
  include
  arch
  bsp
  kernel
  drivers
)

target_compile_options(cronus.elf PRIVATE
  -mcpu=cortex-m7
  -mthumb
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -Wall -Wextra
  -Og -g
  -ffunction-sections -fdata-sections
  $<$<COMPILE_LANGUAGE:C>:-std=c11>
)

target_link_options(cronus.elf PRIVATE
  -mcpu=cortex-m7
  -mthumb
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -T${CMAKE_SOURCE_DIR}/linker/teensy41.ld
  -Wl,--gc-sections
  -nostartfiles
  -nostdlib
)

add_custom_command(TARGET cronus.elf POST_BUILD
  COMMAND arm-none-eabi-objcopy -O ihex cronus.elf cronus.hex
  COMMAND arm-none-eabi-size cronus.elf
)
