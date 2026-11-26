# Nexa Machine Architecture Notes

## Current: Nexa VM 0.2 (interpreter)

This project currently implements a deliberately small educational machine. It is not a type-1/type-2 hypervisor and does not virtualize physical hardware.

### CPU state

- Register A: 8-bit accumulator
- Register B: 8-bit secondary operand
- Program counter: host-side index into the decoded instruction vector
- RAM: 256 bytes, addressed 0 through 255
- Halt state: stops the interpreter loop

### Instructions

| Assembly | Behavior |
|---|---|
| `LOAD A, imm` | A = immediate byte |
| `LOAD B, imm` | B = immediate byte |
| `LOAD A, [addr]` | A = RAM[addr] |
| `STORE [addr]` | RAM[addr] = A |
| `ADD A, B` | A = (A + B) modulo 256 |
| `PRINT A` | print A as an unsigned decimal |
| `HALT` | stop execution |

Numbers may be decimal or hexadecimal (0x-prefixed). Assembly lines can use `#` or `; ` comments.

## Planned virtualization architecture

A future Nexa desktop application will use a backend abstraction:

- **VM Manager:** configuration, lifecycle, and process management
- **Virtual machine backend:** native acceleration where supported
- **Hardware abstraction:** CPU, memory, storage, display, and network devices
- **UI:** Qt 6 desktop front end

Target backends are Linux KVM and Windows WHPX. These are future work; current emulator code does not use either API.

## Safety boundary

Never run unknown guest images or user-provided VM configurations with elevated privileges. Native virtualization work will require explicit device access, careful input validation, and tests.
