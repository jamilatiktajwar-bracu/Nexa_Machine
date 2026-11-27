# Nexa OS 2.12 — Advanced synchronization

## Changes
- **Priority inheritance:** Mutex owners temporarily inherit the highest effective priority of blocked waiters, including propagation across dependency chains.
- **Priority restoration:** Effective priorities are recomputed from base priority, aging, and currently blocked waiters. No permanent base-priority mutation.
- **syncstat:** shell command displays synchronization object counts, semaphore counts/maxima, mutex owner and waiter queue with effective priorities.
- **Regression tests:** boost on contention and restoration after unlock.

## API
- `NexaKernel::effectivePriority(pid)` — computed scheduler priority, range 0–7.
- `NexaKernel::syncDiagnostics()` — human-readable snapshot of kernel synchronization objects.
- Shell: `syncstat`

## Limitations
The simulator remains single-core and educational. Priority inheritance is a scheduling model, not SMP-safe locking. Synchronization objects are host-created, kernel-global, and limited to 255 IDs. Guest object lifecycle, shared memory, user authorization, and priority-ceiling protocols are not implemented by this milestone.
