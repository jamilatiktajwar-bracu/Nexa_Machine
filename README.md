# Nexa Machine
Educational C++20 virtual machine and assembly environment. Not a hypervisor; cannot boot a guest OS.

## Features (0.4)
- 8-bit A/B CPU, 8-bit arithmetic, 16-bit RAM addressing
- configurable RAM up to 64 KiB
- assembly: `LOAD A, value`, `LOAD B, value`, `LOAD A, [address]`, `STORE [address]`, `ADD A, B`, `PRINT A`, `HALT`
- config and virtual disk API
- interactive console: `nexa console`
- optional Qt6 desktop editor and runner
- CMake, tests, GitHub Actions

## Build
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nexa
```
GUI auto-builds if Qt6 Widgets is installed. To explicitly disable: `-DNEXA_BUILD_GUI=OFF`.

## Commands
```sh
./build/nexa examples/addition.nexa
./build/nexa console
./build/nexa --config examples/configured_machine.json examples/memory_16bit.nexa
```
Console commands: help, load FILE, run, reset, status, mem ADDRESS, disk-read OFFSET COUNT, disk-write OFFSET BYTE..., disk-save, quit.

## GUI
Run `./build/nexa_gui` on systems where Qt6 Widgets is available. GUI has assembly editor, run/reset, output and status.

## Limitations
VM instructions currently run synchronously; no OS boot, isolation, privileged CPU, GUI device emulation or hardware virtualization.

MIT license.
