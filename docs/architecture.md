# Nexa Machine Architecture

## VM 0.3
Educational assembly interpreter; not a hypervisor and cannot boot a guest OS.
- CPU: 8-bit A/B registers and program counter.
- RAM size configurable to 64 KiB, but current instructions address only 256 bytes.
- Assembly parsed to instruction objects.
- Virtual disk: fixed-size file-backed image, bounds-checked operations.
- Configuration: JSON config for name, RAM, disk path and size.
- Manager API: load/start/reset/stop and state reporting.

Execution is synchronous until HALT. No isolation, privileged instructions, hardware virtualization, OS boot or GUI.

## Planned
1. 16-bit addressing and I/O instructions.
2. Qt desktop UI and VM controls.
3. Snapshots and disk image format.
4. KVM/WHPX hardware virtualization once emulator is stable.
