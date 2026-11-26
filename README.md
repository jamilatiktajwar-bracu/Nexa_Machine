# Nexa Machine

## Nexa OS 1.0 kernel foundation
The embeddable `NexaKernel` can spawn executable images as isolated VM processes, schedule one instruction per process in deterministic round-robin order, track Ready/Running/Halted/Faulted states, collect console output, and expose a small host-mediated syscall API. See [docs/NEXA_OS_1_0.md](docs/NEXA_OS_1_0.md). This is an educational kernel foundation; it is not yet a standalone bootable OS.
Educational C++20 virtual machine and assembly environment (not a hypervisor).

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
- Portable executable binary format (`NEXAEXE1`) and CLI compile/load\n- In-memory virtual filesystem (directories, file CRUD, path traversal rejection)\n- Interactive GUI pixel drawing (left-click/drag white, right-click/drag erase)\n- NexaKernel process lifecycle, per-process VM isolation and round-robin scheduler\n- Kernel console output and host-mediated syscall API\n- NexaOS host shell prototype

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
