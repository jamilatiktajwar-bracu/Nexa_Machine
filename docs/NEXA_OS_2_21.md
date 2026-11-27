# Nexa OS 2.21 — Synchronization cleanup on process exit

Nexa OS 2.21 hardens semaphore and mutex lifecycle handling when a process exits while waiting or after receiving a handoff.

## Dead waiter removal

When a process terminates, is killed by signal, faults, exits through the guest syscall, or reaches its instruction budget, the kernel removes its PID from semaphore and mutex wait queues. This keeps synchronization diagnostics accurate and prevents later wakeups from targeting dead processes.

## Semaphore grant recovery

A semaphore signal may reserve a permit for a blocked waiter. If that process exits before retrying its wait operation, the kernel transfers the reserved permit to the next live blocked waiter. If no waiter remains, the permit is returned to the semaphore count, respecting its configured maximum.

## Mutex handoff

Mutex waiters belonging to an exiting process are removed. If the exiting process owns a mutex, the existing owner-death recovery path hands ownership to the next live waiter.

## Regression coverage

Tests cover:
- A blocked semaphore waiter exiting before a signal.
- A process exiting after receiving a semaphore grant, with the grant handed to another waiter.
- A blocked mutex waiter being terminated and removed from the queue.
- TERM/KILL signal paths applying the same waiter and grant cleanup.
- Existing owner-death handoff behavior.

Nexa is an educational single-core VM/OS simulator. Synchronization operations are scheduler-coordinated and do not claim SMP hardware atomicity.
