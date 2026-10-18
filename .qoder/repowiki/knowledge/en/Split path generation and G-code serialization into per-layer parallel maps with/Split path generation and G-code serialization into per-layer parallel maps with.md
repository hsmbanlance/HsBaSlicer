---
kind: design
name: Split path generation and G-code serialization into per-layer parallel maps with ordered merge
source: session
category: adr
---

# Split path generation and G-code serialization into per-layer parallel maps with ordered merge

_Source: coding plans from commit period d9ffcdd → ca9d258 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
`GenerateGCodePathV2`, `GenerateGCodePath`, and `ToGCode` build per-layer data structures and append them to non-thread-safe containers (`GCodePath`, `std::ostringstream`). Running these loops on multiple threads would race on the destination container.

## Decision drivers
- parallelize the CPU-heavy per-layer work while preserving strict layer order
- avoid locking or copying GCodePath internals
- reduce string buffer reallocations during final concatenation

## Considered options
- **Two-phase: `ParallelMapLayers` produces per-layer results into a vector, then main thread merges in order** — pros: no locks on shared containers; `GCodePath` stays single-threaded; `ToGCode` switches to pre-sized `std::string` with `reserve`; cons: extra temporary vector of strings/PolygonsD per layer
- **Lock-guard each push_back / ostringstream <<** — pros: minimal code change; cons: serialization lock becomes a bottleneck; defeats parallelism goal; `GCodePath` is not designed for concurrent mutation

## Decision
Rewrite `GenerateGCodePathV2` and `GenerateGCodePath` to compute per-layer `PolygonsD` / `GPoint` segments via `ParallelMapLayers`, then concatenate on the main thread. Rewrite `ToGCode` to collect per-layer strings and join them into a pre-reserved `std::string`; `SaveGCode` reuses the new `ToGCode`. Spiral and continuous-wall remain serial.

## Consequences
Memory usage increases temporarily by one vector per parallelized stage (per-layer results). Output ordering is guaranteed because the merge loop runs on the main thread after `ParallelMapLayers` completes. Existing tests in `tests/Support`, `tests/Path`, and `tests/GCodePath` validate functional equivalence.