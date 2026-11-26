# Nexa OS 1.1 — Guest syscall bridge

Guest programs can invoke the kernel with the `SYS` instruction. Before `SYS`, register A contains the service number and B contains its 8-bit argument. For services that return a value, A receives the result.

| Service (A) | Name | B input | A result |
|---:|---|---|---|
| 0 | Exit | ignored | 0; process halts |
| 1 | Write | character byte | 0 |
| 2 | Read | ignored | next queued keyboard byte, or 0 |
| 3 | GetPid | ignored | low 8 bits of PID |
| 4 | Yield | ignored | 0 |
| 5 | FileExists | numeric path suffix | 1 if `/<B>` exists, else 0 |

Example:

```asm
LOAD A, 1
LOAD B, 72
SYS
LOAD A, 0
SYS
```

This writes `H` and exits. Syscalls execute through a host-mediated callback; this is not yet a hardware privilege boundary. Arguments and return values are currently 8-bit. Yield naturally returns control after the current instruction because the scheduler uses one-instruction quanta.

## Build and test
Build with CMake and run CTest. The kernel tests include guest-originated syscalls.
