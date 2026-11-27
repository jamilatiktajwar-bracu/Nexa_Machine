# Nexa OS 2.14 — Guest Shared Memory

## Guest syscalls
| Number | Name | Register / descriptor contract |
|---|---|---|
| 18 | SharedAttach | B = segment ID; A = service; returns 1/0 |
| 19 | SharedDetach | B = segment ID; A = service; returns 1/0 |
| 20 | SharedRead | B = guest RAM address of [segment ID, offset]; returns byte in A |
| 21 | SharedWrite | B = guest RAM address of [segment ID, offset, value]; returns 1 on success |

The syscall dispatcher uses A for syscall number and B for its argument, consistent with the VM syscall instruction.

## Example guest-side descriptor
To attach segment 3 at virtual page 2, RAM descriptor bytes 40..41 are [3,2], set B=40, A=18, then execute Syscall. The segment is mapped at 0x0200 onward. Guest loads/stores at these addresses directly access shared physical pages.

SharedCreate descriptor [sizeLo,sizeHi] creates a segment; attach then maps it. SharedRead/SharedWrite descriptors provide explicit kernel-checked byte operations.

## Diagnostics
The shell command `shmstat` lists current segment IDs, byte sizes and attachment counts.

## Scope and limitations
Shared mapping consumes physical frames and is not currently pageable; frame allocation can fail when RAM is exhausted. Shared pages are non-executable. Read/write is not atomic; use mutex/semaphore APIs to coordinate concurrent access.
