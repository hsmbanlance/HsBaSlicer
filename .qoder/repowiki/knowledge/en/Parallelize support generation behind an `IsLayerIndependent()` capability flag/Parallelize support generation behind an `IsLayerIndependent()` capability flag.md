---
kind: design
name: Parallelize support generation behind an `IsLayerIndependent()` capability flag
source: session
category: adr
---

# Parallelize support generation behind an `IsLayerIndependent()` capability flag

_Source: coding plans from commit period d9ffcdd → ca9d258 — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
`ISupport::GenerateAll` currently loops over layers sequentially. Many concrete supports (plane, honeycomb, SLA sacrificial) are layer-independent, while tree support may grow across layers and cannot safely run in parallel.

## Decision drivers
- unlock parallelism for independent supports without changing their algorithms
- preserve correctness for cross-layer generators like `FdmTreeSupport`
- keep Lua-based support paths unchanged

## Considered options
- **Add `virtual bool IsLayerIndependent() const { return true; }` and parallelize default `GenerateAll`** — pros: new subclasses opt out by overriding to false; existing plane/honeycomb/sacrificial defaults stay true; Lua `GenerateAll` override is untouched; cons: adds a virtual dispatch per support type; requires `support/` to depend on `base/parallel.hpp`
- **Parallelize only known-safe subclasses explicitly** — pros: no new base-class API surface; cons: fragile — any future subclass needs manual wiring; harder to audit correctness

## Decision
Expose `IsLayerIndependent()` on `ISupport` (default true) and rewrite the default `GenerateAll` to use `ParallelMapLayers<PolygonsD>`. `FdmTreeSupport` will override to false once cross-layer growth is implemented. Lua support remains serial.

## Consequences
Progress reporting for Stage 3 (`ReportProgress(45→60)`) is now driven by the parallel map's `on_progress`. A dedicated test verifies that `IsLayerIndependent()==true/false` produce identical outputs for the same input layers.