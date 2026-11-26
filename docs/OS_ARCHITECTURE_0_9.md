# Nexa OS Architecture Foundation (0.9)

This milestone is an architecture foundation, not a claim that Nexa runs a general-purpose guest OS yet.

## Components
- CPU/ISA: 8-bit A/B registers, 16-bit address space, assembly instructions and device ports.
- Executable: portable `NEXAEXE1` container. Signature (8 bytes), entry PC (u16 little-endian), instruction count (u32), followed by opcode/u16 operand records.
- VFS: normalized absolute paths, root directory, explicit directory creation, file CRUD and directory listing. Parent traversal is rejected.
- Interrupts: planned next increment: interrupt vector table, software interrupt instruction, interrupt-return instruction, pending device interrupts and masking.
- Graphics: existing 64x32 framebuffer; GUI exposes it. Interactive mouse drawing is planned for the next GUI pass.

## Guest startup
The boot demo is assembled by the current assembler. Host boot loading into an executable image will be added after binary format validation and VFS are exercised in CI.

## Security / limitations
The VFS is currently in-memory and has no host path passthrough. Executable files should be treated as untrusted; loader validates signature, metadata, opcodes and truncation. This version is an educational emulator, not a hardware hypervisor.
