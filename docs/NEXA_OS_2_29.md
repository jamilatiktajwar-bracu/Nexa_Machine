# Nexa OS 2.29 — Memory pressure diagnostics

Nexa now exposes a compact read-only memory pressure snapshot through the root shell.

## Command

```
oomstat
```

Example fields:
- `pressure`: normal, high, or critical
- `free_percent`, `frames_free`, and `frames_total`
- `resident_pages` and `swapped_pages`
- `swap_ins`, `swap_outs`, and `swap_backend`

## Classification

- **normal**: more than 10% physical frames free
- **high**: 10% or less free, but at least one frame remains
- **critical**: no free frames

The command is advisory. Nexa's existing VM may evict pageable mappings and use its swap backend, but this diagnostic does not trigger reclaim, kill processes, or promise allocation recovery. Shared frames and pages that cannot be evicted may still result in allocation failure.

See `memstat` for the broader paging and TLB counters.
