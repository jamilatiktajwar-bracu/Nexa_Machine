# Nexa Machine
An experimental educational virtual machine built with C++20. **This is not a hypervisor and cannot boot an operating system.**

## VM 0.3
- 8-bit A/B CPU and assembly interpreter
- Configurable RAM size (1 byte to 64 KiB; current instruction addresses limited to 8-bit)
- JSON-style machine config
- File-backed virtual disk with bounds-checked byte reads/writes
- VM manager API for load/start/stop/reset and state
- Runtime instruction counter
- CMake + tests + CI

## Build
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/nexa
```
Windows: run `build\\Debug\\nexa.exe` (or your generator's output path).

## Run assembly
```sh
./build/nexa examples/addition.nexa
```
Expected output includes `Output: 42`.

## Configuration
See `config/nexa.json` and `examples/configured_machine.json`. The config parser supports `name`, `ram_bytes`, `disk_path`, and `disk_bytes`. Config/manager/disk components are currently library-level APIs; the CLI is still a basic assembly runner.

## Roadmap
- [x] VM interpreter and assembler
- [x] CLI runner
- [x] Configurable RAM, configuration parser, disk image API, VM manager API
- [ ] Expand instruction addressing to 16-bit
- [ ] Interactive console and device I/O
- [ ] Qt desktop manager
- [ ] Snapshot/restore
- [ ] KVM/WHPX hardware virtualization
- [ ] Bootable guest support (long-term)

MIT License. See LICENSE.
