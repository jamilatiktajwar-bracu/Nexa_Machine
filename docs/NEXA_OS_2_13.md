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


## Nexa OS 2.14 — Guest shared-memory syscalls

Guest syscall numbers 18–21:
- **18 SharedAttach**: B contains segment ID; attaches current process.
- **19 SharedDetach**: B contains segment ID; detaches current process.
- **20 SharedRead**: B points to a guest RAM descriptor [segment ID, byte offset], returns byte in A; invalid attachment or bounds faults.
- **21 SharedWrite**: B points to descriptor [segment ID, byte offset, value], returns 1 on success, 0 on denied/unattached/bounds failure.

Segments must currently be created through the host/kernel API. This syscall layer enables guest programs to access shared buffers without mapping them into virtual address space. Use shell `shmstat` to inspect segment IDs, sizes and attachment counts.
