# Nexa OS 1.2 — Process isolation and fault containment

- Each process retains a private VM and RAM image.
- Kernel-side memory inspection APIs validate the PID and address against that process's RAM size. Out-of-range access throws `std::out_of_range`.
- A guest instruction fault marks only that process `Faulted`; the scheduler continues running other Ready processes.
- `ProcessInfo::faultReason` records the exception text for diagnostics.
- Scheduling remains deterministic round-robin with a one-instruction time slice. This is instruction-boundary preemption in the VM, not a host timer interrupt.

## Memory API

```cpp
kernel.writeProcessMemory(pid, address, value);
auto value = kernel.readProcessMemory(pid, address);
```

A process cannot address another process's RAM through these APIs. This is VM-level isolation, not a full guest/kernel privilege boundary: guest code still has its own full private RAM and no paging or shared-memory model.

## Fault containment
Invalid opcodes, invalid memory accesses, and unknown guest syscalls fault the current process. The kernel captures the reason and continues scheduling other runnable processes.

## Build and test
Run CMake configure/build and CTest. `nexa_kernel_tests` verifies separate process RAM, bounds checks, guest syscalls, and fault isolation.
