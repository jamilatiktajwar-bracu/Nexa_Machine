# Nexa OS 2.24 — IPC mailbox byte accounting

Nexa OS 2.24 adds per-process visibility into queued IPC payload memory.

## Process diagnostics

`ProcessInfo` exposes two mailbox metrics:

- `pendingMessages`: number of messages currently queued in the process inbox.
- `pendingMessageBytes`: sum of the payload byte lengths for all queued messages.

The byte total is calculated from the live FIFO inbox when `NexaKernel::processes()` creates its diagnostic snapshot. It therefore stays consistent after successful sends, receives, and process-exit cleanup without maintaining a separately mutable counter.

## Limits and behavior

The existing mailbox limits remain unchanged: at most 64 queued messages per process, with a maximum payload of 251 bytes per message. Empty payloads contribute zero bytes. The metrics describe payload bytes only; they do not include C++ container or allocator overhead.

## Regression coverage

Kernel tests verify that enqueue increments both metrics, dequeue reduces them, multiple messages sum correctly, and FIFO receive order is preserved.
