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
To read byte 4 from segment 3, guest RAM at address 40 contains segment ID (3) and RAM at 41 contains offset (4). Set B=40, A=20, then execute Syscall. The byte result is returned in A.

To write byte 0x77 at offset 5, RAM descriptor bytes 40..42 are [3,5,0x77], B=40, A=21, then execute Syscall.

## Diagnostics
The shell command `shmstat` lists current segment IDs, byte sizes and attachment counts.

## Scope and limitations
Segments are created through the host/kernel API for now; guest create syscall and actual VMM page mapping are not implemented. This is a shared kernel buffer accessed via checked guest syscalls. Read/write is byte-level, not atomic; use mutex/semaphore APIs to coordinate concurrent access.
