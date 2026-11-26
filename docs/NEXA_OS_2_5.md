# Nexa OS 2.5 — Execute Protection and NX

## Address spaces
Each process owns a data address space and a separate code address space (ID: PID with the high bit set). Data pages are writable/user-accessible/non-executable. Code pages are read-only/user-accessible/executable.

## Instruction dispatch
Before each guest instruction is dispatched, the VM invokes an instruction-fetch guard. The kernel resolves the guest program counter against the process code address space. Unmapped, supervisor-only, or non-executable code pages fault the process.

## Protection API
- `mapPage(pid, page, writable, userAccessible, executable)`
- `protectPage(pid, page, writable, userAccessible, executable)`
- `canExecute(pid, virtualAddress, access)`

## Tests
The test suite covers NX data pages, read-only code pages, execute permission denial, supervisor-only mappings, and process fault containment.

## Current limitation
The instruction payload is still stored in the VM's host-side instruction vector. The page table guards instruction dispatch and enforces execute permission, but does not yet fetch encoded instruction bytes from physical frames. A future release can move the executable representation into guest physical memory and add TLB/cache simulation.
