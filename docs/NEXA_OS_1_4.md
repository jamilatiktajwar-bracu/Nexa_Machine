# Nexa OS 1.4 — Process-scoped VFS permissions

Nexa OS 1.4 adds a kernel-managed permission table for each process.

## API

- `grantFileAccess(pid, path, permissions)` grants Read, Write, and/or Execute access to an existing VFS path.
- `hasFileAccess(pid, path, permission)` checks that the process owns every requested permission.
- Permissions are independent per PID; a grant to one process does not grant access to another.
- Unknown PIDs throw; malformed or non-absolute paths are rejected.
- The guest `FileExists` syscall reports a file only when that process has Read permission.

A process starts with no VFS permissions. The host/kernel setup layer grants access explicitly. This is an educational access-control policy layer: the VFS still exposes a host-side API, and this does not yet provide hardware privilege rings, protected kernel memory, or a security boundary against malicious host code. File read/write syscalls and guest path arguments remain future work.


## Nexa OS 1.5 — Guest file handles

The guest syscall ABI now includes:

| Service | Name | Argument in B | Result in A |
|---:|---|---|---|
| 6 | OpenRead | RAM address of NUL-terminated absolute path | process-local handle |
| 7 | OpenWrite | RAM address of NUL-terminated absolute path | process-local handle |
| 8 | ReadByte | handle | next byte, or 0 at EOF |
| 9 | WriteByte | RAM address of two-byte descriptor: handle, byte | 0 on success |
| 10 | CloseFile | handle | 0 on success |

Open operations require the matching per-process Read or Write permission. Handles belong to the process that opened them. Guest paths are read only from that process's RAM and are limited to 255 bytes. OpenWrite updates an existing file in place, one byte at a time; it does not create files. The one-byte result convention means a zero byte and EOF are not distinguishable yet. The guest ABI remains a prototype, not a POSIX-compatible interface.
