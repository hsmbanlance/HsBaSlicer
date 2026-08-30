# Graph (Graph Structure & Algorithms)

The Graph component wraps Boost.Graph with a generic graph keyed by **external business IDs**, located in the header `utils/Graph.hpp` (header-only). Internally it uses `boost::adjacency_list<vecS, vecS, ...>`, while exposing interfaces addressed by a user-defined `VertexId`, hiding Boost vertex descriptors so business graphs can be extended directly.

## Namespaces

- `HsBa::Slicer::graph`: graph types (`BaseGraph`, `DirectedGraph`, `UndirectedGraph`)
- `HsBa::Slicer::graph::algorithm`: algorithms (BFS/DFS/Dijkstra/A*/MST/components/topological sort/max flow/genetic TSP)
- `HsBa::Slicer::graph::concepts`: template constraints (`TSPWeight`, `ArithmeticWeight`, `VertexIdType`, ...)

## Graph Types

```cpp
template <VertexIdType V,
          typename VP = boost::no_property,   // vertex property
          typename EP = boost::no_property,   // edge property
          typename W = double,                // weight type
          typename VD = void>                 // vertex "description" (side storage, separate from property)
class DirectedGraph;   // boost::bidirectionalS, supports inNeighbors

template <...>
class UndirectedGraph; // boost::undirectedS
```

Both derive from `BaseGraph`. Template constraints:

| Constraint | Requirement |
|------------|-------------|
| `VertexIdType` | `std::hash`-able, equality-comparable, copyable |
| `TSPWeight` | Supports `a + b` (returns its own type), `a < b`, default-constructible (zero value) |
| `ArithmeticWeight` | Arithmetic type (required by the native Boost algorithms) |

## BaseGraph Core API

| Method | Description |
|--------|-------------|
| `addVertex(id, prop = {})` | Add a vertex; duplicate ID throws `RuntimeError` |
| `hasVertex(id)` | Vertex existence query |
| `vertexProperty(id)` | Read/write reference to vertex property |
| `addEdge(from, to, weight, prop = {})` | Add an edge, returns `{EdgeDescriptor, bool}` |
| `hasEdge(from, to)` | Edge existence query |
| `edgeProperty(from, to)` | Read/write reference to edge property (undirected graphs tolerate reversed storage) |
| `weight(from, to)` / `weight(edge)` / `setWeight(from, to, w)` | Edge weight access |
| `neighbors(id)` | Out-edge neighbors |
| `predecessors(id)` | In-edge neighbors (directed only; `DirectedGraph` also exposes `inNeighbors`) |
| `allVertices()` / `allEdges()` | Full enumeration |
| `vertexCount()` / `edgeCount()` | Size queries |
| `internalGraph()` | Read/write reference to the underlying `boost::adjacency_list` (extension entry point, see below) |
| `findVertex(id)` / `vertexId(vd)` | Convert between ID and Boost vertex descriptor |
| `idToVertexMap()` / `vertexToIdMap()` | Read-only map access |

**VertexDescription**: when `VD != void`, `setVertexDescription` / `vertexDescription` / `hasVertexDescription` / `removeVertexDescription` / `vertexDescriptions()` attach auxiliary per-vertex-ID data (e.g. geometry objects) outside the graph structure; it takes no part in graph algorithms.

All ID-lookup failures throw `RuntimeError` (`base/error.hpp`).

## Algorithm Reference (`graph::algorithm`)

| Function | Graph kind | Weight requirement | Returns |
|----------|-----------|--------------------|---------|
| `genericDijkstra(g, sources)` | any | `TSPWeight` | `{dist, pred}` (keyed by vertex descriptor) |
| `genericDijkstraPath(g, sources, targets, outDist)` | any | `TSPWeight` | best path (multi-source, multi-target) |
| `bfs(g, start, onDiscover)` | any | none | visit order + optional discover callback |
| `dfs(g, start, onDiscover)` | any | none | same |
| `dijkstraWithPath(g, source)` | any | arithmetic | `{dist, pred}` (keyed by vertex ID) |
| `bellmanFord(g, source)` | any | arithmetic | distance map; `nullopt` on negative cycle |
| `astar(g, start, goal, heuristic)` | any | arithmetic | path; empty if unreachable |
| `prim(g, start)` | undirected | arithmetic | MST edges `(parent, child)` |
| `kruskal(g)` | undirected | arithmetic | same |
| `connectedComponents(g)` | undirected | none | vertex → component index |
| `strongComponents(g)` | directed | none | vertex → SCC index |
| `topologicalSort(g)` | directed | none | topological order |
| `maxFlow(g, source, sink)` | directed | arithmetic | max flow (Boykov-Kolmogorov; edge weight as capacity) |
| `GeneticTSP(g).solve(mustVisit)` | any | `TSPWeight` | `{tour, totalCost, generations}` |

`GeneticTSP` constructor: `(graph, popSize = 100, maxGen = 1000, mutRate = 0.02, cxRate = 0.8)`; edges are expected between all must-visit pairs (individuals with missing edges are penalized); `mustVisit.size() < 2` throws.

## Usage

### 1. Basic Construction and Query

```cpp
#include "utils/Graph.hpp"
using namespace HsBa::Slicer;

graph::DirectedGraph<std::string> g;
g.addVertex("A");
g.addVertex("B");
g.addEdge("A", "B", 1.5);

std::cout << g.weight("A", "B");            // 1.5
auto order = graph::algorithm::bfs(g, "A"); // {"A", "B"}
```

### 2. Properties and Descriptions

```cpp
struct NodeInfo { int layer = 0; };
graph::DirectedGraph<int, NodeInfo, boost::no_property, double, std::string> g;

g.addVertex(1);
g.vertexProperty(1).layer = 3;
g.setVertexDescription(1, "start region");
```

### 3. Shortest Path / TSP

```cpp
auto [dist, pred] = graph::algorithm::dijkstraWithPath(g, "A");
auto tour = graph::algorithm::GeneticTSP<decltype(g)>(g).solve({"A", "B", "C"});
```

### 4. Multi-Source Shortest Path with Custom Weights

`genericDijkstra` / `genericDijkstraPath` only require the weight to satisfy `TSPWeight` (addable, comparable, default-constructible), so non-arithmetic weights work; the native Boost algorithms (Dijkstra/Bellman-Ford/A*/MST/max flow) require arithmetic weights.

## Extension Guide

- **Plugging in unwrapped Boost algorithms**: call any Boost.Graph algorithm directly through `internalGraph()`; the algorithm templates only rely on the type aliases `internalGraph()`, `findVertex()`, `vertexId()`, `VertexId`, `WeightType`, etc., so new algorithms can follow the existing template signatures to participate in the generic dispatch (see the duck-typing style of `genericDijkstra`).
- **Adding graph types**: derive from `BaseGraph` and fix the direction tag (see the 3-line implementations of `DirectedGraph`/`UndirectedGraph`).
- **Attaching business data**: prefer the vertex property `VP` (participates in graph copies) or the vertex description `VD` (independent storage, can be heavy objects); avoid maintaining side maps yourself.
- **Exception contract**: lookup failures throw `RuntimeError` uniformly; keep the same exception type when extending. Error messages may rely on `detail::toString` (uses `operator<<` when available, otherwise prints `[vertex]`).

## Notes

- Graphs are stored with `vecS` and **do not support removing vertices/edges**; rebuild the graph when removal semantics are needed.
- Most algorithm results are `std::unordered_map` keyed by vertex ID (`genericDijkstra` is the exception, keyed by vertex descriptor).
- `maxFlow` treats edge weights as capacities and rebuilds an internal flow network; the original graph is not modified.
- `GeneticTSP` uses random engines, so results are non-deterministic; for stable results, fix the input scale and validate `totalCost`.
- Real-world usage: `LibHsBaSlicer/Path/path_optimizer.cpp` solves region visiting order via `AreaGraph` (see [AreaGraph documentation](./areagraph.md)).
