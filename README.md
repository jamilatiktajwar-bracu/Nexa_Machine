# Nexa Machine
Educational C++20 virtual machine and assembly environment. This is an interpreter/emulator project, not a hypervisor; it cannot boot a guest OS.

## Features (0.6)
- 8-bit A/B registers, wraparound arithmetic, 16-bit RAM addressing (up to 64 KiB)
- CPU instructions: LOAD, STORE, ADD, SUB, INC, DEC, CMP, PRINT, HALT
- Control flow: JMP, JZ, JNZ, CALL, RET; label support
- Stack operations: PUSH A/B, POP A/B
- Configurable RAM, virtual disk API, console, optional Qt6 desktop
- Runtime instruction counter, stack depth, bounded execution (10M instructions default)\n- Save/load binary VM snapshots via API and console
- Automated C++ tests and GitHub Actions CI

## Assembly quick reference
```asm
LOAD A, 5
LOAD B, 3
ADD A, B       # A = 8
SUB A, B       # A = 5
INC A
DEC A
CMP A, 0       # updates zero flag
JZ done
JNZ loop
JMP label
PUSH A
POP B
CALL function
RET
PRINT A
HALT
```
Labels are standalone tokens ending in colon. CMP sets the zero flag if the operands match. JZ/JNZ branch based on that flag. CALL pushes the return address; RET restores it. Stack is a shared LIFO for data and return addresses.

## Countdown sample
```asm
LOAD A, 3
loop:
PRINT A
DEC A
CMP A, 0
JNZ loop
HALT
```

## Build and run
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nexa
./build/nexa examples/addition.nexa
./build/nexa examples/memory_16bit.nexa
./build/nexa console
./build/nexa --config examples/configured_machine.json examples/memory_16bit.nexa
```
Qt6 Widgets is optional; if installed, CMake builds `nexa_gui`. Disable with `-DNEXA_BUILD_GUI=OFF`.

Console commands: help, load FILE, run, reset, status, mem ADDRESS, disk-read OFFSET COUNT, disk-write OFFSET BYTE..., disk-save, state-save FILE, state-load FILE, quit.\n\nSnapshots preserve program, RAM, registers, program counter, stack, flags, halt state, and instruction counter.

## Limitations
No privileged CPU, device emulation layer, OS boot, sandboxing, or hardware virtualization. Instructions currently execute synchronously.

MIT license.
