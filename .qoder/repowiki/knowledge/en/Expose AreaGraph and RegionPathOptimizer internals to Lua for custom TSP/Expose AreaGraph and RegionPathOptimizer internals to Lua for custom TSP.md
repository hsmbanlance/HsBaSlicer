---
kind: design
name: Expose AreaGraph and RegionPathOptimizer internals to Lua for custom TSP
source: session
category: adr
---

# Expose AreaGraph and RegionPathOptimizer internals to Lua for custom TSP

_Source: coding plans from commit period 91ac74c → d9ffcdd — records intent at planning time; the implementation may lag or differ._

**Status:** accepted

## Context
Users need to plug in their own TSP heuristics (e.g. nearest-neighbor, brute-force) over the area-level cost graph rather than being locked into the built-in optimizer.

## Decision drivers
- extensibility of path ordering
- low overhead data export to Lua
- keeping the C++ optimizer as a single source of truth for gate-level routing

## Considered options
- **Expose raw AreaGraph + shortestPath to Lua** — pros: Maximum flexibility; users can recompute costs on demand; cons: Requires Lua-side graph traversal and repeated shortest-path calls; harder to serialize efficiently
- **Pre-bake FullGraphData / ExpandedGraphData snapshots via dump* methods** — pros: Single serialization pass; Lua gets ready-to-consume tables with distMatrix and vertex/edge lists; avoids re-running shortestPath from Lua; cons: Allocates copies of the internal graphs; snapshot becomes stale if the underlying AreaGraph changes

## Decision
Add `dumpExpandedGraph()` / `dumpPairwiseGraph()` on `AreaGraph` and `getFullGraph()` / `getExpandedGraph()` / `setCustomTour()` on `RegionPathOptimizer`, then bind them to Lua userdata so scripts receive `FullGraphData` and `ExpandedGraphData` tables and can feed a custom tour back through `setCustomTour`.

## Consequences
Lua scripts can implement arbitrary TSP algorithms over the exported cost matrix while still delegating gate-level route construction to the C++ optimizer. The snapshot approach trades extra memory allocation for simpler Lua-side code and avoids repeated shortest-path queries. The public dump structures (`ExpandedGraphDump`, `PairwiseGraphDump`, `FullGraphData`, `ExpandedGraphData`) become part of the stable C++ API surface.