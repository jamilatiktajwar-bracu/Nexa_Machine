# Nexa OS 2.31 — Automatic allocation recovery

The virtual-memory manager now supports an optional allocation-recovery callback. It runs only when the allocator has no unused physical frame and cannot find an evictable page (for example, all remaining frames are pinned by shared-memory ownership).

The callback is guarded against recursive recovery. If it frees a frame, allocation retries once; if no frame becomes available, the existing out-of-memory exception is retained. Callback exceptions propagate after the recursion guard is reset.

The kernel registers a recovery callback that reaps processes already in the **Halted** or **Faulted** state. It does not terminate Ready, Running, Blocked, or Stopped processes. Recovery remains conservative: it only releases resources belonging to processes that have already terminated.

A memory regression test pins the physical frames with shared-frame ownership, installs a callback that releases one frame, and confirms that a subsequent page mapping succeeds and all frames are reclaimed after cleanup.
