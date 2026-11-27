# Nexa OS 2.22.0 — Mutex deadlock detection

Nexa OS now exposes `NexaKernel::hasMutexDeadlock()` to distinguish circular mutex waits from ordinary scheduler blocking.

## Detection model

The kernel builds a wait-for graph from its mutex table:
- Each process waiting on a mutex has a dependency edge to that mutex's current owner.
- A depth-first traversal tracks active and completed nodes.
- An edge to an active node indicates a cycle and therefore a mutex deadlock.
- Acyclic waits (for example, one process waiting for another process to unlock) are not reported as cycles.

The existing `hasDeadlock()` API is unchanged. It reports a scheduler stall when no process is Ready or Running and at least one process is Blocked. It is intentionally broader and does not prove a circular mutex dependency.

## Recovery

When a process terminates, synchronization cleanup removes it from waiter queues and releases mutexes it owns. Ownership handoff updates the wait-for graph immediately, so `hasMutexDeadlock()` reflects the recovered state on the next call.

## Validation

Regression tests cover:
- A single blocked waiter without a cycle.
- A two-process/two-mutex circular wait.
- Cycle removal and mutex handoff after one owner terminates.

This is an educational single-core simulator feature. It detects cycles in kernel-managed mutex dependencies, not arbitrary logical deadlocks in guest programs.
