# Nexa Machine

Nexa Machine is an experimental virtual-machine project. The first milestone is a small, portable educational VM engine; later milestones will add a desktop manager and hardware-assisted virtualization.

## Current milestone: Nexa VM 0.1

- 8-bit virtual registers (A, B)
- 256-byte virtual RAM
- Small instruction set (LOAD, ADD, STORE, PRINT, HALT)
- Program execution with basic error checking
- C++20 and CMake

> This initial emulator is **not** a hypervisor and cannot boot Windows or Linux. Native virtualization (KVM/WHPX) is planned for a later milestone.

## Build

Requires CMake 3.20+ and a C++20 compiler.

```bash
cmake -S . -B build
cmake --build build
./build/nexa
```

On Windows, run `build\\Debug\\nexa.exe` (or the matching build configuration).

## Run

The first version runs a built-in demonstration program. Expected output:

```
Nexa Machine VM starting...
Output: 42
VM halted.
```

## Roadmap

1. [x] Minimal VM core and demo
2. [ ] Program loader and assembly-like text format
3. [ ] VM configuration and virtual disk format
4. [ ] Qt 6 desktop management UI
5. [ ] Native virtualization backend (KVM / WHPX)
6. [ ] Guest OS boot support

## License

MIT. See [LICENSE](LICENSE).
