# Phase 2 · Exercise 1: Counting Semaphore

**Phase 2 · Exercise 1 of 12** · **Difficulty:** ★★★★☆ (the timeout half nudges ★★★★★) · **Est. time:** 1–2 sessions

## Overview

A counting semaphore is a count plus a wait-list.

- `take` decrements the count and proceeds if a token is available; if the count is zero, it blocks the caller — parks it on the semaphore's own wait-list and yields.
- `give` either hands a token directly to the highest-priority waiter (waking it) or, if nobody's waiting, increments the count.

The mechanism is small. What makes it a real exercise is two pieces of machinery you don't have yet:

1. **A per-object wait-list** (until now every list has been a global kernel list; now each sync object owns one).
2. **Block-with-timeout**, which forces a task to be a member of two lists at once — the semaphore's wait-list and the delayed list — and is exactly the `xStateListItem`/`xEventListItem` split that FreeRTOS carries two link-sets per task to solve.

Your singly-linked delayed list and single `next`/`prev` pair have been enough for eleven exercises. This is where they aren't.

## Concept

### Waiting on an event, not a deadline

`task_delay` puts a task on the delayed list keyed by a wake tick — `kernel_tick` wakes it when the clock arrives, deterministically. A semaphore has no wake tick. The blocked task sits on the semaphore's wait-list until someone else calls `give`. The wake is driven by another thread of control, not by time — and that's the whole conceptual content of the exercise. The blocked task consumes zero CPU while it waits (it's off the ready list entirely), which you'll confirm on the analyzer: between signals, idle runs.

### The per-object wait-list, priority-ordered

Each semaphore owns a list of the tasks blocked on it, distinct from the ready and delayed lists. When several tasks wait and a token arrives, you wake the highest-priority waiter — not FIFO — because that's what respecting priority means. Keep the wait-list ordered by priority on insert so the head is always the highest-priority waiter: O(1) to wake, O(n) to insert, which is the right trade because waking (often from an ISR) must be fast.

### Direct handoff

When `give` finds a waiter, the clean model is to hand the token straight to that task without ever touching the count — remove it from the wait-list, mark it ready, done. The count only moves when there's nobody to hand to. This avoids a subtle race (the "increment count, wake task, task races another taker for the token it was just promised" bug) and matches how FreeRTOS does it. The woken task is guaranteed its token; it doesn't re-contend.

### Block-with-timeout — the crux, and why `next`/`prev` isn't enough

`sem_take(s, timeout)` waits on two things at once: the semaphore event, and a timeout. So the task must live on both lists simultaneously — the semaphore's wait-list (so `give` can find it) and the delayed list (so `kernel_tick` can time it out).

Your delayed list threads through `next`; your ready list uses `next`/`prev`. Both are already claimed. A task that's blocked-with-timeout needs a second, independent link-set to be on the wait-list at the same time it's on the delayed list via `next`. That's a new pair of pointers in the TCB (`ev_next`/`ev_prev`), and it's precisely FreeRTOS's reason for `xEventListItem` existing separately from `xStateListItem`.

The coordination is the hard part:

- When `give` fires, the task must come off the delayed list too (its timeout is moot).
- When the timeout fires, the task must come off the wait-list too (it gave up).

Whichever wakes it has to unlink it from the other list and tell it which happened.

### `give` from an ISR — your Ex5 handshake, reused

The canonical use is an interrupt signalling a task: hardware fires, the ISR does minimal work and gives a semaphore, a high-priority task blocked on it wakes and does the heavy lifting. That's deferred interrupt handling — the "bottom half" — and it's how every driver in Phase 2 will be built.

`sem_give_from_isr` must be ISR-safe and return a "higher-priority task woke — yield on exit" flag so the ISR epilogue can pend PendSV. You already built this exact return-the-yield-flag pattern in `resume_task_isr`; this is it again.

Note the asymmetry: there is no `sem_take_from_isr` that blocks — an ISR can't yield, so an ISR can only ever poll (take-if-available), never wait.

### Counting vs binary, and a warning that sets up P2-3

The count can exceed one, which lets a semaphore tally events that arrive faster than they're consumed (an ISR gives three times before the task runs; the task then takes three times and catches up) or guard N identical resources. Binary is the count-capped-at-one special case — that's P2-2.

And a caution worth carrying forward: a counting semaphore used as a lock does not solve priority inversion — a low-priority holder can be preempted by a medium task while a high task waits, stalling the high task indefinitely. The semaphore has no notion of ownership, so it can't fix this. That's the entire reason the mutex (P2-3) is a separate, more complex primitive. Hold that thought; the last stretch makes you see the inversion.

## Goal

### Part 1 — infinite-wait core

1. A task calls `sem_take` on an empty semaphore and blocks — off the ready list, consuming no CPU (idle runs in the gap, visible on the analyzer).
2. Another task calls `sem_give` and the blocked task wakes and proceeds, having received a token by direct handoff.
3. With multiple waiters, `give` wakes the highest-priority one first (and FIFO among equals).
4. `sem_give_from_isr` wakes a blocked task and signals the yield flag, so the task runs on ISR exit, not at the next tick.
5. Counting behavior: several gives while nobody waits raise the count, and a task then takes them one at a time without blocking.

### Part 2 — timeout (the hard half)

6. `sem_take(s, 100)` on an empty semaphore returns failure after ~100 ms if no give arrives — and the task is cleanly off both lists afterward.
7. If a give arrives within the window, the same call returns success early, and the timeout is cancelled (the task comes off the delayed list too).
8. `sem_take(s, 0)` is a non-blocking poll — takes a token if one's available, returns failure immediately if not, never blocks.

> When a consumer blocks on an empty semaphore, a producer (or an ISR) signals it, and it wakes at the signal — and when a timed wait either succeeds early or expires cleanly — you have the primitive the whole rest of Phase 2 is built on.

### On the analyzer

- `SCOPE_PIN_16` toggles at the producer's 300 ms cadence, and in the 300 ms gaps the consumer is genuinely blocked — idle runs, no CPU burned.
- Flip to two consumers at different priorities and confirm the higher one always wins the token.
- Then have an ISR give and watch the consumer run on ISR exit.

## Implementation Hints

- **The blocked task is already off the ready list — don't remove it again.** `curr_task` is running, which means it was pulled from the ready list when it became `RUNNING`. `sem_take` just adds it to the wait-list (and maybe the delayed list) and yields; there's no ready-list removal to do. This trips people who reflexively call an unlink.

- **`remove_delayed_task` is safe to call on a task that isn't on the delayed list.** Your implementation walks and only unlinks if found, so `give` can call it unconditionally — an infinite-wait waiter simply isn't there and it's a no-op walk. If the O(n) walk bothers you, gate it on `timeout != FOREVER` recorded per task; for now, unconditional is fine and simpler.

- **Two link-sets, two lists, zero overlap.** The delayed list uses `next`; the wait-list uses `ev_next`/`ev_prev`. A blocked-with-timeout task is on both at once through different pointers. If you ever find yourself putting a waiter on the wait-list through `next`, stop — that's the bug that corrupts the delayed list the instant a timeout and a give race.

- **Direct handoff means the count stays untouched when there's a waiter.** Waking a task and incrementing the count double-counts the token — the woken task takes one, and the phantom increment lets a future taker succeed when it shouldn't. Hand off xor increment, never both.

- **`wait_result` is the only channel telling the woken task what happened.** Set it before you ready the task, from whichever path wakes it: `give`/`give_from_isr` set `1`, the `kernel_tick` timeout path sets `0`. The task reads it after `yield()` returns. Initialize it to `0` at block time so a lost race defaults to "timed out," never a false success.

- **Preempt on wake only for strictly-higher priority — the same `>` not `>=` rule from Ex5.** Waking an equal-priority waiter shouldn't jump the running task; round-robin handles that at the next tick.

- **`enter_critical` around the whole take/give decision.** The count check, the wait-list edit, and the delayed-list arming must be atomic against both an ISR give and `kernel_tick`. The window between "saw `count == 0`" and "added self to wait-list" is exactly where a concurrent give would otherwise be lost.

- **No `sem_take_from_isr` that blocks — ever.** An ISR can't yield, so it can't wait. If you want an ISR-side take, it's a non-blocking poll only. Make blocking-take from interrupt context impossible (it has no `curr_task` to block in any meaningful sense).

## Stretch Challenges — FreeRTOS-grade

### 1. Prove priority-ordered wake on the wire

Two consumers, priorities 8 and 6, both blocked on the same empty semaphore. `give` once and capture both scope pins: the priority-8 task must toggle, the priority-6 task must stay dark until the next `give`.

Then add a third at priority 8 and confirm FIFO within the priority level (the one that blocked first wakes first — that's what the `>=` in `wait_insert` buys you). This is the test that separates "a wait-list" from "a correct wait-list."

### 2. Show the counting behavior explicitly

Have an ISR give faster than the consumer drains — e.g., three gives in a burst while the consumer is busy. Watch count climb to 3, then the consumer take three times back-to-back with no blocking, catching up. Print the count each step. This is the concrete difference between counting and binary, and it's the behavior a driver relies on when interrupts outrun processing.

### 3. Deferred interrupt handling + latency number

Build the real bottom-half pattern: a GPIO or timer ISR does nothing but `sem_give_from_isr`, and a top-priority task blocked on the semaphore does the "work." Timestamp with DWT from the ISR entry to the first instruction of the woken task — that's your interrupt response latency, a headline RTOS benchmark and a number for your launch writeup. Compare it against processing directly in the ISR and articulate the trade (bounded ISR time + prioritizable work, vs one context switch of latency).

### 4. `sem_take` timeout under tickless idle

Your Ex3 tickless path sleeps until the next wake. A `sem_take` timeout is a wake — confirm `get_next_wake` accounts for a blocked-with-timeout task's wake, so the system doesn't oversleep past a timeout that should have fired. If it doesn't, that's the same class of bug as the `get_next_wake` overflow you already fixed — the delayed list has an entry the tickless calculation must see.

### 5. Watch priority inversion happen — and understand why this primitive can't fix it

Construct it deliberately:

- A low-priority task takes the semaphore-as-lock.
- A medium-priority task spins (never blocking).
- A high-priority task blocks on the semaphore.

Observe on the scope that the high task waits on the low task, which the medium task is starving — unbounded inversion, the high task stalled by a task two levels below it. The semaphore has no ownership concept, so it structurally cannot help. This is the exact failure the mutex's priority inheritance solves in P2-3, and seeing it here is the motivation for why the next exercise is harder than "a semaphore that only one task holds."

## What you'll come away with

- The first event-driven blocking primitive — the conceptual jump from a scheduler that waits on time to an OS that waits on other threads of control
- Per-object wait-lists and the priority-ordered wake discipline that every sync primitive after this reuses
- The two-list-membership solution — a second link-set so a task can wait on an event and a timeout at once — which is the single most important structural upgrade in Phase 2 and the reason FreeRTOS carries `xStateListItem` and `xEventListItem` separately
- The direct-handoff protocol and why it's race-free where "increment-then-wake" isn't
- A reusable ISR-to-task signalling primitive — the foundation every Phase 2 driver is built on, plus an interrupt-latency number for your benchmarks
- Concrete sight of priority inversion, which turns P2-3's priority-inheritance mutex from an abstract feature into an obvious necessity

## The moment to chase

Put the consumer to sleep on an empty semaphore and watch `SCOPE_PIN_16` go flat — genuinely flat, idle running underneath, the task consuming nothing — then fire a single `give` and watch the pin toggle at the instant of the give, not on the next tick edge. That gap-then-wake is the whole primitive: a task suspended on nothing but the promise of a future event, resumed the moment the event arrives.

Then flip on the timeout and watch a wait expire cleanly — the task coming off both lists, returning failure, no corruption. That's the two-list machinery holding.

## When you're back

- The zip
- The analyzer capture of block-then-wake (the flat gap is the proof, not the toggle)
- A timeout expiring, and a timeout beaten by an early give
- If you take the inversion stretch, send the scope trace — I want to see the high task stall

**The questions I care about:**

1. Which link-set a blocked-with-timeout task uses for each list
2. What your `kernel_tick` does differently when it wakes a `TASK_BLOCKED` task versus a `TASK_SLEEP` one

Tag it `p2-01-semaphore`.

## Next up: P2-2 — binary semaphore + task notifications

The binary sem falls out of this as a count capped at one, and task notifications are the lightweight, object-free signal that's the fastest primitive in the box. After the machinery you just built, it's a short one.
