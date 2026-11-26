# Nexa OS 2.8 — Disk Swap, ELF-like Executables and Privilege Modes

## Disk-backed swap
`VirtualMemoryManager::setSwapFile(hostPath)` configures a host file for page eviction. The file begins with the `NEXASWP1` signature; each page-out appends one 256-byte page image and records its offset in the page mapping. Page-in seeks to that offset and restores the page. If no swap file is configured, the previous in-memory swap-image backend remains active. Configure the swap file before any pages have been swapped. The swap file is a simulator backing store, not a secure or crash-consistent swap partition.

## ELF-like executable format
New binaries are saved as `NEXAELF2`:
- machine ID 1, format version 2
- entry point and instruction count
- one code section with type 1 and flags 5 (read + execute, no write)
- virtual address 0 and byte length equal to three bytes per instruction
- opcode byte followed by a little-endian 16-bit operand

The loader continues to accept legacy `NEXAEXE1` images. It validates machine/version, section metadata, opcodes, truncation and trailing data. This is an ELF-inspired educational container, not a standards-compliant ELF implementation.

## User/kernel mode separation
The VM exposes `PrivilegeMode::User` and `PrivilegeMode::Kernel`. A registered host syscall handler runs during a scoped Kernel-mode transition; the prior mode is restored even when the handler throws. Guest code has no instruction or API to set its own privilege mode. Kernel-only page mappings remain inaccessible through guest User-mode LOAD/STORE and instruction-fetch paths.

## Security and implementation boundaries
This is a software simulation, not a hardware security boundary. Host callbacks and the C++ kernel share one process; swap files are unencrypted and not crash-consistent; no hardware MMU, ring levels, ELF dynamic linker, relocations, or signed executable verification are implemented.
