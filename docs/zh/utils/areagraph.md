# AreaGraph (区域-门禁图)

AreaGraph 组件（头文件 `utils/AreaGraph.hpp`，纯头文件库）实现了一个**两级图模型**：以"区域（Area）"为粗粒度顶点、以区域上的"门禁（Gate）"为进出点的层次图。它把每个区域展开为入口/出口顶点与内部穿越代价，把区域间路线展开为门禁到门禁的边，从而支持**自动选择每个区域从哪个门禁进、哪个门禁出**的最短路径与区域巡回（TSP）求解。

## 概念模型

```
Area A                          Area B
  entry(g1) ──internalCost──> exit(g1)
  entry(g2) ──internalCost──> exit(g2)
                    │
                    └── route 边 (exit gate → entry gate) ──> entry(g3) ...
```

- **区域**：一组门禁 + 门禁间内部穿越代价。
- **门禁**：区域边界上的候选进/出点，可单独配置仅可进、仅可出或双向。
- **路线（Route）**：两区域之间的门禁对代价表，`(出口门禁, 入口门禁) -> 权重`。
- 内部通过**展开图**（每个门禁拆成 entry/exit 两个顶点）惰性构建，`addArea`/`addRoute` 修改后置脏，首次求解时重建。

## 数据结构

```cpp
template <typename GateId, typename Weight>
struct GateInfo { GateId id; bool canEnter = true; bool canExit = true; };

template <typename GateId, typename Weight>
struct AreaConfig
{
    std::vector<GateInfo<GateId, Weight>> gates;
    bool allowSameGateInOut = true;        // 允许同一门禁既进又出
    Weight sameGateInternalCost = Weight{}; // internalCosts 缺省时的内部穿越代价
    std::map<std::pair<GateId, GateId>, Weight> internalCosts; // (进门禁, 出门禁) -> 代价
};
```

查询结果：

- `AreaPathResult`：`areaPath`（区域序列）+ `entryGates`/`exitGates`（每区域的进/出门禁，与 `areaPath` 等长）+ `totalCost`；`empty()` 表示不可达。
- `AreaTSPResult`：`tour`（区域巡回序）+ `entryGates`/`exitGates`（`vector<vector<GateId>>`，按巡回邻接收集）+ `totalCost` + `generations`。

## AreaGraph API

```cpp
template <concepts::VertexIdType AreaId, concepts::VertexIdType GateId, typename Weight,
          typename AreaProperty = boost::no_property, typename RouteProperty = boost::no_property,
          typename AreaDescription = void>
requires concepts::TSPWeight<Weight>
class graph::AreaGraph;
```

| 方法 | 说明 |
|------|------|
| `addArea(id, config, prop = {})` | 添加/覆盖区域；带 `AreaDescription` 的重载可附带描述 |
| `hasArea(id)` / `allAreas()` / `areaCount()` | 区域查询 |
| `areaProperty(id)` | 区域属性读写引用 |
| `setAreaDescription` / `areaDescription` / `hasAreaDescription` / `removeAreaDescription` / `areaDescriptions()` | 区域描述（仅 `AreaDescription != void` 时可用） |
| `addRoute(from, to, gateWeights, prop = {})` | 添加/覆盖区域间路线（门禁对代价表） |
| `shortestPath(from, to)` | 区域级最短路：起点从各可出门禁出发（初始代价为区域内"入口→该出口"的最小内部代价，无入口则以零代价出发），终点到达任一可进门禁即结束 |
| `shortestPath(from, fromExit, to, toEntry)` | 指定起区域出门禁、终区域进门禁的最短路 |
| `solveTSP(mustVisit, popSize=150, maxGen=500, mutRate=0.03, cxRate=0.85)` | 遗传算法巡回：先两两求 `shortestPath` 压缩为完全有向图，再调用 `algorithm::GeneticTSP`；`mustVisit.size() < 2` 抛异常 |

所有求解入口内部先调用 `ensureExpanded()` 惰性重建展开图。

## 使用方法

### 1. 区域级最短路

```cpp
#include "utils/AreaGraph.hpp"
using namespace HsBa::Slicer;

graph::AreaGraph<int, int, double> ag;

graph::AreaConfig<int, double> cfgA;
cfgA.gates = {{1, true, true}, {2, true, true}};
cfgA.sameGateInternalCost = 10.0;             // 缺省内部穿越代价
cfgA.internalCosts = {{{1, 2}, 3.0}};         // 从门禁1进、门禁2出，内部代价 3（更优）
ag.addArea(10, cfgA);

graph::AreaConfig<int, double> cfgB;
cfgB.gates = {{5, true, true}};
ag.addArea(20, cfgB);

ag.addRoute(10, 20, {{ {2, 5}, 1.5 }}); // A 门禁2 出 -> B 门禁5 进，代价 1.5

auto path = ag.shortestPath(10, 20);
// path.areaPath = {10, 20}, entryGates = {2, 5}, exitGates = {2, 5}, totalCost = 4.5
// （起区域从门禁2出发：内部代价取"门禁1进->门禁2出"的 3，路线 2->5 代价1.5；
//   起区域内部穿越计入代价但不追踪，故其记录门禁为出发门禁）
```

### 2. 区域巡回（TSP）

```cpp
auto tsp = ag.solveTSP({10, 20, 30});
for (std::size_t i = 0; i < tsp.tour.size(); ++i)
    ; // tsp.tour[i] -> tsp.tour[(i+1)%n]，对应 exitGates[i] / entryGates[(i+1)%n]
```

### 3. 属性与描述挂载

```cpp
struct AreaInfo { double areaSize = 0; };
graph::AreaGraph<int, int, double, AreaInfo, boost::no_property, std::string> ag2;
ag2.addArea(1, cfgA);
ag2.areaProperty(1).areaSize = 12.5;
ag2.setAreaDescription(1, "左侧填充区");
```

## 扩展指南

- **新增求解算法**：参照 `shortestPath`/`solveTSP` 的模式——调用 `ensureExpanded()` 后，在私有展开图（`expandedGraph_`，`boost::adjacency_list<vecS, vecS, directedS>`，顶点属性为 `{areaId, gateId, isExit}`）上实现；结果经 `solveShortestPath` 式的"展开路径折叠回区域序列"逻辑还原。
- **新增门禁语义**：门禁的方向性仅由 `canEnter`/`canExit` 控制；新增语义（如单向门禁组）可通过组合 `GateInfo` 配置实现，无需改类模板。
- **自定义代价**：`Weight` 只需满足 `TSPWeight`（可加、可比、可默认构造），可为时间、能量等复合类型；`internalCosts`/路线代价均为按门禁对查表，缺省回退 `sameGateInternalCost`。
- **与 Graph 组件协作**：`solveTSP` 内部用 `DirectedGraph` 压缩图 + `algorithm::GeneticTSP`；扩展其它巡回策略（如贪心最近邻）可复用相同压缩流程，见 [Graph 文档](./graph.md)。

## 注意事项

- `addRoute` 是有向的；需要双向通行时应注册两个方向。
- 起点区域的 `allowSameGateInOut` 限制不适用于初始出发（起点不受该限制）；起区域内部穿越只计入初始代价、不出现在结果路径中（其记录门禁为出发门禁）；终点区域到达入口即结束，不再穿越其内部。
- 不可达时 `shortestPath` 返回 `empty()` 的结果（不抛异常）；起区域无出门禁/终区域无进门禁则抛 `RuntimeError`。
- 展开图为惰性缓存：批量 `addArea`/`addRoute` 后首次求解一次性重建，避免逐条触发。
- 实际使用参考：`LibHsBaSlicer/Path/path_optimizer.cpp` 将多边形区域建模为 `AreaGraph<int, int, double>`（门禁 = 填充路径端点或多边形顶点），用于路径输出前的区域顺序优化。
