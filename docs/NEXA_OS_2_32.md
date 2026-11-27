# Nexa OS 2.32 — Recovery telemetry

The virtual memory manager exposes cumulative counters for recovery attempts, successes, and failures.

- Attempts increments when a configured recovery callback is invoked.
- Success increments only when the callback frees a frame that is successfully returned for allocation.
- Failure increments when a callback throws or returns without freeing a usable frame.

The counters are read-only and do not affect recovery behavior. Regression coverage verifies the successful recovery path.
