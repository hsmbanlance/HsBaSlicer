---
kind: design
name: Adopt a process-scope ThreadPool singleton with atomic work-stealing for layer-parallel stages
source: session
category: adr
---

# Adopt a process-scope ThreadPool singleton with atomic work-stealing for layer-parallel stages

_Source: coding plans from commit period d9ffcdd → ca9d258 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
The slicer pipeline already parallelizes slicing and C++ fills, but support generation, path generation, and G-code serialization still run per-stage on the main thread. Each call to `ThreadPool(n)` incurred construction/destruction overhead and fixed block sizes caused tail latency when some workers finished early.

## Decision drivers
- eliminate per-call thread-pool startup cost
- balance uneven work across layers via work-stealing
- keep Lua-backed support/fill serial (shared interpreter)
- zero behavioral regression between HSBA_PIPELINE_THREADS=1 and >1

## Considered options
- **Process-scope `PipelinePool()` + `std::atomic<int> cursor` work-stealing** — pros: single pool reused across all pipeline stages; dynamic load balancing avoids stragglers; progress callback throttled every 32 layers; no changes to existing Stage 2/4 callers; cons: requires moving `pipeline_parallel.hpp` into `base/parallel.hpp` so `support/` and `paths/` can include it without depending on `DllHsBaSlicer`
- **Keep per-call `ThreadPool(n)` with fixed blocks** — pros: no global state, simpler lifetime; cons: repeated construction/destruction of worker threads; fixed block granularity leaves idle workers at stage end; worse latency on uneven workloads

## Decision
Introduce `inline PipelinePool()` returning a static `ThreadPool`, sized from `HSBA_PIPELINE_THREADS` (default `hardware_concurrency()`), and rewrite `ParallelForLayers` to use an atomic cursor that workers poll until exhausted. Add `ParallelMapLayers<R>` as a typed variant. Move the header to `base/parallel.hpp` and keep `DllHsBaSlicer/pipeline_parallel.hpp` as a thin forwarding header.

## Consequences
All Dll pipelines (`fdm_pipeline`, `sla_pipeline`, `sls_pipeline`, `slm_pipeline`, `lom_pipeline`, `waam_pipeline`, `tdp_pipeline`) get automatic benefit in Stages 2/4 with zero caller changes. The single pool is shared across the process; environment must be set before first call. Coroutine code using `Utils::Task` does not touch this pool, so both coexist. Tests must assert output parity between `HSBA_PIPELINE_THREADS=1` and `>1`.