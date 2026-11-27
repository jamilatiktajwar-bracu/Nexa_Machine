# Nexa OS 2.23.0 — Deadlock cycle diagnostics

Nexa OS 2.22 introduced circular mutex wait detection. Version 2.23 adds a diagnostic API that identifies the processes in the first detected cycle.

## APIs

- `bool hasMutexDeadlock() const`: returns true if a circular mutex wait exists.
- `std::vector<std::uint32_t> mutexDeadlockCycle() const`: returns the process IDs participating in the first detected cycle, or an empty vector when no cycle exists.

The returned vector lists each process in the cycle once; the first PID is not repeated at the end. Its order follows the wait-for graph traversal and is diagnostic, not a scheduling order.

## Detection and recovery

The kernel traverses dependencies from a blocked process to the current owner of each mutex it is waiting for. A back-edge to an active DFS node identifies a cycle. Acyclic wait chains are not reported.

Normal synchronization cleanup removes terminated waiters and hands off mutex ownership. The cycle report therefore reflects the updated graph on the next query.

## Validation

Tests verify that a two-process/two-mutex cycle returns exactly the two participating PIDs, and that terminating an owner removes the cycle report.

This feature covers kernel-managed mutex dependencies in the educational single-core simulator; it does not identify arbitrary application-level logical deadlocks.
