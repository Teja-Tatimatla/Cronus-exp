# Cross-compile toolchain for ARM Cortex-M7 (Teensy 4.1 / i.MX RT1062).
# Pass to cmake with: -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# No runnable libc/startup during the compiler check, so build a static lib
# for the try-compile instead of a full executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Defines the chip. Visible to CMakeLists.txt (toolchain file is read first).
# set(MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")
