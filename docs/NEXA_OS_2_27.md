# Nexa OS 2.27 — Per-process memory accounting

## Overview

Nexa OS now reports memory residency per process using the virtual memory manager's live page tables. The process snapshot combines the process data address space with its separate executable-code address space.

## Process metrics

- `virtualPages`: all mapped pages in the process data and code address spaces.
- `residentPages`: mapped pages currently backed by physical frames.
- `swappedPages`: mapped pages currently stored in the simulated swap backend.
- `residentBytes`: resident pages multiplied by the 256-byte page size.

Shared-memory aliases are counted in each process address space that maps them. This is an address-space usage view; it is not a claim of unique physical ownership. The system-wide `memstat` counters remain the source for total physical frame usage.

## Shell

The `ps` command adds:
- `mem=RESIDENT/VIRTUAL` bytes
- `pages=RESIDENT/VIRTUAL`
- `swapped=N`

The byte totals use the 256-byte virtual-memory page size.

## Validation

Regression tests assert that a newly spawned process initially accounts for its executable page, then reports an additional resident data page after its first data-memory access. The tests also verify the corresponding byte total.

## Scope

This release adds accounting and observability only. It does not yet impose per-process memory quotas or guarantee isolation from host resource exhaustion. Nexa remains an educational VM/OS simulator.
