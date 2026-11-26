# Nexa Machine Architecture
Nexa is an educational assembly VM, not a hypervisor and does not boot operating systems.
- 8-bit A/B CPU registers, arithmetic wraps at 8 bits.
- RAM configurable from 1 byte to 64 KiB.
- 16-bit memory addresses; immediate values remain 8-bit.
- Assembly instructions: LOAD A/B, ADD A,B, STORE [address], LOAD A,[address], PRINT A, HALT.
- File-backed virtual disk with checked byte read/write.
- Config file and VM manager API.
- CLI demo, assembly runner and interactive console.
- Optional Qt6 graphical assembly editor/runner.

Build the Qt GUI if Qt6 Widgets is installed. Otherwise the core and CLI build without Qt.

No process isolation, privileged instructions, real hardware virtualization, or guest OS boot is implemented.
Future work: instruction set expansion, memory-mapped I/O, snapshot/restore, native KVM/WHPX research.
