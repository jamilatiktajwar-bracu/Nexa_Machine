# Nexa OS 2.15 — Shared Virtual Pages

## Syscall interface
| Number | Name | B descriptor | Return |
|---|---|---|---|
| 18 | SharedAttach | [segment ID, base virtual page] at B | A=1 success, 0 failure |
| 19 | SharedDetach | [segment ID] at B | A=1 success, 0 failure |
| 20 | SharedRead | [segment ID, byte offset] at B | byte in A; invalid access faults |
| 21 | SharedWrite | [segment ID, byte offset, value] at B | A=1 success, 0 failure |
| 22 | SharedCreate | [size low, size high] at B | segment ID in A |

## Actual virtual mappings
Shared segments allocate reference-counted physical frames. SharedAttach maps each frame into the caller's address space starting at the requested virtual page. All pages are user-readable/writable and non-executable. Guest load/store instructions using virtual addresses access those physical frames directly, so updates are visible to other attached processes and the kernel byte APIs.

SharedDetach and process termination remove mappings and release frame references. Destroying a segment detaches all users and releases its frame ownership. Shared frames are excluded from paging eviction for now, so shared allocations/mappings can fail when physical RAM is exhausted.

## Example
For segment ID 3 mapped at virtual page 2, write descriptor bytes `[3,2]` at guest RAM address 40. Set B=40, A=18, execute syscall. Segment byte offset 5 is then available at virtual address `2*256+5 = 517`. Guest Store/Load there modifies the shared page.

SharedCreate uses descriptor `[sizeLo,sizeHi]`; SharedRead/Write remain available for checked descriptor-based byte access. Use `shmstat` to inspect segment size and attachment count.
