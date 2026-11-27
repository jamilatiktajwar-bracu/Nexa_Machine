# Nexa OS 2.25 — IPC mailbox memory quota

Each process inbox is now limited by three independent bounds:

- 64 queued messages maximum
- 251 payload bytes maximum per message
- 8,192 aggregate queued payload bytes maximum per process

`NexaKernel::sendMessage()` calculates the current queued payload byte total and rejects a new message if it would exceed the aggregate quota. A rejected message is not enqueued and does not wake a blocked receiver. `receiveMessage()` removes the oldest FIFO message, immediately freeing its payload-byte capacity for later sends.

The `ProcessInfo::pendingMessages` and `pendingMessageBytes` diagnostics expose current inbox usage. The byte quota counts payload bytes only, not C++ container/allocator overhead or message metadata.

Regression tests fill the inbox to 8,032 bytes using 32 maximum-size messages, verify that another 251-byte message is rejected, dequeue one message, and confirm that a new message is accepted while the total remains within quota.
