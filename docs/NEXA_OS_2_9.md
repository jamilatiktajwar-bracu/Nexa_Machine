# Nexa OS 2.9 — Priority Scheduling, Timer Preemption and IPC

## Priority-aware scheduler
Processes now have priorities in the range 0–7; 7 is highest. The scheduler chooses the highest-priority Ready process and preserves round-robin ordering among processes at that priority. A continuously-ready higher-priority process can starve lower-priority work; this is a deliberate simple fixed-priority policy for the simulator. Use `setPriority(pid, priority)` to update a process. The root shell user can also run `nice PID PRIORITY`; `ps` now displays priority, instruction ticks and inbox depth.

## Time slicing and timer interrupts
`setTimeQuantum(instructions)` controls the maximum number of guest instructions executed in one scheduler slice (1–1,000,000). `setTimerInterval(ticks)` configures a simulated timer interrupt every N global guest instruction ticks; zero disables timer IRQs. A timer event preempts the active slice. The kernel exposes `systemTicks()`, `timerInterrupts()`, and `pendingTimerInterrupts()`; call `acknowledgeTimerInterrupt()` to consume one pending event. The `sched` shell command prints the scheduling policy, quantum, tick count and timer counters. `SYS Yield` ends the current process slice early.

## Process-to-process message passing
The kernel provides bounded FIFO inboxes, blocking guest receives and host APIs:
- `sendMessage(fromPid, toPid, payload)`
- `receiveMessage(pid)`

Each inbox holds at most 64 messages; payloads are limited to 251 bytes. Sending from or to a finished process, or sending to a full inbox, fails without silently dropping an existing message. A blocked guest receiver becomes Ready when a message arrives.

Guest ABI:
- `SYS SendMessage` (service 11), B points to `[targetPidLow, targetPidHigh, length, payload...]`
- `SYS ReceiveMessage` (service 12), B points to an output buffer. On success it writes `[senderPid byte0..byte3, length, payload...]` and returns 1. If the inbox is empty, the calling process blocks and retries the receive syscall after a sender wakes it.
- Payload is at most 251 bytes. All descriptors and payloads must fit in the guest process RAM. A too-small receive buffer leaves the message queued. An empty inbox blocks the guest process and the syscall is retried when a sender wakes it.

## Blocking and deadlock detection
An empty guest receive blocks the process instead of busy-waiting. A successful send wakes a blocked receiver, which retries its receive syscall. `hasDeadlock()` reports when at least one process is blocked and no process is Ready or Running; the `ps` command shows blocked state and inbox depth. This is a scheduler-level detector, not a full wait-for-graph analysis.

## Regression coverage
Tests cover priority choice, quantum accounting, timer IRQ delivery/acknowledgement, host FIFO IPC, payload limits and guest-to-guest message passing through the syscall ABI.

## Simulator boundaries
This scheduler is deterministic cooperative-at-instruction-boundary preemption, not a real hardware timer or SMP scheduler. It schedules whole VM processes, not independent POSIX-style threads. IPC is local in-memory messaging, not shared memory or a network protocol. Fixed priorities do not include aging, priority inheritance or deadline scheduling.
