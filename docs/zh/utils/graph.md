# Graph (图结构与算法)

Graph 组件基于 Boost.Graph 封装了一套以**外部业务 ID 为主键**的通用图结构与常用图算法集合，位于头文件 `utils/Graph.hpp`（纯头文件库）。内部使用 `boost::adjacency_list<vecS, vecS, ...>`，对外暴露以用户自定义 `VertexId` 寻址的接口，屏蔽 Boost 内部顶点描述符，方便直接扩展业务图。

## 命名空间

- `HsBa::Slicer::graph`：图类型（`BaseGraph`、`DirectedGraph`、`UndirectedGraph`）
- `HsBa::Slicer::graph::algorithm`：算法（BFS/DFS/Dijkstra/A*/MST/连通分量/拓扑排序/最大流/遗传 TSP）
- `HsBa::Slicer::graph::concepts`：模板约束（`TSPWeight`、`ArithmeticWeight`、`VertexIdType` 等）

## 图类型

```cpp
template <VertexIdType V,
          typename VP = boost::no_property,   // 顶点属性
          typename EP = boost::no_property,   // 边属性
          typename W = double,                // 权重类型
          typename VD = void>                 // 顶点"描述"（独立于属性的外挂存储）
class DirectedGraph;   // boost::bidirectionalS，支持 inNeighbors

template <...>
class UndirectedGraph; // boost::undirectedS
```

两者均继承自 `BaseGraph`。模板约束：

| 约束 | 要求 |
|------|------|
| `VertexIdType` | 可 `std::hash`、可判等、可拷贝 |
| `TSPWeight` | 支持 `a + b`（返回自身类型）、`a < b`、默认构造（零值） |
| `ArithmeticWeight` | 算术类型（Boost 原生算法的要求） |

## BaseGraph 核心 API

| 方法 | 说明 |
|------|------|
| `addVertex(id, prop = {})` | 添加顶点；ID 重复抛 `RuntimeError` |
| `hasVertex(id)` | 顶点存在性查询 |
| `vertexProperty(id)` | 顶点属性读写引用 |
| `addEdge(from, to, weight, prop = {})` | 添加边，返回 `{EdgeDescriptor, bool}` |
| `hasEdge(from, to)` | 边存在性查询 |
| `edgeProperty(from, to)` | 边属性读写引用（无向图自动兼容反向存储） |
| `weight(from, to)` / `weight(edge)` / `setWeight(from, to, w)` | 边权重读写 |
| `neighbors(id)` | 出边邻居 |
| `predecessors(id)` | 入边邻居（仅有向图，`DirectedGraph` 另提供 `inNeighbors`） |
| `allVertices()` / `allEdges()` | 全量枚举 |
| `vertexCount()` / `edgeCount()` | 规模查询 |
| `internalGraph()` | 底层 `boost::adjacency_list` 读写引用（扩展入口，见下文） |
| `findVertex(id)` / `vertexId(vd)` | ID ↔ Boost 顶点描述符互转 |
| `idToVertexMap()` / `vertexToIdMap()` | 映射表只读访问 |

**顶点描述（VertexDescription）**：当 `VD != void` 时，可用 `setVertexDescription` / `vertexDescription` / `hasVertexDescription` / `removeVertexDescription` / `vertexDescriptions()` 在图结构之外挂载一份按顶点 ID 索引的辅助数据（如几何对象），不参与图算法。

所有按 ID 寻址失败的操作均抛 `RuntimeError`（`base/error.hpp`）。

## 算法一览（`graph::algorithm`）

| 函数 | 适用图 | 权重要求 | 返回值 |
|------|--------|----------|--------|
| `genericDijkstra(g, sources)` | 任意 | `TSPWeight` | `{dist, pred}`（按顶点描述符） |
| `genericDijkstraPath(g, sources, targets, outDist)` | 任意 | `TSPWeight` | 最优路径（多源多汇） |
| `bfs(g, start, onDiscover)` | 任意 | 无 | 访问顺序 + 可选发现回调 |
| `dfs(g, start, onDiscover)` | 任意 | 无 | 同上 |
| `dijkstraWithPath(g, source)` | 任意 | 算术 | `{dist, pred}`（按顶点 ID） |
| `bellmanFord(g, source)` | 任意 | 算术 | 距离表；存在负环返回 `nullopt` |
| `astar(g, start, goal, heuristic)` | 任意 | 算术 | 路径；不可达返回空 |
| `prim(g, start)` | 无向 | 算术 | 最小生成树边 `(parent, child)` |
| `kruskal(g)` | 无向 | 算术 | 同上 |
| `connectedComponents(g)` | 无向 | 无 | 顶点 → 分量编号 |
| `strongComponents(g)` | 有向 | 无 | 顶点 → 强连通分量编号 |
| `topologicalSort(g)` | 有向 | 无 | 拓扑序 |
| `maxFlow(g, source, sink)` | 有向 | 算术 | 最大流（Boykov-Kolmogorov，边权视为容量） |
| `GeneticTSP(g).solve(mustVisit)` | 任意 | `TSPWeight` | `{tour, totalCost, generations}` |

`GeneticTSP` 构造参数：`(graph, popSize = 100, maxGen = 1000, mutRate = 0.02, cxRate = 0.8)`；要求必访点两两间存在边（缺边个体以惩罚代价处理），`mustVisit.size() < 2` 抛异常。

## 使用方法

### 1. 基本建图与查询

```cpp
#include "utils/Graph.hpp"
using namespace HsBa::Slicer;

graph::DirectedGraph<std::string> g;
g.addVertex("A");
g.addVertex("B");
g.addEdge("A", "B", 1.5);

std::cout << g.weight("A", "B");           // 1.5
auto order = graph::algorithm::bfs(g, "A"); // {"A", "B"}
```

### 2. 携带属性与描述

```cpp
struct NodeInfo { int layer = 0; };
graph::DirectedGraph<int, NodeInfo, boost::no_property, double, std::string> g;

g.addVertex(1);
g.vertexProperty(1).layer = 3;
g.setVertexDescription(1, "起始区域");
```

### 3. 最短路 / TSP

```cpp
auto [dist, pred] = graph::algorithm::dijkstraWithPath(g, "A");
auto tour = graph::algorithm::GeneticTSP<decltype(g)>(g).solve({"A", "B", "C"});
```

### 4. 自定义权重的多源最短路

`genericDijkstra` / `genericDijkstraPath` 只要求权重满足 `TSPWeight`（可加、可比、可默认构造），可用于非算术权重；Boost 原生算法（Dijkstra/Bellman-Ford/A*/MST/最大流）要求算术权重。

## 扩展指南

- **接入未封装的 Boost 算法**：通过 `internalGraph()` 直接调用任意 Boost.Graph 算法；算法函数模板只依赖 `internalGraph()`、`findVertex()`、`vertexId()`、`VertexId`、`WeightType` 等类型别名，新增算法时照抄现有模板签名即可参与泛化（参考 `genericDijkstra` 的 duck-typing 写法）。
- **新增图类型**：继承 `BaseGraph` 并固定方向标签即可（参考 `DirectedGraph`/`UndirectedGraph` 各 3 行实现）。
- **业务挂载数据**：优先使用顶点属性 `VP`（参与图拷贝）或顶点描述 `VD`（独立存储、可为重对象），避免自行维护旁路映射。
- **异常契约**：查询类失败统一抛 `RuntimeError`；扩展时建议沿用同一异常类型，错误消息可依赖 `detail::toString`（支持 `operator<<` 的类型，否则输出 `[vertex]`）。

## 注意事项

- 图基于 `vecS` 存储，**不支持删除顶点/边**；需要删除语义时重建图。
- 算法返回的映射多为 `std::unordered_map`，键为顶点 ID（`genericDijkstra` 例外，键为顶点描述符）。
- `maxFlow` 将边权重视为容量，内部重建流网络，不修改原图。
- `GeneticTSP` 使用随机数引擎，结果非确定；需要稳定结果时固定输入规模并校验 `totalCost`。
- 实际使用参考：`LibHsBaSlicer/Path/path_optimizer.cpp` 通过 `AreaGraph`（见 [AreaGraph 文档](./areagraph.md)）完成区域访问顺序求解。
