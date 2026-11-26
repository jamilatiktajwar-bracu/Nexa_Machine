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


## Nexa OS 1.1

Guest programs can now invoke the kernel syscall bridge using the `SYS` instruction. Register A selects the service and register B supplies its 8-bit argument. See [Nexa OS 1.1 syscall ABI](docs/NEXA_OS_1_1.md). The bridge currently runs through a host callback and is a prototype, not a hardware privilege boundary.


## Nexa OS 1.2

Process RAM can be inspected through PID-checked kernel APIs, and faults are contained to the failing process. See [Nexa OS 1.2](docs/NEXA_OS_1_2.md).


## Nexa OS 1.3 — Persistent VFS

The virtual filesystem now supports portable disk-image persistence with validated loading (`NEXAVFS1`). Save and restore the directory tree and file contents using `VirtualFileSystem::saveImage()` and `loadImage()`. See [Nexa OS 1.3 documentation](docs/NEXA_OS_1_3.md). This remains a host-side image format, not a production journaled filesystem or a standalone boot disk.


## Nexa OS 1.4 — Process-scoped file permissions

Processes now receive no VFS permissions by default. The kernel can grant and check Read, Write, and Execute access independently per PID; the guest `FileExists` service requires Read permission. See [Nexa OS 1.4](docs/NEXA_OS_1_4.md). This is a software policy layer, not a hardware-enforced security boundary.


## Nexa OS 1.5 — Guest VFS file handles

Guest programs can open existing VFS files through permission-checked kernel syscalls, read/write bytes, and close process-local handles. Guest paths are read from process RAM. See [Nexa OS 1.5 documentation](docs/NEXA_OS_1_5.md). The ABI is a compact educational prototype.


## Nexa OS 1.6 — User-space shell and application launcher

Run `nexa --os` to open the kernel-backed VFS shell. It supports `ls`, `cat`, `touch`, `mkdir`, `write`, `rm`, `run`, and `ps`. The `run` command assembles and launches guest assembly stored in the VFS. See [Nexa OS 1.6 documentation](docs/NEXA_OS_1_6.md). The VFS is in-memory by default and the shell is an educational host-side interface, not a standalone bootable OS.


## Nexa OS 1.7 — Persistent shell workspace

The shell now supports `pwd`, `cd`, relative paths, and VFS persistence through `savefs HOST_PATH` / `loadfs HOST_PATH`. See [Nexa OS 1.7 documentation](docs/NEXA_OS_1_7.md).


## Nexa OS 1.8 — User accounts and file permissions

The shell now supports `useradd`, `login`, `whoami`, `chmod`, and `chown`, with owner/group/other mode-bit checks for file reads, writes, removal, and execution. See [Nexa OS 1.8](docs/NEXA_OS_1_8.md). This is an in-memory educational policy layer without password authentication or hardware-enforced isolation.


## Nexa OS 1.9 — Persistent account and permission metadata

VFS disk images now preserve shell users and file ownership/mode metadata. savefs stores versioned security metadata alongside the filesystem; loadfs restores it and resets the active session to root. Older images without metadata remain compatible. This is educational metadata persistence, not password authentication or cryptographic protection. See [Nexa OS 1.9](docs/NEXA_OS_1_9.md).


## Nexa OS 2.0 — Password authentication

Shell accounts now use salted PBKDF2-HMAC-SHA256 password digests, password-verified login, and persistent credential metadata in VFS images. Account commands: `useradd USER PASSWORD`, `login USER PASSWORD`, and `passwd PASSWORD`. OpenSSL Crypto is a build dependency. This remains an educational shell; command-line passwords are not hidden and disk images are not cryptographically authenticated. See [Nexa OS 2.0](docs/NEXA_OS_2_0.md).


## Nexa OS 2.1 — Account lockout and groups

The shell now locks an account after five consecutive failed login attempts. Root can recover an account with `passwd USER NEW_PASSWORD` or `unlock USER`. Root can create groups with `groupadd GROUP` and change a user's primary group with `usermod -g GROUP USER`. Security metadata uses `NEXASEC3`, persisting group membership and lockout state while retaining readers for `NEXASEC1` and `NEXASEC2`. See [Nexa OS 2.1](docs/NEXA_OS_2_1.md). Lockout is a simple persistent account policy, not a network-grade authentication service.


## Nexa OS 2.2 — Sessions and supplementary groups

NexaSecurity now supports multiple supplementary group memberships per account. Use `usermod -aG GROUP USER` to add a membership, `usermod -rG GROUP USER` to remove one, and `groups [USER]` to inspect memberships. Group-based file permission checks recognize both primary and supplementary groups.

The shell supports `logout`; after logout, commands are blocked until a successful `login USER PASSWORD`. Logging out also resets the shell working directory to `/`. Security metadata is now written as `NEXASEC4`; readers remain compatible with `NEXASEC1`, `NEXASEC2`, and `NEXASEC3` images.

This remains an educational host-side security model. Password entry is still supplied as command text by the current shell interface, and it is not a production authentication boundary.


## Nexa OS 2.3 — Directory permissions and session security

- Interactive terminal password prompts for login, account creation, and password changes disable terminal echo.
- Sessions expire after 15 minutes of inactivity and return to a logged-out state.
- Directory traversal requires execute permission on each ancestor; listing requires read and execute permission.
- Creating and deleting entries requires write and execute permission on the parent directory.
- File reads and execution require traversable parent directories.
- These are educational host-side checks, not a production security boundary.


## Nexa OS 2.4 — Virtual Memory Manager

- 256-byte paging with per-process virtual address spaces and physical frame allocation.
- Page map/unmap and protection changes (read-only, user-accessible, supervisor-only).
- User-mode and kernel-mode memory access checks with explicit page-fault errors.
- Physical frame zeroing on allocation and reuse after unmap/address-space destruction.
- Kernel exposes its memory manager for diagnostics and controlled page-table operations.
- This release establishes the paging subsystem API; VM instruction fetch/data accesses are not yet transparently translated through these page tables.

