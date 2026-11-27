# Changelog

All notable Nexa Machine milestones are documented here. This is an educational VM and OS simulator, not a production hypervisor or standalone OS.

## [2.11.0] — Kernel synchronization
- Added bounded counting semaphores and FIFO mutexes with owner validation and waiter handoff.
- Added guest syscalls 14–17: SemaphoreWait, SemaphoreSignal, MutexLock and MutexUnlock; wait syscalls block and retry atomically after wakeup.
- Added host APIs for synchronization-object creation and use; validation for invalid bounds, unknown IDs, overflow, wrong-owner unlock and recursive lock.
- Added regression coverage for guest blocking/wakeup, semaphore saturation and mutex ownership/handoff.
- Synchronization objects are kernel-global, host-created handles, limited to 255 per type; no shared-memory mapping or priority inheritance yet.

## [2.10.0] — Process signals and lifecycle control
- Added TERM/KILL termination signals, STOP/CONT process suspension and resumption, and bounded FIFO USR1/USR2 queues.
- Added guest `SYS GetSignal` service (13), returning the next pending user signal or zero.
- Added root-only `signal PID SIGNAL`, stopped-state visibility and pending-signal counts in `ps`.
- Added regression coverage for queue bounds, FIFO delivery, guest signal polling, process transitions and shell controls.
- Signal delivery is intentionally a compact simulator ABI: no user-installed handlers, masks, real-time signals or Unix permission model.

## [2.9.1] — Scheduler quotas and VM safety polish
- Per-process instruction budget controls, exact quota termination with exit code 124.
- 4,096-entry VM stack cap and controlled stack overflow faults.
- Saved states with oversized stacks rejected.

## [2.9.0] — Scheduler, IPC and process lifecycle

### Added
- Bounded VM stack (4,096 entries), controlled overflow faults, and rejection of oversized saved-state stacks.
- Per-process instruction budgets, deterministic quota termination (exit code 124), and a root `limit PID INSTRUCTIONS` shell command.
- Fixed-priority scheduler (0–7), round-robin selection within equal effective priorities, and configurable priority aging.
- Configurable instruction quantum, simulated timer IRQ interval, global tick accounting and interrupt acknowledgement.
- Guest SYS Yield, SendMessage and ReceiveMessage services.
- Bounded FIFO IPC inboxes, blocking guest receive, sender-triggered wakeup and scheduler-level deadlock detection.
- Host process termination and reaping; process file handles and mailboxes are cleared on exit/fault.
- Exit-code and termination-reason reporting, including fault details in ps.
- Shell administration and diagnostics: nice, aging, quantum, timer, sched, memstat, swapfile, kill and reap.

### Improved
- VM can retry the exact blocked syscall instruction while preserving syscall service/argument registers.
- Scheduler aging counters saturate safely rather than overflowing.
- Guest IPC to absent, stopped or full destinations returns a failure code instead of faulting the sender.
- Explicit exit closes process-local resources immediately.

### Validation
- Regression coverage includes priority selection/aging, timer preemption, voluntary yield, guest and host IPC, blocked/woken states, deadlock reporting, mailbox bounds, exit codes and process reaping.
- GitHub Actions build, test and demo checks run for each commit.

## [2.8.0] — Disk swap, executable format and privilege modes
- Optional host-file-backed swap (NEXASWP1) with in-memory swap fallback.
- ELF-inspired NEXAELF2 executable sections; legacy NEXAEXE1 remains readable.
- Scoped simulated user/kernel privilege transitions around host-mediated syscalls.

## [2.7.0] — LRU memory and memory-backed instruction fetch
- Global LRU eviction and demand page-in.
- Encoded guest instruction fetch from executable code pages.

## [2.6.0] — TLB and demand paging
- Simulated TLB accounting and process memory reclamation.
