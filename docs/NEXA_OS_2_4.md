# Nexa OS 2.4 — Virtual Memory Manager

## Paging model
- 16-bit virtual address space divided into 256-byte pages.
- Each process ID owns an independent page table.
- Physical frames are allocated on map and zeroed before use.
- Unmap and address-space destruction release frames.

## Protection
- Writable/read-only page flag.
- User-accessible/supervisor-only page flag.
- User and kernel access modes.
- Unmapped, supervisor-only, and write-protected accesses raise explicit page-fault exceptions.

## API
See `include/nexa/memory_manager.hpp`. The kernel exposes `memoryManager()` for page-table operations and diagnostics.

## Scope
This release establishes and tests the paging allocator and page protection layer. The VM now routes guest LOAD/STORE and kernel-mediated process memory access through the process page table. Each spawned process receives mapped pages for its configured RAM; user-mode guest accesses are subject to page protection, while explicit kernel inspection/writes use supervisor access. Instruction bytes remain in the VM's executable instruction vector rather than being fetched from guest RAM.
