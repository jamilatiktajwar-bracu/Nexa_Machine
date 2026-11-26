# Nexa Machine

Nexa Machine is an experimental virtual-machine project. Our first stage is a portable educational byte-sized VM; future stages will add a desktop manager and hardware-assisted virtualization.

## Nexa VM 0.2 (Assembler + Loader)

- 8-bit registers A and B
- 256 bytes of virtual RAM
- Assembly text parser and file loader
- CLI runs `.nexa` program files
- Instructions: `LOAD`, `ADD`, `STORE`, `PRINT`, `HALT`
- Decimal and hexadecimal values (e.g. `42`, `0x2A`)
- `#` and `;` comments
- C++20 / CMake

**This is an educational CPU emulator, not yet a hypervisor. It cannot boot a guest OS.**

## Build

Requirements: CMake 3.20+ and a C++20 compiler.

```sh
cmake -S . -B build
cmake --build build
./build/nexa
```

Windows: run the executable in the generated build directory (for example `build\\Debug\\nexa.exe`).

## Run your own assembly

Create `example.nexa`:

```asm
LOAD A, 19
LOAD B, 23
ADD A, B
PRINT A
HALT
```

Then:

```sh
./build/nexa example.nexa
```

Expected:

```
Nexa Machine VM 0.2
Output: 42
VM halted.
```

### Memory example

```asm
LOAD A, 77
STORE [10]
LOAD A, [10]
PRINT A
HALT
```

## Roadmap

- [x] VM execution core
- [x] Assembly parser and file loader
- [x] CLI program runner
- [ ] Unit tests and instruction encoding
- [ ] VM config / virtual disk
- [ ] Qt 6 desktop manager
- [ ] Hardware acceleration (KVM / WHPX)
- [ ] Bootable guest OS support

## License

MIT. See [LICENSE](LICENSE).
