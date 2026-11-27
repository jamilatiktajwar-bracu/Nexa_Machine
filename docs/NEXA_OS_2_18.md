# Nexa OS 2.18 — Shared 16-bit atomic operations

Nexa OS 2.18 extends shared-memory synchronization with 16-bit atomic read-modify-write operations.

## Host API

- `sharedAtomicAdd16(pid, segment, offset, delta)`
- `sharedCompareSwap16(pid, segment, offset, expected, replacement)`

Both return the previous 16-bit value as `std::optional<uint16_t>`. Values use little-endian byte order. Addition wraps modulo 65536. Both bytes must be within the segment and the caller's attachment must be writable.

## Guest syscalls

| Number | Name | Descriptor at B | Return |
|---|---|---|---|
| 26 | SharedAtomicAdd16 | `[segment, offset, deltaLo, deltaHi, resultHi]` | Previous value low byte in A; high byte written to `resultHi` |
| 27 | SharedCompareSwap16 | `[segment, offset, expectedLo, expectedHi, replacementLo, replacementHi, resultHi]` | Previous value low byte in A; high byte written to `resultHi` |

The guest ABI returns the previous value's low byte in register A and writes its high byte to the final descriptor byte (`B+4` for syscall 26, `B+6` for syscall 27). Descriptors must therefore fit all five or seven bytes, respectively. Both operations execute synchronously within the single-core simulator and are serialized relative to scheduled guest execution. This is not a hardware SMP atomic guarantee.

## Shared semaphore model

Kernel semaphores created by `createSemaphore` are kernel-global objects identified by an ID. Any process that is given the ID can wait/signal the same semaphore, so they already provide process-shared synchronization. Wait queues are FIFO and blocked waiters are granted on signal. This milestone does not add persistent named semaphore handles or cross-kernel sharing.
