# Nexa OS 1.5 — Guest VFS file handles

Nexa OS 1.5 extends the guest syscall ABI with process-local file handles.

| Service | Name | Argument in B | Result in A |
|---:|---|---|---|
| 6 | OpenRead | RAM address of NUL-terminated absolute path | process-local handle |
| 7 | OpenWrite | RAM address of NUL-terminated absolute path | process-local handle |
| 8 | ReadByte | handle | next byte, or 0 at EOF |
| 9 | WriteByte | RAM address of two-byte descriptor: handle, byte | 0 on success |
| 10 | CloseFile | handle | 0 on success |

Open operations require the matching per-process Read or Write permission. Handles belong to the process that opened them. Guest paths are read only from that process's RAM and are limited to 255 bytes. OpenWrite updates an existing file in place; it does not create files. The one-byte result convention means a zero byte and EOF are not distinguishable yet.

The ABI is intentionally small and educational, not POSIX-compatible. It is mediated by the host kernel callback and is not a hardware privilege boundary.
