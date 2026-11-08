---
kind: design
name: Expose spiral toolpath as a flat xyz polyline in the geometry snapshot
source: session
category: adr
---

# Expose spiral toolpath as a flat xyz polyline in the geometry snapshot

_Source: coding plans from commit period 68c42f0 → c513cb3 — records intent at planning time; the implementation may lag or differ._

## Context
The WebUI needs to render the continuous spiral/vase path, but only FDM/TDP/WAAM pipelines produce one and it is derived from per-layer outlines via `SpiralizeOuterWall`. The existing snapshot API (`HsBaGeometrySnapshot`) only held per-layer rings; adding a new field would break the C ABI unless backward-compatible.

## Decision drivers
- zero ABI break for existing callers
- single source of truth for toolpath order
- deterministic pure-function derivation

## Considered options
- **Append optional `path_xyz` vector to `GeometrySnapshot` with a default-arg overload on `CaptureLayerGeometry`** — pros: existing 7 call sites compile unchanged; snapshot owns lifetime; single malloc/free pair via pool
- **Recompute the helix at request time in restapi/webui** — pros: no snapshot storage cost; cons: recomputes deterministic work on every view switch; loses the authoritative export order

## Decision
Add `std::vector<double> path_xyz` to `GeometrySnapshot` and an overloaded `CaptureLayerGeometry(geometry_id, ..., path_xyz={})`; export through three new C functions (`HsBaGeometryPathPointCount`, `HsBaGetGeometryPath`, `HsBaFreeGeometryPath`) that return a separately allocated `HsBaGeometryPath_t` with bounds/z-range. Only FDM/TDP/WAAM pipelines pass a non-empty vector when `cfg.spiral_mode` is true.

## Consequences
The snapshot now carries an extra contiguous buffer (only for spiral runs). Callers must use the matching free function; the pool does not extend the snapshot's lifetime. The empty-walk segments between rings are implicit lines drawn by the UI, not data points.