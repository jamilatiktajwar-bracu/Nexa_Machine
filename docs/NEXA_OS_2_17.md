# Nexa OS 2.17 — Read-only shared-memory mappings

Nexa OS 2.17 adds a read-only mapping mode without changing the existing shared-memory ABI.

## Guest syscall 25: SharedAttachReadOnly

At guest address B, provide the existing two-byte descriptor:

| Byte | Meaning |
|---|---|
| B | Shared segment ID |
| B+1 | Base virtual page |

Return A=1 on success or A=0 on failure. The segment is mapped into the caller's address space with user read permission, no write permission, and no execute permission.

Syscall 18 (SharedAttach) remains read/write for compatibility. Both modes use syscall 19 (SharedDetach).

## Enforcement

Read-only mappings are protected at two levels:
- Guest LOAD is allowed; guest STORE is rejected by the virtual-memory page permission check.
- Kernel-mediated SharedWrite, SharedAtomicAdd and SharedCompareSwap cannot modify a segment through a read-only attachment.

A process may still have a separate read/write attachment to the same segment if it explicitly creates one. Permissions are per-process attachment, not a global segment property.

## Scope

This is a software-enforced permission model in the single-core Nexa simulator. It is not an SMP hardware memory-ordering primitive or a security boundary against host code.
