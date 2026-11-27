# Nexa OS 2.26 — Per-process resource accounting

Nexa OS 2.26 extends process snapshots with live resource usage for three kernel-managed resource classes.

## ProcessInfo metrics

- `pendingMessages`: queued IPC message count.
- `pendingMessageBytes`: total payload bytes currently held by the inbox.
- `openFileHandles`: number of process-local VFS handles currently open.
- `sharedMemoryAttachments`: number of shared-memory segments attached by the process.
- `pendingSignals`: number of queued process signals.

The kernel computes these values from the current process containers whenever `NexaKernel::processes()` creates a snapshot. They are not separately maintained counters, avoiding drift when a message is received, a handle is closed, a segment is detached, or process-exit cleanup runs.

## Shell diagnostics

The `ps` command adds the following fields to each process row:

- `inbox=N/MB`: queued messages and payload bytes.
- `files=N`: currently open VFS handles.
- `shm=N`: attached shared-memory segments.
- `signals=N`: pending signal count.

Example shape (values are illustrative):

```text
1 worker ready ... inbox=2/128B files=1 shm=1 signals=0 ...
```

## Validation

Regression tests verify that opening a VFS file increments `openFileHandles`, process exit clears it, shared-memory attach increments `sharedMemoryAttachments`, and detach returns the count to zero. Existing IPC count/byte and signal accounting remain part of the same process snapshot.

## Scope

These are diagnostic counts, not byte-accurate accounting of VFS metadata, shared physical frames, page tables, or total resident memory. Use `memstat` and shared-memory diagnostics for those separate views. Nexa remains an educational VM/OS simulator, not a production kernel.
