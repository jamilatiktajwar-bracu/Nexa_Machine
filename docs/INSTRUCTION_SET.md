# Nexa VM Instruction Reference

Registers: A and B are unsigned 8-bit registers. Arithmetic wraps modulo 256.
RAM: 1 to 65,536 bytes, addressed with 16-bit values.
Flags: CMP and most value-loading/arithmetic instructions update the zero flag.

| Instruction | Operands | Semantics |
|---|---|---|
| LOAD | A, value | Load 8-bit immediate into A |
| LOAD | B, value | Load 8-bit immediate into B |
| LOAD | A, [address] | Read RAM into A |
| STORE | [address] | Write A to RAM |
| ADD | A, B | A = A + B (wraps) |
| SUB | A, B | A = A - B (wraps) |
| INC / DEC | A | Increment/decrement A (wraps) |
| CMP | A, B or A, value | Set zero flag when equal |
| JMP | label/address | Unconditional branch |
| JZ / JNZ | label/address | Branch if zero flag is set/clear |
| PUSH / POP | A or B | Shared LIFO stack |
| CALL / RET | label / none | Push return PC and return |
| PRINT | A | Print A as decimal |
| HALT | none | Stop execution |

Labels are case-insensitive and must be alone on a line, ending with `:`.
Numeric literals accept decimal or 0x-prefixed hexadecimal. `#` and `;` begin comments.
The interpreter defaults to a 10,000,000 instruction budget per run to help stop accidental infinite loops. Pass an explicit budget to `VM::run` to change it.

## Snapshot commands
From the interactive console:
- `state-save path.nexa-state` — persist the loaded program and machine state.
- `state-load path.nexa-state` — restore snapshot (replaces current program and RAM).

Snapshots store registers, RAM, program, PC, stack, flags, halt state, and instruction count. They are binary files and should only be loaded from trusted sources.
