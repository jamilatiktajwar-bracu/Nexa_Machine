# Nexa OS 2.20 — Shared mapping isolation and revocation

Nexa OS 2.20 hardens shared-memory mapping failure and destruction behavior.

## Mapping rollback

A regression test reserves the second page of a two-page shared segment in a guest address space, then attempts to map the segment starting at the preceding free page. The first alias can be created, but the second-page collision must fail the operation and roll back the first alias. The test also verifies that the pre-existing guest mapping remains intact.

## Live destruction and revocation

A second test maps a two-page segment into a live guest and destroys the segment without first detaching it. Destruction must:
- Remove both guest virtual aliases.
- Revoke the process attachment so shared-memory reads fail.
- Release all backing shared frames and restore the physical free-frame count.

These tests complement the 40-cycle, two-process lifecycle stress suite introduced in 2.19.

## Scope

Nexa is an educational single-core VM/OS simulator. These are deterministic lifecycle and isolation regression tests, not SMP concurrency tests.
