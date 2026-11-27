# Nexa OS 2.10 — Process Signals

Nexa OS 2.10 adds a small, explicit process-signal facility to the educational kernel simulator. It is designed for lifecycle control and guest notification without pretending to implement the full POSIX signal model.

## Signal set

| Signal | Value | Kernel behavior |
|---|---:|---|
| TERM | 1 | Stops the process, records exit code 143 and reason `signal:TERM`, and closes process-local resources |
| KILL | 2 | Stops the process, records exit code 137 and reason `signal:KILL`, and closes process-local resources |
| STOP | 3 | Moves a live process into the non-runnable Stopped state |
| CONT | 4 | Resumes a Stopped process into Ready |
| USR1 | 10 | Enqueues a guest-readable notification |
| USR2 | 12 | Enqueues a guest-readable notification |

TERM and KILL are immediate simulator actions; there is no guest-installed termination handler. STOP prevents scheduling without destroying the VM context. CONT only resumes a process that is currently Stopped.

## Host API

- `sendSignal(pid, signal)` applies lifecycle control or enqueues a user signal.
- `receiveSignal(pid)` removes the oldest pending user signal.
- Each process has a bounded FIFO of 32 pending user signals. A full queue rejects new signals without overwriting older entries.
- Unknown PIDs raise `std::out_of_range`; attempts to signal terminal processes are rejected.

The `ps` command now displays `stopped` and a `signals=N` count. The root shell can issue:

```text
signal PID TERM
signal PID KILL
signal PID STOP
signal PID CONT
signal PID USR1
signal PID USR2
```

## Guest ABI

Service 13 is `SYS GetSignal`. It consumes and returns the oldest pending USR1/USR2 signal number in register A. It returns zero when no user signal is pending.

Example:

```asm
LOAD A, 13
SYS
; A = 10 (USR1), 12 (USR2), or 0 (no signal)
STORE 90
```

Signals are not automatically delivered to guest handlers. A guest can poll with `GetSignal` at a point where it is ready to process notifications. Signal queues are discarded when a process exits, faults, or is forcibly terminated.

## Scope and limitations

This is a compact educational mechanism, not POSIX conformance. It does not implement signal masks, priorities, real-time signals, process groups, handler registration, asynchronous handler execution, user ownership checks, or signal interruption of blocked syscalls. Only the shell command is root-restricted; trusted host code can use the kernel API directly.

## Validation

Regression tests cover STOP/CONT scheduling behavior, USR1/USR2 FIFO delivery, the 32-signal bound, guest polling through service 13, TERM exit reporting, shell syntax and process-state diagnostics.
