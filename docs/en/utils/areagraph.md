# AreaGraph (Area-Gate Graph)

The AreaGraph component (header `utils/AreaGraph.hpp`, header-only) implements a **two-level graph model**: areas as coarse-grained vertices and gates on each area as entry/exit points. Each area is expanded into entry/exit vertices with internal traversal costs, and inter-area routes are expanded into gate-to-gate edges, enabling shortest-path and area-tour (TSP) solving that **automatically picks which gate to enter and which gate to leave each area through**.

## Conceptual Model

```
Area A                          Area B
  entry(g1) ──internalCost──> exit(g1)
  entry(g2) ──internalCost──> exit(g2)
                    │
                    └── route edge (exit gate → entry gate) ──> entry(g3) ...
```

- **Area**: a set of gates plus internal traversal costs between gates.
- **Gate**: a candidate entry/exit point on the area boundary; individually configurable as entry-only, exit-only, or both.
- **Route**: a gate-pair cost table between two areas, `(exit gate, entry gate) -> weight`.
- Internally an **expanded graph** (each gate split into an entry and an exit vertex) is built lazily: `addArea`/`addRoute` mark it dirty, and it is rebuilt on the first solve.

## Data Structures

```cpp
template <typename GateId, typename Weight>
struct GateInfo { GateId id; bool canEnter = true; bool canExit = true; };

template <typename GateId, typename Weight>
struct AreaConfig
{
    std::vector<GateInfo<GateId, Weight>> gates;
    bool allowSameGateInOut = true;         // allow entering and leaving through the same gate
    Weight sameGateInternalCost = Weight{}; // default internal cost when missing from internalCosts
    std::map<std::pair<GateId, GateId>, Weight> internalCosts; // (entry gate, exit gate) -> cost
};
```

Query results:

- `AreaPathResult`: `areaPath` (area sequence) + `entryGates`/`exitGates` (per-area gates, same length as `areaPath`) + `totalCost`; `empty()` means unreachable.
- `AreaTSPResult`: `tour` (area visiting order) + `entryGates`/`exitGates` (`vector<vector<GateId>>`, collected per tour adjacency) + `totalCost` + `generations`.

## AreaGraph API

```cpp
template <concepts::VertexIdType AreaId, concepts::VertexIdType GateId, typename Weight,
          typename AreaProperty = boost::no_property, typename RouteProperty = boost::no_property,
          typename AreaDescription = void>
requires concepts::TSPWeight<Weight>
class graph::AreaGraph;
```

| Method | Description |
|--------|-------------|
| `addArea(id, config, prop = {})` | Add/replace an area; the overload with `AreaDescription` also attaches a description |
| `hasArea(id)` / `allAreas()` / `areaCount()` | Area queries |
| `areaProperty(id)` | Read/write reference to the area property |
| `setAreaDescription` / `areaDescription` / `hasAreaDescription` / `removeAreaDescription` / `areaDescriptions()` | Area descriptions (only when `AreaDescription != void`) |
| `addRoute(from, to, gateWeights, prop = {})` | Add/replace an inter-area route (gate-pair cost table) |
| `shortestPath(from, to)` | Area-level shortest path: the source area departs from each exitable gate (initial cost = the minimal internal "entry → this exit" cost of the source area, or zero if it has no entry gates); reaching any enterable gate of the target area ends the search |
| `shortestPath(from, fromExit, to, toEntry)` | Shortest path with fixed source exit gate and target entry gate |
| `solveTSP(mustVisit, popSize=150, maxGen=500, mutRate=0.03, cxRate=0.85)` | Genetic tour: all-pairs `shortestPath` compresses the areas into a complete directed graph, then `algorithm::GeneticTSP` solves it; `mustVisit.size() < 2` throws |

All solving entry points call `ensureExpanded()` first to lazily rebuild the expanded graph.

## Usage

### 1. Area-Level Shortest Path

```cpp
#include "utils/AreaGraph.hpp"
using namespace HsBa::Slicer;

graph::AreaGraph<int, int, double> ag;

graph::AreaConfig<int, double> cfgA;
cfgA.gates = {{1, true, true}, {2, true, true}};
cfgA.sameGateInternalCost = 10.0;             // default internal traversal cost
cfgA.internalCosts = {{{1, 2}, 3.0}};         // enter gate 1, leave gate 2: cost 3 (better)
ag.addArea(10, cfgA);

graph::AreaConfig<int, double> cfgB;
cfgB.gates = {{5, true, true}};
ag.addArea(20, cfgB);

ag.addRoute(10, 20, {{ {2, 5}, 1.5 }}); // A exit gate 2 -> B entry gate 5, cost 1.5

auto path = ag.shortestPath(10, 20);
// path.areaPath = {10, 20}, entryGates = {2, 5}, exitGates = {2, 5}, totalCost = 4.5
// (the source area departs from gate 2: internal cost "enter g1 -> exit g2" = 3, route 2 -> 5 = 1.5;
//  source-internal traversal is charged but not tracked, so its recorded gate is the departure gate)
```

### 2. Area Tour (TSP)

```cpp
auto tsp = ag.solveTSP({10, 20, 30});
for (std::size_t i = 0; i < tsp.tour.size(); ++i)
    ; // tsp.tour[i] -> tsp.tour[(i+1)%n], with exitGates[i] / entryGates[(i+1)%n]
```

### 3. Properties and Descriptions

```cpp
struct AreaInfo { double areaSize = 0; };
graph::AreaGraph<int, int, double, AreaInfo, boost::no_property, std::string> ag2;
ag2.addArea(1, cfgA);
ag2.areaProperty(1).areaSize = 12.5;
ag2.setAreaDescription(1, "left fill region");
```

## Extension Guide

- **Adding solving algorithms**: follow the `shortestPath`/`solveTSP` pattern — call `ensureExpanded()`, implement on the private expanded graph (`expandedGraph_`, a `boost::adjacency_list<vecS, vecS, directedS>` whose vertex property is `{areaId, gateId, isExit}`), then fold the expanded path back into an area sequence as `solveShortestPath` does.
- **Adding gate semantics**: gate directionality is controlled solely by `canEnter`/`canExit`; new semantics (e.g. one-way gate groups) can be expressed by composing `GateInfo` configurations without changing the class template.
- **Custom costs**: `Weight` only needs to satisfy `TSPWeight` (addable, comparable, default-constructible) and can be a composite type such as time or energy; `internalCosts`/route costs are gate-pair lookup tables falling back to `sameGateInternalCost`.
- **Cooperating with the Graph component**: `solveTSP` internally uses a compressed `DirectedGraph` + `algorithm::GeneticTSP`; other tour strategies (e.g. greedy nearest-neighbor) can reuse the same compression flow, see [Graph documentation](./graph.md).

## Notes

- `addRoute` is directed; register both directions if bidirectional travel is needed.
- The source area's `allowSameGateInOut` restriction does not apply to the initial departure; source-internal traversal is only charged into the initial cost and does not appear in the result path (its recorded gate is the departure gate); reaching an entry of the target area ends the search without traversing its interior.
- When unreachable, `shortestPath` returns an `empty()` result (no exception); a source area without exit gates / a target area without entry gates throws `RuntimeError`.
- The expanded graph is a lazy cache: after a batch of `addArea`/`addRoute` calls it is rebuilt once on the first solve.
- Real-world usage: `LibHsBaSlicer/Path/path_optimizer.cpp` models polygon regions as `AreaGraph<int, int, double>` (gates = fill-path endpoints or polygon vertices) for region ordering before path output.
