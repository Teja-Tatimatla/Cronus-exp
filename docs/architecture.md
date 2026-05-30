# Architecture

Cronus is layered the way the Greek cosmogony is: order emerging from the void,
primordials, Titans, then the gods built on top. That maps almost exactly onto an
OS layer stack, so the cosmology *is* the architecture diagram.

| Layer                | Greek figure | Subsystem (descriptive name)          |
|----------------------|--------------|---------------------------------------|
| Power-on / boot      | Chaos        | `bsp/boot.c` (FlexSPI FCB / IVT)       |
| Foundation / memory  | Gaia         | `linker/teensy41.ld`                   |
| **Kernel core (Titans)** |          |                                       |
| The kernel / time    | Cronus       | the project itself                     |
| Scheduler            | Themis       | `kernel/sched.c`        (Phase 1)      |
| System tick          | Chronos      | `arch/systick.c`        (Ex3)          |
| Memory / MPU         | Mnemosyne    | `kernel/*` + `arch/mpu.c`              |
| Context switch       | Charon       | `arch/context.S`        (Ex6-7)        |
| **Services (Olympians)** |          |                                       |
| IPC / queues         | Hermes       | `kernel/queue.c`        (Phase 2)      |
| Locks                | Cerberus     | `kernel/sem.c`, `mutex.c` (Phase 2)    |
| Software timers      | Moirai       | `kernel/timer.c`        (Phase 2)      |
| Idle / WFI           | Hypnos       | `kernel/idle.c`         (Phase 1)      |
| Driver framework     | Hephaestus   | `drivers/driver.h`      (Phase 2)      |
| Shell / CLI          | Pythia       | `app/shell.c`           (Phase 2)      |
| **Failure (underworld)** |          |                                       |
| HardFault / panic    | Tartarus     | fault handlers in `arch/vectors.c`     |
| Watchdog             | Argus        | (later)                                |

## arch vs bsp

`arch/` is everything true of any Cortex-M7 (core registers, exceptions, SysTick,
context switch). `bsp/` is everything specific to this chip and board (CCM clock
tree, IOMUXC, GPIO, the FlexSPI boot config). The kernel talks to the core only
through `arch/port.h`, so it never touches a raw register.
