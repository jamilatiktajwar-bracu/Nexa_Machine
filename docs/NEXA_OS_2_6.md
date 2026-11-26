# Nexa OS 2.6 — TLB and Demand Paging

## Simulated TLB
The virtual memory manager now caches translations by address-space ID and virtual page. It tracks TLB hits and misses, supports explicit flush, and invalidates an address space's cached entries on map, unmap, protection changes, and destruction. The cache has a fixed 16-entry capacity with simple replacement.

## Demand paging
Process data pages are no longer allocated eagerly at spawn. An unmapped data-page access raises a simulated page fault; the kernel page-fault callback allocates and zero-initializes the page, then retries translation. Code address spaces do not use this data-page allocator: an unmapped code page remains an execution fault.

## Memory reclamation
Unmapping and address-space destruction release physical frames. Released frames are zeroed when allocated again. TLB entries are invalidated when address spaces change. `NexaKernel::reapProcess(pid)` removes a halted or faulted process, destroys both data and code address spaces, clears per-process file permissions, and releases all mapped frames. Ready/running processes cannot be reaped.

## Instrumentation
- `tlbHits()`, `tlbMisses()`
- `pageFaults()`
- `flushTlb()`, `invalidateTlb(pid)`
- `translate(pid, address, access, writeAccess, executeAccess)`

## Scope
This is a deterministic educational simulation, not hardware paging. It has no concurrent CPU cores, asynchronous disk-backed swap, replacement policy for virtual pages, or hardware TLB timing model. Demand allocation is for data pages; code remains mapped at process creation.
