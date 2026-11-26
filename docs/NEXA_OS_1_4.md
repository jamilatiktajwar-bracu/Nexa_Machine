# Nexa OS 1.4 — Process-scoped VFS permissions

Nexa OS 1.4 adds a kernel-managed permission table for each process.

## API

- `grantFileAccess(pid, path, permissions)` grants Read, Write, and/or Execute access to an existing VFS path.
- `hasFileAccess(pid, path, permission)` checks that the process owns every requested permission.
- Permissions are independent per PID; a grant to one process does not grant access to another.
- Unknown PIDs throw; malformed or non-absolute paths are rejected.
- The guest `FileExists` syscall reports a file only when that process has Read permission.

A process starts with no VFS permissions. The host/kernel setup layer grants access explicitly. This is an educational access-control policy layer: the VFS still exposes a host-side API, and this does not yet provide hardware privilege rings, protected kernel memory, or a security boundary against malicious host code. File read/write syscalls and guest path arguments remain future work.
