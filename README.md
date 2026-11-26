# Nexa Machine
Educational C++20 virtual machine and assembly environment. This is an interpreter/emulator project, not a hypervisor.

## Features (0.7)
- 8-bit A/B registers, wraparound arithmetic, 16-bit RAM addressing (up to 64 KiB)
- CPU: LOAD, STORE, ADD, SUB, INC, DEC, CMP, PRINT, IN, OUT, HALT
- Control flow: JMP, JZ, JNZ, CALL, RET; labels
- Stack: PUSH/POP A/B
- Keyboard and character display device ports
- Debugger API and console single-step, instruction-address breakpoints
- Optional Qt6 GUI with breakpoint control and 64-byte hex RAM monitor
- Configurable RAM, virtual disk, console, snapshots
- Execution budget and automated tests/CI

## Device I/O
Port 0 is keyboard input. Console command `keyboard TEXT` queues bytes before program execution. Assembly reads a byte with `IN A, 0`; when no bytes remain, the input value is zero.
Port 1 is character display output. `OUT A, 1` writes the ASCII character to terminal and the VM device output buffer.

```asm
IN A, 0
OUT A, 1
HALT
```

## Debugger
- `step` executes one instruction in the console.
- `break ADDRESS` sets a breakpoint at an instruction address.
- `run` / `continue` executes until halt, budget exhaustion, or breakpoint.
- GUI includes Run, Step, breakpoint input, and RAM hex view.

## Boot sequence example
`examples/boot_message.nexa` displays a startup banner as a boot-sequence demo. This is bootloader groundwork only; it does not load an operating system or emulate firmware.

## Assembly reference
```asm
LOAD A, 5
LOAD B, 3
ADD A, B
SUB A, B
INC A
DEC A
CMP A, 0
JZ done
JNZ loop
JMP label
PUSH A
POP B
CALL function
RET
PRINT A
IN A, 0
OUT A, 1
HALT
```
Labels are case-insensitive and standalone. Numeric literals may be decimal or 0x-prefixed hex.

## Build and run
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nexa
./build/nexa examples/addition.nexa
./build/nexa examples/memory_16bit.nexa
./build/nexa examples/keyboard_echo.nexa
./build/nexa examples/boot_message.nexa
./build/nexa console
```
Qt6 Widgets is optional. Disable the GUI with `-DNEXA_BUILD_GUI=OFF`.

Console commands: help, load FILE, keyboard TEXT, run/continue, step, break ADDRESS, reset, status, mem ADDRESS, disk-read, disk-write, disk-save, state-save, state-load, quit.

Snapshot files are binary and should only be loaded from trusted sources. The VM is not sandboxed.

## Limitations
No OS boot, privileged CPU, sandboxing, or hardware virtualization. Device I/O is a small simulated interface. Instructions execute synchronously.

MIT license.
