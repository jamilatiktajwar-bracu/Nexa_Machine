# Nexa OS 2.30 — Safe memory recovery

Root can run:

```
oomreap
```

The kernel finds processes already in `Halted` or `Faulted` state and reaps them, releasing their code/data address spaces and related kernel bookkeeping. The command reports the number of reaped processes and current free physical frames.

It does **not** kill Ready, Running, Blocked, or Stopped processes. This conservative operation is a manual recovery tool to reclaim memory/resources after workloads have already exited or failed. It does not replace swapping or provide a guaranteed recovery from a live allocation failure.
