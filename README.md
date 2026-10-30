# Cronus

A from-scratch preemptive RTOS for the ARM Cortex-M7 (Teensy 4.1 / i.MX RT1062).
No SDK, no CMSIS, no Arduino.

> This is my from-scratch RTOS and learning journey. I've learned from
> *Real-Time Operating Systems for ARM Cortex-M Microcontrollers* by Jonathan
> Valvano, *The Definitive Guide to the ARM Cortex-M3* (2nd edition) by Joseph
> Yiu, and many online resources. I asked Claude to structure a course for me,
> with exercises that build towards a fully functioning RTOS. I try to implement
> each feature myself, then ask it to check my work and point out any
> shortcomings. Once I'm clear on the concepts, I plan on rebuilding the entire
> repo from scratch, again, using only reference manuals.

For example, this is [Phase 2 Exercise 1](docs/exercises/p2-01-semaphore.md).

## Layout

```
arch/            ARM Cortex-M7 core (any M7): startup, vectors, DWT, context switch
bsp/             i.MX RT1062 + Teensy 4.1: boot (FlexSPI/IVT), clock, pin map
kernel/          RTOS core: scheduler, IPC, timers   (Phase 1+)
drivers/         driver framework + UART/SPI/GPIO     (Phase 2)
app/             entry point / current exercise
include/cronus/  public API + config knobs
```

## Build / Flash / Run

```cmd
make build     # configure + compile -> build/cronus.hex
make flash     # build, then flash over USB (press the Teensy button if prompted)
make clean
```

## Zed

```cmd
make build
ln -sf build/compile_commands.json compile_commands.json
```

## Notes / known limits

- **SET/CLR/TOG aliases.** i.MX RT CCM/analog registers have sibling SET/CLR/TOG regs
  at +0x4/+0x8/+0xC for atomic bit ops. The RMW in `switch_clock_600mhz()` is safe only
  because interrupts aren't running yet; once the scheduler is live, RMW on a shared
  register is a race. SET/CLR is the lock-free fix.
- **Core voltage.** 600 MHz assumes VDD_SOC is adequate. A clean bring-up would raise
  DCDC_REG3 first — a hidden dependency worth noting.
- **Delay overflow.** `delay_ms()` overflows uint32_t past ~7,158 ms (the ~7 s CYCCNT
  ceiling). Software timers (Phase 2) handle long intervals differently.
