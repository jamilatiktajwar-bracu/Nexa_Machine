# Nexa OS 2.9 — Priority Scheduling, Timer Preemption and IPC

## Priority-aware scheduler
Processes now have priorities in the range 0–7; 7 is highest. The scheduler chooses the highest-priority Ready process and preserves round-robin ordering among processes at that priority. Priority aging can be enabled with `setPriorityAgingThreshold(ticks)`: each threshold of Ready-state waiting raises a process effective priority by one, up to 7. A threshold of zero disables aging. This reduces starvation while preserving fixed-priority behavior when disabled. Use `setPriority(pid, priority)` to update base priority. The root shell user can run `nice PID PRIORITY` and `aging TICKS`; `ps` displays base/effective priority, wait ticks, instruction ticks and inbox depth.

## Time slicing and timer interrupts
`setTimeQuantum(instructions)` controls the maximum number of guest instructions executed in one scheduler slice (1–1,000,000). `setTimerInterval(ticks)` configures a simulated timer interrupt every N global guest instruction ticks; zero disables timer IRQs. A timer event preempts the active slice. The kernel exposes `systemTicks()`, `timerInterrupts()`, and `pendingTimerInterrupts()`; call `acknowledgeTimerInterrupt()` to consume one pending event. The root shell user can configure these values with `quantum INSTRUCTIONS` and `timer TICKS` (`timer 0` disables IRQs). The `sched` shell command prints the scheduling policy, quantum, tick count, timer counters, aging threshold and deadlock status. `ps` displays process state, base/effective priority, wait ticks and inbox depth. `SYS Yield` ends the current process slice early.

## Process-to-process message passing
The kernel provides bounded FIFO inboxes, blocking guest receives and host APIs:
- `sendMessage(fromPid, toPid, payload)`
- `receiveMessage(pid)`

Each inbox holds at most 64 messages; payloads are limited to 251 bytes. Sending from or to a finished process, or sending to a full inbox, fails without silently dropping an existing message. A blocked guest receiver becomes Ready when a message arrives.

Guest ABI:
- `SYS SendMessage` (service 11), B points to `[targetPidLow, targetPidHigh, length, payload...]`; target PID is a 16-bit little-endian value in this guest ABI. It returns 1 on delivery and 0 when the target is absent, stopped or its inbox is full
- `SYS ReceiveMessage` (service 12), B points to an output buffer. On success it writes `[senderPid byte0..byte3, length, payload...]` and returns 1. If the inbox is empty, the calling process blocks and retries the receive syscall after a sender wakes it.
- Payload is at most 251 bytes. All descriptors and payloads must fit in the guest process RAM. A too-small receive buffer leaves the message queued. An empty inbox blocks the guest process and the syscall is retried when a sender wakes it.

## Blocking and deadlock detection
An empty guest receive blocks the process instead of busy-waiting. A successful send wakes a blocked receiver, which retries its receive syscall. `hasDeadlock()` reports when at least one process is blocked and no process is Ready or Running; the `ps` command shows blocked state and inbox depth. This is a scheduler-level detector, not a full wait-for-graph analysis.

## VM stack hardening
The VM call/data stack is capped at 4,096 entries. PUSH, CALL and interrupt entry check the bound before changing the stack and raise a controlled `Stack overflow` fault. State files containing oversized stacks are rejected during load.

## Process resource budgets
Each process may have an optional instruction budget configured with `setInstructionBudget(pid, instructions)`. A value of zero disables the budget. When the process reaches its exact instruction limit, it is stopped with exit code 124 and termination reason `budget`; resources are closed automatically. Root can set a limit from the shell using `limit PID INSTRUCTIONS`. This is a deterministic CPU-instruction quota, not a wall-clock timeout.

## Process management
The root shell user can use `kill PID` to halt a running or blocked process and close its file handles/mailbox; the simulator records kill exit code 137. `reap PID` then destroys the process data/code address spaces, releases resident frames, discards swapped pages and removes process metadata. Guest `SYS Exit` stores B as the exit code; a plain `HALT` is recorded with reason `halt`, and faults record reason `fault`. `ps` shows exit code, termination reason and fault detail when available. Processes that exit or fault also close their process-local handles and inboxes automatically.

## Shell memory diagnostics
`memstat` reports resident/free frames, swapped pages, swap-in/out counts, page faults, TLB hit/miss counts and the active swap backend. Root can configure disk-backed swap using `swapfile HOST_PATH`; configure it before the next swap-out activity.

## Regression coverage
Tests cover priority choice, quantum accounting, timer IRQ delivery/acknowledgement, host FIFO IPC, payload limits and guest-to-guest message passing through the syscall ABI.

## Simulator boundaries
This scheduler is deterministic cooperative-at-instruction-boundary preemption, not a real hardware timer or SMP scheduler. It schedules whole VM processes, not independent POSIX-style threads. IPC is local in-memory messaging, not shared memory or a network protocol. Priority inheritance and deadline scheduling are not implemented.
