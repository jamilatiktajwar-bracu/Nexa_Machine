# Nexa OS 1.0 — Kernel foundation

NexaKernel is an embeddable guest-kernel prototype built over the Nexa VM. It provides:
- Process creation from validated executable images.
- Isolated VM instances per process (separate CPU state, RAM and framebuffer).
- Deterministic round-robin, one-instruction scheduling.
- Process states Ready, Running, Halted and Faulted.
- Kernel-owned console output buffer and VFS.
- Host-mediated syscall API: Exit, Write, Read, GetPid, Yield and FileExists.

## Current boundary
This is a kernel foundation, not a complete general-purpose OS. Scheduling is cooperative and instruction-granular. Processes do not yet share a protected address space. Syscalls are currently exposed through the host kernel API and are not wired to a guest trap/privilege transition. The VFS is in-memory. There is no relocation loader, paging, preemption timer, device IRQ controller or graphical window manager yet.

## Build and test
The nexa_kernel_tests target validates process lifecycle, scheduling and basic syscall behavior. Public API: include/nexa/kernel.hpp.
