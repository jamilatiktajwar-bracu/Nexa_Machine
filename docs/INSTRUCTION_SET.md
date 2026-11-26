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
| PRINT | A | Print A as decimal to host console |\n| IN | A, port | Read byte from keyboard port 0 (empty queue returns 0) |\n| OUT | A, port | Write A as character to display port 1 |
| HALT | none | Stop execution |

Labels are case-insensitive and must be alone on a line, ending with `:`.
Numeric literals accept decimal or 0x-prefixed hexadecimal. `#` and `;` begin comments.
The interpreter defaults to a 10,000,000 instruction budget per run to help stop accidental infinite loops. Pass an explicit budget to `VM::run` to change it.

## Snapshot commands
From the interactive console:
- `state-save path.nexa-state` — persist the loaded program and machine state.
- `state-load path.nexa-state` — restore snapshot (replaces current program and RAM).

Snapshots store registers, RAM, program, PC, stack, flags, halt state, and instruction count. They are binary files and should only be loaded from trusted sources.
\n## Device ports\n- Port 0: keyboard input. Queue text from the console using `keyboard TEXT` before running. The VM consumes one byte per `IN A, 0`.\n- Port 1: character display. `OUT A, 1` writes the character to the host console and device output buffer.\n\n## Debugger\n- `step` executes one instruction.\n- `break ADDRESS` sets an instruction-address breakpoint. `run` / `continue` stops when the program reaches it.\n- GUI has Run, Step, breakpoint setting, and a 64-byte hex RAM view.\n\n## Boot demo\n`examples/boot_message.nexa` is a startup-message demonstration. It is not a real bootloader or OS boot process.\n
## Nexa 0.8 extensions

`CONST NAME, value` (or `.CONST`) declares a numeric constant. `//` is an additional comment prefix.

`SYS` dispatches service using A: 0 halt, 1 output character in B, 2 keyboard byte into A, 3 clear framebuffer.

Graphics ports: OUT 2 sets X, OUT 3 sets Y, OUT 4 sets pixel color, OUT 5 draws at X/Y, OUT 6 clears using A (zero black, nonzero white). The display is a 64x32 monochrome framebuffer.

RAM watchpoints stop execution after a watched address is written. Console commands `watch ADDRESS`, `unwatch ADDRESS`, `mem-write ADDRESS BYTE`.
