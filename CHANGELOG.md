# Changelog

## [2.32.0] - 2026-11-27
- Added allocation recovery attempt/success/failure telemetry and regression assertions.


## [2.31.0] - 2026-11-27
- Added guarded memory-allocation recovery callbacks; allocator retries after callback frees a frame when no free or evictable frames exist.
- Connected kernel recovery to safe reaping of Halted/Faulted processes only; active processes are never force-killed.
- Added pinned shared-frame exhaustion test for recovery retry and complete frame reclamation.


## [2.29.0] - 2026-11-27
- Added `oomstat` system memory pressure diagnostics: free-frame ratio classification, resident/swapped pages, and swap counters.
- Defined pressure labels as normal (>10% free), high (<=10% free), and critical (zero free frames); these are advisory diagnostics, not automatic OOM recovery.
- Added tests for snapshot output and pressure classification.


## [2.28.0] - 2026-11-27
- Added configurable per-process memory page quotas covering executable and data mappings, including shared-memory mappings.
- Added the root-only `memlimit PID PAGES` command (`0` disables the limit) and exposed configured limits in `ps`.
- Processes that exceed their quota fault cleanly instead of allocating beyond the configured page budget; lowering a limit below current usage is rejected.
- Added regression coverage for quota enforcement, process fault reporting, and disabling limits.


## [2.27.0] - 2026-11-27
- Added per-address-space resident and swapped page counters and exposed combined data/code virtual pages, resident pages, swapped pages, and resident bytes in `ProcessInfo`.
- Extended `ps` with per-process memory usage and page residency diagnostics.
- Added regression checks for process memory accounting before and after demand paging.


## [2.26.0] - 2026-11-27
- Added live per-process open-file handle and shared-memory attachment counts to `ProcessInfo`.
- Extended `ps` diagnostics with inbox payload bytes, open file handles, and shared-memory attachments.
- Added regression coverage for resource counts during file open/exit and shared-memory attach/detach.


## [2.25.0] - 2026-11-27
- Added an 8,192-byte per-process IPC inbox payload quota alongside existing message-count and per-message limits.
- Sending beyond the quota is rejected; receiving releases capacity immediately. Added regression tests for quota enforcement and recovery.


## [2.24.0] - 2026-11-27
- Added `ProcessInfo::pendingMessageBytes` to expose per-process queued IPC payload bytes alongside message counts.
- Added regression coverage for byte accounting during FIFO enqueue and dequeue.


## [2.23.0] - 2026-11-27
- Added `mutexDeadlockCycle()` to return the process IDs participating in a detected circular mutex wait, alongside the boolean `hasMutexDeadlock()` query.
- Added tests verifying cycle membership and that termination cleanup clears the reported cycle.


## [2.22.0] - 2026-11-27
- Added wait-for graph cycle detection through `hasMutexDeadlock()`, identifying circular mutex dependencies separately from scheduler-wide blocked-state detection.
- Added regression coverage for an acyclic waiter, a two-process mutex cycle, and cycle recovery after owner termination.


## [2.21.1] - 2026-11-27
- Fixed synchronization cleanup for TERM/KILL signal exits so dead waiters are removed and abandoned semaphore grants are recovered consistently.
- Added regression tests for signal-driven semaphore and mutex waiter termination.


## [2.21.0] - 2026-11-27
- Added process-exit cleanup for semaphore and mutex wait queues, preventing terminated waiters from lingering in synchronization diagnostics.
- Semaphore permits already handed to a process that exits before consuming them are transferred to the next live waiter or returned to the bounded semaphore count.
- Added regression coverage for terminated semaphore and mutex waiters and semaphore grant handoff.


## [2.20.0] - 2026-11-27
- Added late-page mapping-collision rollback tests proving partial aliases are removed without disturbing pre-existing guest mappings.
- Added live-segment destruction tests proving guest aliases are revoked immediately and backing shared frames are reclaimed.


## [2.16.0] - 2026-11-27
- Added scheduler-atomic shared byte add and compare-swap operations (guest syscalls 23–24).
- Expanded regression tests for atomic syscall dispatch, byte overflow and compare-swap success/failure.
- Shared-memory API now includes previous-value-returning atomic operations; operations are atomic relative to the simulated single-core scheduler, not SMP hardware.


## [2.15.0] - 2026-11-27
- Added reference-counted shared physical frames and guest virtual-page aliases.
- Shared segments map into guest address spaces, with user read/write and non-executable permissions; detach/exit unmaps.
- Added guest SharedCreate syscall (22) and high virtual address loads/stores via VMM-backed VM accessors.
- Shared pages are currently non-pageable; physical-frame exhaustion is reported.


All notable Nexa Machine milestones are documented here. This is an educational VM and OS simulator, not a production hypervisor or standalone OS.

## [2.14.0] - 2026-11-27
- Added guest shared-memory syscalls 18–21 (attach/detach/read/write) with checked descriptors and bounds/attachment validation.
- Added `shmstat` shell diagnostics and isolated mutex owner-death regression test.

## [2.13.0] - 2026-11-27
- Added kernel-managed shared memory segments with attach/detach, byte read/write APIs, lifecycle cleanup and diagnostics.
- Shared memory remains host-accessible through kernel API; it is not yet mapped into guest virtual address spaces.

## [2.12.0] - 2026-11-27
- Added transitive priority inheritance for mutex owners, preventing priority inversion across nested lock chains.
- Added `syncstat` shell diagnostics for semaphore/mutex state, owners, wait queues, and effective priorities.
- Added regression coverage for inheritance boost and restoration.

## [2.11.0] — Kernel synchronization
- Added bounded counting semaphores and FIFO mutexes with owner validation and waiter handoff.
- Added guest syscalls 14–17: SemaphoreWait, SemaphoreSignal, MutexLock and MutexUnlock; wait syscalls block and retry atomically after wakeup.
- Added host APIs for synchronization-object creation and use; validation for invalid bounds, unknown IDs, overflow, wrong-owner unlock and recursive lock.
- Added regression coverage for guest blocking/wakeup, semaphore saturation, mutex ownership/handoff and owner-death recovery.
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
