# Nexa OS 2.13 — Kernel-managed shared memory

## Features
- Bounded segment allocation: 1 to 65,536 bytes, initialized to zero.
- Attach/detach by process ID with membership tracking.
- Byte reads/writes with attachment checks and bounds checks.
- Destroy segments and clear attachments automatically.
- Segment metadata query reports ID, size, and attachment count.
- Attachments cleaned up on terminate, TERM/KILL, guest Exit/Halt, budget exhaustion, fault, and reap.

## Host/kernel API
- `createSharedMemory(bytes)` returns an 8-bit segment ID.
- `attachSharedMemory(pid,id)` and `detachSharedMemory(pid,id)`.
- `readSharedMemory(pid,id,offset)` returns optional byte.
- `writeSharedMemory(pid,id,offset,value)`.
- `sharedMemoryInfo()` returns segment summaries.
- `destroySharedMemory(id)`.

## Limitations
This is a kernel-managed shared buffer accessed through the host API. It is **not** mapped into guest virtual memory, does not yet have guest syscalls, synchronization primitives for shared-data races, permissions beyond attachment membership, or true kernel-thread support. IDs are limited to 255 and segments to 64 KiB.
