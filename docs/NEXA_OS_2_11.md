# Nexa OS 2.11 — Synchronization primitives

This milestone provides kernel-global counting semaphores and mutexes for coordination between simulated guest processes.

## Object creation (host API)
```cpp
auto semaphore = kernel.createSemaphore(0, 4); // initial count, max
auto mutex = kernel.createMutex();
```
IDs are 8-bit, start at 1, and objects are not destroyed/reused in this milestone. Each type has a maximum of 255 objects.

## Guest syscall ABI
Service number is placed in register A and the object ID in B:
| Service | ID | Behavior |
|---|---:|---|
| SemaphoreWait | 14 | Consume count, or block and retry the syscall when zero |
| SemaphoreSignal | 15 | Wake the oldest blocked waiter, otherwise increment count up to max |
| MutexLock | 16 | Acquire free mutex, or block; owner attempting recursive lock is rejected |
| MutexUnlock | 17 | Only owner can unlock; hands ownership directly to oldest valid waiter |

Syscalls return 1 on success and 0 on rejection. A blocking wait preserves the exact instruction and wakes to retry; it returns success when the resource is available.

## Host API
- `semaphoreWait(pid,id)`, `semaphoreSignal(id)`
- `mutexLock(pid,id)`, `mutexUnlock(pid,id)`
- `createSemaphore(initial,maximum)`, `createMutex()`

Semaphore constraints: maximum must be nonzero, and initial cannot exceed maximum. Signal at maximum is rejected. Mutex unlock by a non-owner is rejected. Wait queues are FIFO and stale/non-blocking waiters are skipped.

## Scope
Objects are kernel-global and IDs are host-created. There is no guest object creation or destruction, shared memory, priority inheritance, robust mutex recovery after owner death, or user-level authorization. The simulator API is educational, not POSIX-compatible.

## Tests
Regression tests cover guest wait blocking and wakeup, semaphore limits, mutex ownership, recursive-lock rejection and FIFO owner handoff.
