

## Nexa OS 2.28 — Per-process memory limits

- Root can set a process page budget with `memlimit PID PAGES`; `0` disables the limit.
- The quota covers executable and data pages, including shared-memory mappings, and is enforced when new pages are mapped.
- A process exceeding its quota receives an isolated memory-limit fault; setting a limit below current usage is rejected.
- `ps` reports the configured `memlimit` alongside resident and virtual memory metrics.

## Nexa OS 2.27 — Per-process memory accounting

- `ProcessInfo` now reports each process's combined data/code virtual pages, resident pages, swapped pages, and resident bytes.
- `ps` displays resident versus virtual memory, page counts, and swapped-page counts alongside existing process resources.
- Memory values are calculated from the live page tables; shared mappings are counted in each address space that maps them, while physical `memstat` remains system-wide.
- Regression tests verify accounting before and after a process triggers demand paging.

## Nexa OS 2.26 — Per-process resource accounting

- `ProcessInfo` reports live open-file handle and shared-memory attachment counts for each process.
- `ps` now shows inbox message count/payload bytes, open file handles, shared-memory attachments, and pending signals in one process row.
- Resource values are derived from current kernel containers, so close/detach/exit cleanup is reflected immediately.
- Regression tests cover file open and exit cleanup plus shared-memory attach/detach accounting.

## Nexa OS 2.25 — IPC mailbox memory quota

- Each process inbox is capped at 8,192 queued payload bytes in addition to the existing 64-message and 251-byte-per-message limits.
- `sendMessage()` rejects a message that would exceed the byte quota; dequeue immediately releases quota for subsequent sends.
- `pendingMessageBytes` exposes current payload usage. Tests cover quota rejection and capacity recovery after receive.

## Nexa OS 2.24 — IPC mailbox accounting

- `ProcessInfo::pendingMessages` reports queued inbox message count and the new `pendingMessageBytes` reports the total queued payload bytes.
- Byte accounting is derived from the live FIFO mailbox, so it updates immediately after enqueue, dequeue, and process cleanup without a second mutable counter.
- Regression tests cover enqueue/dequeue accounting and byte totals across multiple FIFO messages.

## Nexa OS 2.23 — Deadlock cycle diagnostics

- `mutexDeadlockCycle()` returns the process IDs in the first detected circular mutex wait, allowing diagnostics to identify the affected processes.
- `hasMutexDeadlock()` remains the quick boolean query and is implemented using the cycle report.
- The cycle list is empty when no circular mutex dependency exists; termination and owner handoff are reflected immediately.
- See [Nexa OS 2.23 documentation](docs/NEXA_OS_2_23.md).

# Nexa Machine

## Nexa OS 2.29 — Memory pressure diagnostics

- Added `oomstat`, a read-only system memory pressure snapshot with used/free/total frames, swap activity and a simple pressure classification.
- Pressure is `critical` when no free frames remain, `high` at or below 10% free frames, and `normal` otherwise; swapping availability is reported separately, not treated as a guarantee of recovery.
- Added regression coverage for command output and pressure classification.


Nexa Machine is an educational C++20 virtual machine and OS-kernel simulator. It is not a hypervisor or a standalone bootable operating system.

**Current development milestone: Nexa OS 2.26.0**

Current kernel capabilities include:
- Isolated guest processes with demand-paged virtual memory, TLB simulation, LRU replacement and optional disk-backed swap.
- Read-only executable code pages with memory-backed instruction fetch and an ELF-inspired `NEXAELF2` binary loader (legacy `NEXAEXE1` remains supported).
- User/kernel syscall transitions, persistent VFS, file permissions and authenticated shell accounts.
- Fixed-priority scheduling, configurable instruction quanta, timer-interrupt simulation and priority aging.
- Bounded process mailboxes, blocking guest receive, automatic wakeup, and scheduler deadlock detection.
- Per-process instruction quotas with deterministic termination and exit-code reporting.
- Process signals: TERM/KILL termination, STOP/CONT lifecycle control, bounded FIFO USR1/USR2 delivery, and guest `SYS GetSignal` polling.
- Signal queue diagnostics in `ps` and root-only `signal PID SIGNAL` administration.
- Process-exit synchronization cleanup applies to normal exit, faults, budget termination and TERM/KILL signals, removing dead semaphore/mutex waiters and safely returns or hands off unconsumed semaphore grants.
- Kernel synchronization, priority inheritance, and syncstat diagnostics: bounded counting semaphores and FIFO mutex handoff, blocking guest waits and syscalls 14–17.
- Kernel-managed shared-memory segments plus guest syscalls 18–21 and 25–27 for read/write or read-only attach, detach, byte and 16-bit atomic read-modify-write through checked descriptors; `shmstat` diagnostics.
- Administrative process termination/reaping with exit-code and fault-reason reporting, automatic file-handle/mailbox cleanup, memory statistics and swap configuration.

Start with the [Nexa OS 2.21 guide](docs/NEXA_OS_2_21.md) or the [Nexa OS 2.20 guide](docs/NEXA_OS_2_20.md) or the [Nexa OS 2.17 guide](docs/NEXA_OS_2_17.md) or the [Nexa OS 2.16 guide](docs/NEXA_OS_2_15.md) or the [Nexa OS 2.12 guide](docs/NEXA_OS_2_12.md) or the [Nexa OS 2.11 guide](docs/NEXA_OS_2_11.md) or the [Nexa OS 2.10 guide](docs/NEXA_OS_2_10.md) or the [Nexa OS 2.9 guide](docs/NEXA_OS_2_9.md) or review the [project changelog](CHANGELOG.md). The build and test instructions are below. Earlier milestones are kept in this README as a development history.

## Version 1.0 (OS kernel foundation)
- 8-bit A/B registers, arithmetic, 16-bit RAM addressing
- Assembler constants (`CONST NAME, value` or `.CONST NAME, value`), labels, hex literals, #/;/\-\- comments
- CPU, branches, stack, calls, character I/O, `SYS` service instruction
- Simulated keyboard and text display, 64×32 monochrome framebuffer
- Graphics ports for coordinates, color, draw pixel, clear screen
- Single-step debugger, instruction breakpoints, RAM watchpoints and memory editing
- Optional Qt GUI includes a pixel framebuffer panel and RAM hex viewer
- VM snapshot persistence, virtual disk, execution budget
- Guest software interrupt vectors with `INT`/`IRET` instructions
- Portable executable binary format (`NEXAEXE1`) and CLI compile/load
- In-memory virtual filesystem (directories, file CRUD, path traversal rejection)
- Interactive GUI pixel drawing (left-click/drag white, right-click/drag erase)
- NexaKernel process lifecycle, per-process VM isolation and round-robin scheduler
- Kernel console output and host-mediated syscall API
- NexaOS host shell prototype

## Device ports
| Port | Direction | Meaning |
|---|---|---|
| 0 | IN | Keyboard byte (empty queue yields 0) |
| 1 | OUT | Character output |
| 2 | IN/OUT | Graphics X coordinate |
| 3 | IN/OUT | Graphics Y coordinate |
| 4 | OUT | Pixel color (0 black, nonzero white) |
| 5 | OUT | Draw pixel at current X,Y |
| 6 | OUT | Clear framebuffer (A=0 black; A!=0 white) |

Example:
```asm
CONST PX, 4
LOAD A, PX
OUT A, 2
LOAD A, 3
OUT A, 3
LOAD A, 1
OUT A, 4
OUT A, 5
HALT
```

## SYS services
Service number is in A. `SYS`:
- 0: halt
- 1: print character in B
- 2: read keyboard byte into A
- 3: clear graphics framebuffer

## Memory tools
Console: `mem ADDRESS`, `mem-write ADDRESS BYTE`, `watch ADDRESS`, `unwatch ADDRESS`. Execution pauses on a write to the watched address. APIs include writeMemory, framebuffer, pixel, and watchpoint status.

## NexaOS prototype
Run `nexa console`, then type `os`. Commands: help, echo, clear, mem, run FILE, exit. This is a host-side shell prototype, not yet a guest OS.

## Examples
- `examples/graphics_demo.nexa`
- `examples/syscall_demo.nexa`
- `examples/nexa_shell.nexa`
- `examples/keyboard_echo.nexa`
- `examples/boot_message.nexa`

## Build
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nexa examples/graphics_demo.nexa
./build/nexa examples/syscall_demo.nexa
./build/nexa console
```
Qt6 Widgets is optional; use `-DNEXA_BUILD_GUI=OFF` to disable it. Binary snapshots should only be loaded from trusted sources.


## Nexa OS 1.1

Guest programs can now invoke the kernel syscall bridge using the `SYS` instruction. Register A selects the service and register B supplies its 8-bit argument. See [Nexa OS 1.1 syscall ABI](docs/NEXA_OS_1_1.md). The bridge currently runs through a host callback and is a prototype, not a hardware privilege boundary.


## Nexa OS 1.2

Process RAM can be inspected through PID-checked kernel APIs, and faults are contained to the failing process. See [Nexa OS 1.2](docs/NEXA_OS_1_2.md).


## Nexa OS 1.3 — Persistent VFS

The virtual filesystem now supports portable disk-image persistence with validated loading (`NEXAVFS1`). Save and restore the directory tree and file contents using `VirtualFileSystem::saveImage()` and `loadImage()`. See [Nexa OS 1.3 documentation](docs/NEXA_OS_1_3.md). This remains a host-side image format, not a production journaled filesystem or a standalone boot disk.


## Nexa OS 1.4 — Process-scoped file permissions

Processes now receive no VFS permissions by default. The kernel can grant and check Read, Write, and Execute access independently per PID; the guest `FileExists` service requires Read permission. See [Nexa OS 1.4](docs/NEXA_OS_1_4.md). This is a software policy layer, not a hardware-enforced security boundary.


## Nexa OS 1.5 — Guest VFS file handles

Guest programs can open existing VFS files through permission-checked kernel syscalls, read/write bytes, and close process-local handles. Guest paths are read from process RAM. See [Nexa OS 1.5 documentation](docs/NEXA_OS_1_5.md). The ABI is a compact educational prototype.


## Nexa OS 1.6 — User-space shell and application launcher

Run `nexa --os` to open the kernel-backed VFS shell. It supports `ls`, `cat`, `touch`, `mkdir`, `write`, `rm`, `run`, and `ps`. The `run` command assembles and launches guest assembly stored in the VFS. See [Nexa OS 1.6 documentation](docs/NEXA_OS_1_6.md). The VFS is in-memory by default and the shell is an educational host-side interface, not a standalone bootable OS.


## Nexa OS 1.7 — Persistent shell workspace

The shell now supports `pwd`, `cd`, relative paths, and VFS persistence through `savefs HOST_PATH` / `loadfs HOST_PATH`. See [Nexa OS 1.7 documentation](docs/NEXA_OS_1_7.md).


## Nexa OS 1.8 — User accounts and file permissions

The shell now supports `useradd`, `login`, `whoami`, `chmod`, and `chown`, with owner/group/other mode-bit checks for file reads, writes, removal, and execution. See [Nexa OS 1.8](docs/NEXA_OS_1_8.md). This is an in-memory educational policy layer without password authentication or hardware-enforced isolation.


## Nexa OS 1.9 — Persistent account and permission metadata

VFS disk images now preserve shell users and file ownership/mode metadata. savefs stores versioned security metadata alongside the filesystem; loadfs restores it and resets the active session to root. Older images without metadata remain compatible. This is educational metadata persistence, not password authentication or cryptographic protection. See [Nexa OS 1.9](docs/NEXA_OS_1_9.md).


## Nexa OS 2.0 — Password authentication

Shell accounts now use salted PBKDF2-HMAC-SHA256 password digests, password-verified login, and persistent credential metadata in VFS images. Account commands: `useradd USER PASSWORD`, `login USER PASSWORD`, and `passwd PASSWORD`. OpenSSL Crypto is a build dependency. This remains an educational shell; command-line passwords are not hidden and disk images are not cryptographically authenticated. See [Nexa OS 2.0](docs/NEXA_OS_2_0.md).


## Nexa OS 2.1 — Account lockout and groups

The shell now locks an account after five consecutive failed login attempts. Root can recover an account with `passwd USER NEW_PASSWORD` or `unlock USER`. Root can create groups with `groupadd GROUP` and change a user's primary group with `usermod -g GROUP USER`. Security metadata uses `NEXASEC3`, persisting group membership and lockout state while retaining readers for `NEXASEC1` and `NEXASEC2`. See [Nexa OS 2.1](docs/NEXA_OS_2_1.md). Lockout is a simple persistent account policy, not a network-grade authentication service.


## Nexa OS 2.2 — Sessions and supplementary groups

NexaSecurity now supports multiple supplementary group memberships per account. Use `usermod -aG GROUP USER` to add a membership, `usermod -rG GROUP USER` to remove one, and `groups [USER]` to inspect memberships. Group-based file permission checks recognize both primary and supplementary groups.

The shell supports `logout`; after logout, commands are blocked until a successful `login USER PASSWORD`. Logging out also resets the shell working directory to `/`. Security metadata is now written as `NEXASEC4`; readers remain compatible with `NEXASEC1`, `NEXASEC2`, and `NEXASEC3` images.

This remains an educational host-side security model. Password entry is still supplied as command text by the current shell interface, and it is not a production authentication boundary.


## Nexa OS 2.3 — Directory permissions and session security

- Interactive terminal password prompts for login, account creation, and password changes disable terminal echo.
- Sessions expire after 15 minutes of inactivity and return to a logged-out state.
- Directory traversal requires execute permission on each ancestor; listing requires read and execute permission.
- Creating and deleting entries requires write and execute permission on the parent directory.
- File reads and execution require traversable parent directories.
- These are educational host-side checks, not a production security boundary.


## Nexa OS 2.4 — Virtual Memory Manager

- 256-byte paging with per-process virtual address spaces and physical frame allocation.
- Page map/unmap and protection changes (read-only, user-accessible, supervisor-only, executable/NX).
- User-mode and kernel-mode memory access checks with explicit page-fault errors.
- Physical frame zeroing on allocation and reuse after unmap/address-space destruction.
- Kernel exposes its memory manager for diagnostics and controlled page-table operations.
- Guest LOAD/STORE and kernel-mediated process RAM access now pass through the process page table. Instruction dispatch is guarded by a separate read-only executable code address space; instruction payloads remain in the VM instruction vector.



## Nexa OS 2.5 — Execute protection and NX

Each process now receives separate data and code address spaces. Data pages are writable and non-executable; code pages are read-only and executable. VM instruction dispatch checks the process code page's execute permission, and violations fault only that process. The instruction payload still resides in the host-side instruction vector; see [Nexa OS 2.5](docs/NEXA_OS_2_5.md) for the current boundary and tests.


## Nexa OS 2.6 — TLB and demand paging

A 16-entry simulated TLB now records hits/misses and invalidates stale translations after page-table changes. Process data pages are allocated on first access through a simulated page-fault handler; unmap and address-space destruction reclaim physical frames; `reapProcess(pid)` cleans up terminal processes and their mappings. See [Nexa OS 2.6 documentation](docs/NEXA_OS_2_6.md).


## Nexa OS 2.7 — Swap, LRU and memory-backed instruction fetch

Physical-memory pressure now triggers global LRU page eviction into simulated swap images; swapped pages are restored on demand. Guest instruction opcodes and operands are encoded into read-only executable pages and fetched/decoded from the process code address space. See [Nexa OS 2.7 documentation](docs/NEXA_OS_2_7.md).


## Nexa OS 2.8 — Disk-backed swap, ELF-like executables and privilege modes

- Optional host-file-backed swap stores evicted 256-byte pages and restores them on demand; in-memory swap remains the default.
- New executable images use the section-based `NEXAELF2` format with machine/version metadata and RX code-section flags. The loader remains compatible with legacy `NEXAEXE1` binaries.
- Host-mediated syscalls enter a scoped simulated Kernel privilege mode and reliably restore the previous mode after completion or exception.
- Guest memory access continues through User-mode page permissions; supervisor-only pages are blocked.
- See [Nexa OS 2.8 documentation](docs/NEXA_OS_2_8.md). This remains an educational simulator, not a hardware-enforced security boundary or standards-compliant ELF loader.


## Nexa OS 2.9 — Priority scheduling, timer preemption and IPC

- Priority levels 0–7 with round-robin fairness among Ready processes at the same priority, plus configurable priority aging to reduce starvation.
- Configurable instruction quantum; deterministic preemption when a simulated timer interrupt fires.
- Global instruction ticks, timer-interrupt and pending-interrupt counters with explicit acknowledgement.
- `SYS Yield` voluntarily ends the current scheduling slice.
- Bounded FIFO process inboxes, host APIs `sendMessage()` / `receiveMessage()`, and blocking guest receive with automatic wakeup.
- Guest syscalls `SendMessage` (11) and `ReceiveMessage` (12), validated RAM descriptors, bounded payloads and scheduler deadlock detection.
- Shell diagnostics and administration: `ps`, `sched`, `memstat`, root-only `nice PID PRIORITY`, `limit PID INSTRUCTIONS`, `aging TICKS`, `quantum INSTRUCTIONS`, `timer TICKS`, `swapfile HOST_PATH`, `kill PID` and `reap PID` (`aging 0` disables aging).
- See [Nexa OS 2.9 documentation](docs/NEXA_OS_2_9.md) for the guest ABI and scheduler semantics.


## Nexa OS 2.22 — Mutex deadlock detection

- `NexaKernel::hasMutexDeadlock()` traverses mutex wait-for dependencies and reports circular waits (including multi-process cycles).
- Acyclic blocking is not reported as a mutex deadlock; the existing `hasDeadlock()` remains a scheduler-stall indicator for when no process is runnable.
- Process termination cleanup and mutex owner handoff remove stale wait edges, allowing the cycle detector to recover.
- This is deterministic graph analysis for the simulator's kernel-managed mutexes; it does not detect arbitrary guest-level logical deadlocks.
