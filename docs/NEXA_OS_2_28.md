# Nexa OS 2.28 — Per-process memory limits

## Overview

Nexa OS supports configurable per-process virtual page budgets. The configured budget includes the process's executable-code address space and data address space. Shared-memory mappings also consume the mapping budget of each process that maps them.

## Configure a limit

As root, run:

```text
memlimit PID PAGES
```

The page count is measured in 256-byte virtual pages. A value of `0` disables the limit. The kernel rejects a new limit if it is lower than the process's current mapped-page usage.

## Enforcement

Private data pages are checked by the virtual-memory manager before allocating a frame. Shared-memory mappings are checked before attaching a mapping. If a running process attempts to access a new page beyond its quota, the page-fault handler reports a memory-limit error and the scheduler isolates the fault to that process.

The quota is a virtual mapped-page limit, not a reservation of physical RAM. Swapping does not reduce virtual page usage. Existing mappings remain valid if the limit is changed to the current usage or higher.

## Diagnostics

`ps` reports `memlimit=N`, or `memlimit=unlimited` when disabled, in addition to resident and virtual memory usage.

## Validation

Regression tests cover a process that successfully allocates within its budget, faults on the next page, retains its configured limit in process diagnostics, rejects lowering a limit below current usage, and permits disabling the limit.

## Scope

This is an educational VM/OS simulator. The page quota limits virtual mappings; it is not a host operating-system memory sandbox.
