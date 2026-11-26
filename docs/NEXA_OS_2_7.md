# Nexa OS 2.7 — Swap, LRU and Memory-Backed Instruction Fetch

## Physical memory pressure
The virtual memory manager now evicts the least-recently-used resident page when no free physical frame remains. Evicted pages are copied into a simulated swap image associated with their virtual mapping. Accessing a swapped page triggers a page fault, restores the saved bytes into a frame, and invalidates stale TLB entries.

## Metrics
- `swappedPages()`, `swapIns()`, `swapOuts()`
- `residentPages()`, `freeFrames()`
- Existing TLB hit/miss and page-fault counters remain available.

## Encoded instruction format
Guest instructions are encoded as three bytes in little-endian order:
1. Opcode byte
2. Operand low byte
3. Operand high byte

The kernel copies this representation into executable code pages, then changes those pages to read-only + executable. The VM fetch callback reads and decodes each instruction from the process code address space. Execute permission is checked for each byte, including instructions crossing a page boundary. Guest LOAD/STORE continues to use the separate data address space.

## Limits
This remains an educational simulator. Swap images are held in host memory, not a disk-backed swap partition; replacement is global LRU, not per-process working-set control. The VM retains a host-side program-length/index structure to bound control flow, but instruction contents are fetched from guest physical memory.
