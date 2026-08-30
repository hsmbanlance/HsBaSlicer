#define BOOST_TEST_MODULE GraphTest
#include <boost/test/included/unit_test.hpp>

#include "utils/AreaGraph.hpp"
#include "utils/Graph.hpp"

#include <cmath>
#include <set>

using namespace HsBa::Slicer::graph;
using namespace HsBa::Slicer::graph::algorithm;

// ---------------------------------------------------------------------------
// 辅助类型与工具
// ---------------------------------------------------------------------------
struct CityInfo
{
    std::string name;
    double x, y;
    bool operator==(const CityInfo& o) const { return name == o.name && x == o.x && y == o.y; }
};

struct RoadInfo
{
    std::string roadName;
    int lanes;
};

struct CityAnalysis
{
    int population;
    std::string climate;
};

// 自定义非数字权重（TSPWeight 概念）
struct TravelCost
{
    int timeMinutes;
    int fuelCost;
    TravelCost() : timeMinutes(0), fuelCost(0) {}
    TravelCost(int t, int f) : timeMinutes(t), fuelCost(f) {}
    TravelCost operator+(const TravelCost& o) const { return {timeMinutes + o.timeMinutes, fuelCost + o.fuelCost}; }
    bool operator<(const TravelCost& o) const
    {
        return timeMinutes != o.timeMinutes ? timeMinutes < o.timeMinutes : fuelCost < o.fuelCost;
    }
    bool operator==(const TravelCost& o) const { return timeMinutes == o.timeMinutes && fuelCost == o.fuelCost; }
    friend std::ostream& operator<<(std::ostream& os, const TravelCost& c)
    {
        return os << "[" << c.timeMinutes << "," << c.fuelCost << "]";
    }
};

// ---------------------------------------------------------------------------
// Suite 1: 基础图操作
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(BaseGraphOperations)

BOOST_AUTO_TEST_CASE(add_vertex_and_has_vertex)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A", CityInfo{"A城", 0, 0});
    g.addVertex("B", CityInfo{"B城", 1, 1});
    BOOST_TEST(g.vertexCount() == 2);
    BOOST_TEST(g.hasVertex("A"));
    BOOST_TEST(g.hasVertex("B"));
    BOOST_TEST(!g.hasVertex("C"));
}

BOOST_AUTO_TEST_CASE(vertex_property_access)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A", CityInfo{"A城", 10.5, 20.3});
    auto& prop = g.vertexProperty("A");
    BOOST_TEST(prop.name == "A城");
    BOOST_TEST(prop.x == 10.5);
    BOOST_TEST(prop.y == 20.3);
    prop.x = 99.0;
    BOOST_TEST(g.vertexProperty("A").x == 99.0);
}

BOOST_AUTO_TEST_CASE(add_edge_and_weight)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addEdge("A", "B", 5.0, RoadInfo{"AB", 4});
    g.addEdge("B", "C", 3.0, RoadInfo{"BC", 2});
    BOOST_TEST(g.edgeCount() == 2);
    BOOST_TEST(g.weight("A", "B") == 5.0);
    BOOST_TEST(g.weight("B", "C") == 3.0);
    BOOST_TEST(g.hasEdge("A", "B"));
    BOOST_TEST(!g.hasEdge("A", "C"));
}

BOOST_AUTO_TEST_CASE(edge_property_access)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addEdge("A", "B", 5.0, RoadInfo{"AB", 4});
    auto& ep = g.edgeProperty("A", "B");
    BOOST_TEST(ep.roadName == "AB");
    BOOST_TEST(ep.lanes == 4);
    ep.lanes = 8;
    BOOST_TEST(g.edgeProperty("A", "B").lanes == 8);
}

BOOST_AUTO_TEST_CASE(neighbors)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("A", "C", 2.0, {});
    g.addEdge("A", "D", 3.0, {});
    auto nbs = g.neighbors("A");
    std::set<std::string> expected{"B", "C", "D"};
    std::set<std::string> actual(nbs.begin(), nbs.end());
    BOOST_TEST(actual == expected);
}

BOOST_AUTO_TEST_CASE(duplicate_vertex_throws)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    BOOST_CHECK_THROW(g.addVertex("A"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(missing_vertex_throws)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    BOOST_CHECK_THROW(g.vertexProperty("B"), std::runtime_error);
    BOOST_CHECK_THROW(g.weight("A", "B"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(all_vertices_and_edges)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("B", "C", 2.0, {});
    auto verts = g.allVertices();
    BOOST_TEST(verts.size() == 3);
    auto edges = g.allEdges();
    BOOST_TEST(edges.size() == 2);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 2: 顶点描述（VertexDescription）
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(VertexDescriptionTests)

BOOST_AUTO_TEST_CASE(description_storage_and_retrieval)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double, CityAnalysis> g;
    g.addVertex("A", CityInfo{"A城", 0, 0});
    g.setVertexDescription("A", CityAnalysis{1000000, "温带"});
    BOOST_TEST(g.hasVertexDescription("A"));
    BOOST_TEST(!g.hasVertexDescription("B"));
    const auto& desc = g.vertexDescription("A");
    BOOST_TEST(desc.population == 1000000);
    BOOST_TEST(desc.climate == "温带");
}

BOOST_AUTO_TEST_CASE(description_modification)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double, CityAnalysis> g;
    g.addVertex("A");
    g.setVertexDescription("A", CityAnalysis{100, "A"});
    g.vertexDescription("A").population = 200;
    BOOST_TEST(g.vertexDescription("A").population == 200);
}

BOOST_AUTO_TEST_CASE(void_description_no_storage)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    // 编译期验证：setVertexDescription 不可用
    // g.setVertexDescription("A", ...); // 编译错误
    BOOST_TEST(g.vertexCount() == 1);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 3: 遍历算法（BFS / DFS）
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(TraversalAlgorithms)

BOOST_AUTO_TEST_CASE(bfs_order)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("A", "C", 1.0, {});
    g.addEdge("B", "D", 1.0, {});
    g.addEdge("C", "D", 1.0, {});

    auto order = bfs(g, "A");
    BOOST_TEST(order.front() == "A");
    // A 的邻居 B, C 必须在 D 之前出现
    auto posB = std::find(order.begin(), order.end(), "B");
    auto posC = std::find(order.begin(), order.end(), "C");
    auto posD = std::find(order.begin(), order.end(), "D");
    BOOST_CHECK(posB != order.end());
    BOOST_CHECK(posC != order.end());
    BOOST_CHECK(posD != order.end());
    auto idxB = std::distance(order.begin(), posB);
    auto idxC = std::distance(order.begin(), posC);
    auto idxD = std::distance(order.begin(), posD);
    BOOST_TEST(idxD > idxB);
    BOOST_TEST(idxD > idxC);
}

BOOST_AUTO_TEST_CASE(dfs_order)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("B", "C", 1.0, {});
    g.addEdge("A", "D", 1.0, {});

    auto order = dfs(g, "A");
    BOOST_TEST(order.front() == "A");
    // DFS 应该深入一条分支
    std::set<std::string> visited(order.begin(), order.end());
    std::set<std::string> expectedABCD{"A", "B", "C", "D"};
    BOOST_TEST(visited == expectedABCD);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 4: 最短路径算法
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(ShortestPathAlgorithms)

BOOST_AUTO_TEST_CASE(dijkstra_correctness)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addVertex("E");
    g.addEdge("A", "B", 4.0, {});
    g.addEdge("A", "C", 2.0, {});
    g.addEdge("B", "C", 1.0, {});
    g.addEdge("B", "D", 5.0, {});
    g.addEdge("C", "D", 8.0, {});
    g.addEdge("C", "E", 10.0, {});

    auto [dist, pred] = dijkstraWithPath(g, "A");
    BOOST_TEST(dist["A"] == 0.0);
    BOOST_TEST(dist["B"] == 3.0);  // A->C->B
    BOOST_TEST(dist["C"] == 2.0);
    BOOST_TEST(dist["D"] == 8.0);  // A->C->B->D
    BOOST_TEST(pred["B"] == "C");
    BOOST_TEST(pred["C"] == "A");
    BOOST_TEST(pred["D"] == "B");
}

BOOST_AUTO_TEST_CASE(astar_correctness)
{
    // 网格图：A(0,0) -> B(1,0) -> C(2,0) -> D(2,1)
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A", CityInfo{"A", 0, 0});
    g.addVertex("B", CityInfo{"B", 1, 0});
    g.addVertex("C", CityInfo{"C", 2, 0});
    g.addVertex("D", CityInfo{"D", 2, 1});
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("B", "C", 1.0, {});
    g.addEdge("C", "D", 1.0, {});
    g.addEdge("A", "D", 10.0, {});  // 绕远路

    auto heuristic = [&](const std::string& id) -> double
    {
        const auto& goal = g.vertexProperty("D");
        const auto& cur = g.vertexProperty(id);
        return std::hypot(cur.x - goal.x, cur.y - goal.y);
    };

    auto path = astar(g, "A", "D", heuristic);
    BOOST_TEST(!path.empty());
    BOOST_TEST(path.front() == "A");
    BOOST_TEST(path.back() == "D");
    // A* 应该找到最短路径 A->B->C->D
    BOOST_TEST(path.size() == 4);
}

BOOST_AUTO_TEST_CASE(bellman_ford_no_negative_cycle)
{
    DirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("S");
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addEdge("S", "A", 5.0, {});
    g.addEdge("S", "B", -2.0, {});
    g.addEdge("A", "C", 3.0, {});
    g.addEdge("B", "A", -4.0, {});
    g.addEdge("B", "C", 7.0, {});

    auto result = bellmanFord(g, "S");
    BOOST_TEST(result.has_value());
    BOOST_TEST((*result)["S"] == 0.0);
    BOOST_TEST((*result)["A"] == -6.0);  // S->B(-2)->A(-4) = -6
    BOOST_TEST((*result)["B"] == -2.0);
    BOOST_TEST((*result)["C"] == -3.0);  // S->B->A->C = -2-4+3 = -3
}

BOOST_AUTO_TEST_CASE(bellman_ford_detects_negative_cycle)
{
    DirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("S");
    g.addVertex("A");
    g.addVertex("B");
    g.addEdge("S", "A", 1.0, {});
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("B", "A", -3.0, {});  // A->B->A = -2 负权环

    auto result = bellmanFord(g, "S");
    BOOST_TEST(!result.has_value());
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 5: 最小生成树
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(MinimumSpanningTree)

BOOST_AUTO_TEST_CASE(prim_correctness)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("A", "C", 4.0, {});
    g.addEdge("B", "C", 2.0, {});
    g.addEdge("B", "D", 5.0, {});
    g.addEdge("C", "D", 3.0, {});

    auto mst = prim(g, "A");
    BOOST_TEST(mst.size() == 3);
    double total = 0;
    for (const auto& [u, v] : mst)
    {
        total += g.weight(u, v);
    }
    BOOST_TEST(total == 6.0);  // AB(1) + BC(2) + CD(3) = 6
}

BOOST_AUTO_TEST_CASE(kruskal_correctness)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("A", "C", 4.0, {});
    g.addEdge("B", "C", 2.0, {});
    g.addEdge("B", "D", 5.0, {});
    g.addEdge("C", "D", 3.0, {});

    auto mst = kruskal(g);
    BOOST_TEST(mst.size() == 3);
    double total = 0;
    for (const auto& [u, v] : mst)
    {
        total += g.weight(u, v);
    }
    BOOST_TEST(total == 6.0);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 6: 连通分量与拓扑排序
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(ConnectivityAndTopology)

BOOST_AUTO_TEST_CASE(connected_components)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addVertex("E");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("B", "C", 1.0, {});
    // D, E 孤立

    auto comps = connectedComponents(g);
    BOOST_TEST(comps["A"] == comps["B"]);
    BOOST_TEST(comps["A"] == comps["C"]);
    BOOST_TEST(comps["D"] != comps["A"]);
    BOOST_TEST(comps["E"] != comps["A"]);
    BOOST_TEST(comps["D"] != comps["E"]);
}

BOOST_AUTO_TEST_CASE(strong_components)
{
    DirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addVertex("E");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("B", "C", 1.0, {});
    g.addEdge("C", "A", 1.0, {});  // A-B-C 强连通
    g.addEdge("D", "E", 1.0, {});  // D, E 各自独立

    auto scc = strongComponents(g);
    BOOST_TEST(scc["A"] == scc["B"]);
    BOOST_TEST(scc["A"] == scc["C"]);
    BOOST_TEST(scc["D"] != scc["A"]);
    BOOST_TEST(scc["E"] != scc["A"]);
    BOOST_TEST(scc["D"] != scc["E"]);
}

BOOST_AUTO_TEST_CASE(topological_sort)
{
    DirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addVertex("E");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("A", "C", 1.0, {});
    g.addEdge("B", "D", 1.0, {});
    g.addEdge("C", "D", 1.0, {});
    g.addEdge("D", "E", 1.0, {});

    auto topo = topologicalSort(g);
    BOOST_TEST(topo.size() == 5);
    // 验证拓扑序：所有边 u->v 满足 u 在 v 之前
    std::unordered_map<std::string, std::size_t> pos;
    for (std::size_t i = 0; i < topo.size(); ++i)
        pos[topo[i]] = i;
    BOOST_TEST(pos["A"] < pos["B"]);
    BOOST_TEST(pos["A"] < pos["C"]);
    BOOST_TEST(pos["B"] < pos["D"]);
    BOOST_TEST(pos["C"] < pos["D"]);
    BOOST_TEST(pos["D"] < pos["E"]);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 7: 最大流
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(MaxFlowTests)

BOOST_AUTO_TEST_CASE(max_flow_basic)
{
    DirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("S");
    g.addVertex("V1");
    g.addVertex("V2");
    g.addVertex("V3");
    g.addVertex("T");
    g.addEdge("S", "V1", 16.0, {});
    g.addEdge("S", "V2", 13.0, {});
    g.addEdge("V1", "V2", 10.0, {});
    g.addEdge("V1", "V3", 12.0, {});
    g.addEdge("V2", "V1", 4.0, {});
    g.addEdge("V2", "T", 14.0, {});
    g.addEdge("V3", "V2", 9.0, {});
    g.addEdge("V3", "T", 20.0, {});

    double flow = maxFlow(g, "S", "T");
    // 该图最大流为 26（割 {S,V1,V2} 容量 = 12+14 = 26；
    // 注意与 CLRS 六顶点版本不同，后者含 V4 时最大流才是 23）
    BOOST_TEST(flow == 26.0);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 8: 遗传算法 TSP
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(GeneticTSPTests)

BOOST_AUTO_TEST_CASE(tsp_numeric_small)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    // 完全图，对称
    g.addEdge("A", "B", 10.0, {});
    g.addEdge("A", "C", 15.0, {});
    g.addEdge("A", "D", 20.0, {});
    g.addEdge("B", "C", 35.0, {});
    g.addEdge("B", "D", 25.0, {});
    g.addEdge("C", "D", 30.0, {});

    GeneticTSP<decltype(g)> solver(g, 100, 300, 0.05, 0.8);
    auto result = solver.solve({"A", "B", "C", "D"});
    BOOST_TEST(!result.tour.empty());
    BOOST_TEST(result.tour.size() == 4);
    std::set<std::string> visited(result.tour.begin(), result.tour.end());
    std::set<std::string> expectedABCD{"A", "B", "C", "D"};
    BOOST_TEST(visited == expectedABCD);
    // 最优解应为 A->B->D->C->A = 10+25+30+15 = 80
    BOOST_TEST(result.totalCost >= 80.0);
    BOOST_TEST(result.totalCost <= 85.0);  // 允许遗传算法轻微次优
}

BOOST_AUTO_TEST_CASE(tsp_custom_weight)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, TravelCost> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addVertex("D");
    g.addEdge("A", "B", TravelCost{30, 50}, {});
    g.addEdge("A", "C", TravelCost{45, 80}, {});
    g.addEdge("A", "D", TravelCost{60, 100}, {});
    g.addEdge("B", "C", TravelCost{25, 40}, {});
    g.addEdge("B", "D", TravelCost{35, 60}, {});
    g.addEdge("C", "D", TravelCost{20, 30}, {});

    GeneticTSP<decltype(g)> solver(g, 100, 200, 0.05, 0.8);
    auto result = solver.solve({"A", "B", "C", "D"});
    BOOST_TEST(!result.tour.empty());
    BOOST_TEST(result.tour.size() == 4);
    // 最优时间: A->B->D->C->A = 30+35+20+45 = 130
    BOOST_TEST(result.totalCost.timeMinutes >= 130);
}

BOOST_AUTO_TEST_CASE(tsp_incomplete_graph)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addEdge("A", "B", 1.0, {});
    // A-C 和 B-C 无边

    GeneticTSP<decltype(g)> solver(g, 50, 100, 0.05, 0.8);
    auto result = solver.solve({"A", "B", "C"});
    // 由于图不完整，TSP 可能找不到有效路径，totalCost 会极大
    BOOST_TEST(result.tour.size() == 3);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 9: 区域-出入口复合图
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(AreaGraphTests)

BOOST_AUTO_TEST_CASE(area_basic_operations)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea("A", {.gates = {{"N", true, true}, {"S", true, true}}, .allowSameGateInOut = true});
    metro.addArea("B", {.gates = {{"E", true, true}, {"W", true, true}}, .allowSameGateInOut = false});

    BOOST_TEST(metro.areaCount() == 2);
    BOOST_TEST(metro.hasArea("A"));
    BOOST_TEST(metro.hasArea("B"));
    BOOST_TEST(!metro.hasArea("C"));
}

BOOST_AUTO_TEST_CASE(shortest_path_auto_gate_selection)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea(
        "A", {.gates = {{"N", true, true}, {"S", true, true}}, .allowSameGateInOut = true, .sameGateInternalCost = 0});
    metro.addArea(
        "B", {.gates = {{"E", true, true}, {"W", true, true}}, .allowSameGateInOut = true, .sameGateInternalCost = 0});
    metro.addArea("C", {.gates = {{"P", true, true}}, .allowSameGateInOut = true, .sameGateInternalCost = 0});

    // A->B: N->E=10, N->W=12, S->E=8, S->W=15
    metro.addRoute("A", "B", {{{"N", "E"}, 10}, {{"N", "W"}, 12}, {{"S", "E"}, 8}, {{"S", "W"}, 15}});
    // B->C: E->P=5, W->P=7
    metro.addRoute("B", "C", {{{"E", "P"}, 5}, {{"W", "P"}, 7}});
    // C->A: P->N=9, P->S=11
    metro.addRoute("C", "A", {{{"P", "N"}, 9}, {{"P", "S"}, 11}});

    auto pathAB = metro.shortestPath("A", "B");
    BOOST_TEST(!pathAB.empty());
    BOOST_TEST(pathAB.totalCost == 8);  // A.S -> B.E (最短)
    std::vector<std::string> expectedAreas{"A", "B"};
    BOOST_TEST(pathAB.areaPath == expectedAreas);
    BOOST_TEST(pathAB.entryGates[0] == "S");
    BOOST_TEST(pathAB.exitGates[0] == "S");
    BOOST_TEST(pathAB.entryGates[1] == "E");
    BOOST_TEST(pathAB.exitGates[1] == "E");

    auto pathAC = metro.shortestPath("A", "C");
    BOOST_TEST(!pathAC.empty());
    // A.S -> B.E [8] -> B.E -> C.P [5] = 13
    BOOST_TEST(pathAC.totalCost == 13);
}

BOOST_AUTO_TEST_CASE(same_gate_in_out_restriction)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea("A", {.gates = {{"N", true, true}},
                        .allowSameGateInOut = false,  // 禁止同口进出！
                        .sameGateInternalCost = 0});
    metro.addArea("B", {.gates = {{"E", true, true}}, .allowSameGateInOut = true, .sameGateInternalCost = 0});

    metro.addRoute("A", "B", {{{"N", "E"}, 5}});
    metro.addRoute("B", "A", {{{"E", "N"}, 5}});

    // A 只有一个出入口 N，但禁止同口进出
    // 所以 A 内部没有 entry->exit 边，A 无法作为中间站
    // 但 A 可以作为起点（entry=N）或终点（exit=N）
    auto pathAB = metro.shortestPath("A", "B");
    BOOST_TEST(!pathAB.empty());
    BOOST_TEST(pathAB.totalCost == 5);

    // A->A 应该无法找到路径（因为无法从 N exit 再 N entry）
    // 除非有 B 作为中转，但这里只有一个 gate
}

BOOST_AUTO_TEST_CASE(directional_gate_restriction)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea("A", {.gates = {{"N", true, true}, {"S", true, true}}, .allowSameGateInOut = true});
    metro.addArea("B", {.gates = {{"E", false, true}, {"W", true, false}},  // E 仅出，W 仅进
                        .allowSameGateInOut = false});

    metro.addRoute("A", "B", {{{"N", "W"}, 10}, {{"S", "W"}, 8}});
    metro.addRoute("B", "A", {{{"E", "N"}, 10}, {{"E", "S"}, 12}});

    auto pathAB = metro.shortestPath("A", "B");
    BOOST_TEST(!pathAB.empty());
    // B 只能 W 进，所以 A 必须选择能到达 W 的出口
    BOOST_TEST(pathAB.entryGates.back() == "W");

    auto pathBA = metro.shortestPath("B", "A");
    BOOST_TEST(!pathBA.empty());
    // B 只能 E 出
    BOOST_TEST(pathBA.exitGates.front() == "E");
}

BOOST_AUTO_TEST_CASE(area_tsp)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea("A", {.gates = {{"N", true, true}}, .allowSameGateInOut = true});
    metro.addArea("B", {.gates = {{"E", true, true}}, .allowSameGateInOut = true});
    metro.addArea("C", {.gates = {{"P", true, true}}, .allowSameGateInOut = true});

    metro.addRoute("A", "B", {{{"N", "E"}, 10}});
    metro.addRoute("B", "A", {{{"E", "N"}, 10}});
    metro.addRoute("B", "C", {{{"E", "P"}, 5}});
    metro.addRoute("C", "B", {{{"P", "E"}, 5}});
    metro.addRoute("C", "A", {{{"P", "N"}, 8}});
    metro.addRoute("A", "C", {{{"N", "P"}, 12}});

    auto tsp = metro.solveTSP({"A", "B", "C"}, 100, 200, 0.03, 0.85);
    BOOST_TEST(!tsp.tour.empty());
    BOOST_TEST(tsp.tour.size() == 3);
    std::set<std::string> visited(tsp.tour.begin(), tsp.tour.end());
    std::set<std::string> expectedABC{"A", "B", "C"};
    BOOST_TEST(visited == expectedABC);
    // 最优: A->C->B->A = 8+5+10 = 23 或 A->B->C->A = 10+5+8 = 23
    BOOST_TEST(tsp.totalCost >= 23);
    BOOST_TEST(tsp.totalCost <= 30);  // 允许遗传算法轻微次优
}

BOOST_AUTO_TEST_CASE(area_description)
{
    AreaGraph<std::string, std::string, int, std::string, std::string, CityAnalysis> metro;
    metro.addArea("A", {.gates = {{"N", true, true}}, .allowSameGateInOut = true}, "Line1",
                  CityAnalysis{50000, "温带"});

    BOOST_TEST(metro.hasAreaDescription("A"));
    BOOST_TEST(!metro.hasAreaDescription("B"));
    const auto& desc = metro.areaDescription("A");
    BOOST_TEST(desc.population == 50000);
    BOOST_TEST(desc.climate == "温带");
}

BOOST_AUTO_TEST_CASE(area_void_description)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea("A", {.gates = {{"N", true, true}}, .allowSameGateInOut = true});
    BOOST_TEST(metro.areaCount() == 1);
    // areaDescription 不可用（编译期禁用）
}

BOOST_AUTO_TEST_CASE(internal_cost_varies_by_gate)
{
    AreaGraph<std::string, std::string, int> metro;
    metro.addArea("A", {.gates = {{"N", true, true}, {"S", true, true}},
                        .allowSameGateInOut = true,
                        .sameGateInternalCost = 1,
                        .internalCosts = {{{"N", "S"}, 10}, {{"S", "N"}, 2}}});
    metro.addArea("B", {.gates = {{"E", true, true}}, .allowSameGateInOut = true});

    metro.addRoute("A", "B", {{{"N", "E"}, 5}, {{"S", "E"}, 5}});

    // A.N entry -> A.N exit [1] -> B.E [5] = 6
    // A.S entry -> A.S exit [1] -> B.E [5] = 6
    // A.N entry -> A.S exit [10] -> B.E [5] = 15
    // A.S entry -> A.N exit [2] -> B.E [5] = 7
    // 最优应为同口进出: 6
    auto path = metro.shortestPath("A", "B");
    BOOST_TEST(path.totalCost == 6);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 10: 有向图专属功能
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(DirectedGraphTests)

BOOST_AUTO_TEST_CASE(in_neighbors)
{
    DirectedGraph<std::string, CityInfo, RoadInfo, double> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addEdge("A", "B", 1.0, {});
    g.addEdge("C", "B", 2.0, {});

    auto preds = g.inNeighbors("B");
    std::set<std::string> expected{"A", "C"};
    std::set<std::string> actual(preds.begin(), preds.end());
    BOOST_TEST(actual == expected);
}

BOOST_AUTO_TEST_SUITE_END()

// ---------------------------------------------------------------------------
// Suite 11: 泛型 Dijkstra（非算术权重）
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_SUITE(GenericDijkstraTests)

BOOST_AUTO_TEST_CASE(generic_dijkstra_on_custom_weight)
{
    UndirectedGraph<std::string, CityInfo, RoadInfo, TravelCost> g;
    g.addVertex("A");
    g.addVertex("B");
    g.addVertex("C");
    g.addEdge("A", "B", TravelCost{10, 20}, {});
    g.addEdge("B", "C", TravelCost{5, 10}, {});
    g.addEdge("A", "C", TravelCost{20, 40}, {});

    auto sources = std::vector{g.findVertex("A")};
    auto [dist, pred] = genericDijkstra(g, sources);

    auto vdB = g.findVertex("B");
    auto vdC = g.findVertex("C");
    BOOST_TEST(dist.contains(vdB));
    BOOST_TEST(dist.contains(vdC));
    TravelCost expectedB{10, 20};
    TravelCost expectedC{15, 30};
    BOOST_TEST(dist[vdB] == expectedB);
    BOOST_TEST(dist[vdC] == expectedC);  // A->B->C
}

BOOST_AUTO_TEST_SUITE_END()