#ifndef HSBA_SLICER_GRAPH_HPP
#define HSBA_SLICER_GRAPH_HPP

#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <format>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/astar_search.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/graph/boykov_kolmogorov_max_flow.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <boost/graph/named_function_params.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/property_map/property_map.hpp>

#include "base/concepts.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer
{
namespace graph::concepts
{

template <typename T>
concept TSPWeight = requires(T a, T b)
{
    {a + b}->std::same_as<T>;
    {a < b}->std::convertible_to<bool>;
    {T{}}->std::same_as<T>;
};

template <typename T>
concept ArithmeticWeight = std::is_arithmetic_v<T>;

template <typename T>
concept DirectedGraphTag = std::is_same_v<T, boost::directedS> || std::is_same_v<T, boost::bidirectionalS>;

template <typename T>
concept UndirectedGraphTag = std::is_same_v<T, boost::undirectedS>;

template <typename T>
concept VertexIdType = StdHash<T> && std::equality_comparable<T> && std::copyable<T>;

}  // namespace graph::concepts

namespace graph::detail
{

template <typename T, typename = void>
struct is_ostreamable : std::false_type
{
};

template <typename T>
struct is_ostreamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> : std::true_type
{
};

template <typename T>
std::string toString(const T& t)
{
    if constexpr (is_ostreamable<T>::value)
    {
        std::ostringstream oss;
        oss << t;
        return oss.str();
    }
    else
    {
        return "[vertex]";
    }
}

template <typename Desc, typename VertexId>
struct VertexDescStorage
{
    std::unordered_map<VertexId, Desc> data;
    void set(const VertexId& id, const Desc& d) { data[id] = d; }
    Desc& get(const VertexId& id) { return data.at(id); }
    const Desc& get(const VertexId& id) const { return data.at(id); }
    bool has(const VertexId& id) const { return data.contains(id); }
    void erase(const VertexId& id) { data.erase(id); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
    bool empty() const { return data.empty(); }
    std::size_t size() const { return data.size(); }
};

template <typename VertexId>
struct VertexDescStorage<void, VertexId>
{
    void set(const VertexId&, const void*) {}
    void get(const VertexId&) const {}
    bool has(const VertexId&) const { return false; }
    void erase(const VertexId&) {}
    bool empty() const { return true; }
    std::size_t size() const { return 0; }
    struct iterator
    {
    };
    iterator begin() const { return {}; }
    iterator end() const { return {}; }
};

}  // namespace graph::detail

namespace graph
{

template <concepts::VertexIdType IdType, typename VertexProperty, typename EdgeProperty, typename Weight, typename DirectionTag, typename VertexDescription = void>
class BaseGraph
{
public:
    using DirectionTagType = DirectionTag;
    using VertexId = IdType;
    using WeightType = Weight;
    using VertexPropertyType = VertexProperty;
    using EdgePropertyType = EdgeProperty;
    using VertexDescriptionType = VertexDescription;

    using InternalEdgeProp =
        boost::property<boost::edge_weight_t, WeightType, boost::property<boost::edge_all_t, EdgeProperty>>;

    using GraphType = boost::adjacency_list<boost::vecS, boost::vecS, DirectionTag, VertexProperty, InternalEdgeProp>;

    using VertexDescriptor = typename boost::graph_traits<GraphType>::vertex_descriptor;
    using EdgeDescriptor = typename boost::graph_traits<GraphType>::edge_descriptor;

    VertexDescriptor addVertex(const VertexId& id, const VertexProperty& prop = {})
    {
        if (idToVertex_.contains(id))
        {
            throw RuntimeError(std::format("Vertex already exists: {}", detail::toString(id)));
        }
        auto v = boost::add_vertex(prop, graph_);
        idToVertex_[id] = v;
        vertexToId_[v] = id;
        return v;
    }

    bool hasVertex(const VertexId& id) const { return idToVertex_.contains(id); }

    VertexProperty& vertexProperty(const VertexId& id)
    {
        auto v = findVertex(id);
        return graph_[v];
    }

    const VertexProperty& vertexProperty(const VertexId& id) const
    {
        auto v = findVertex(id);
        return graph_[v];
    }

    template <typename VD = VertexDescription>
    requires(!std::is_same_v<VD, void>) void setVertexDescription(const VertexId& id, const VD& desc)
    {
        if (!hasVertex(id))
            throw RuntimeError(std::format("Vertex not found: {}", detail::toString(id)));
        vertexDescriptions_.set(id, desc);
    }

    template <typename VD = VertexDescription>
    requires(!std::is_same_v<VD, void>) VD& vertexDescription(const VertexId& id)
    {
        if (!hasVertex(id))
            throw RuntimeError(std::format("Vertex not found: {}", detail::toString(id)));
        return vertexDescriptions_.get(id);
    }

    template <typename VD = VertexDescription>
    requires(!std::is_same_v<VD, void>) const VD& vertexDescription(const VertexId& id) const
    {
        if (!hasVertex(id))
            throw RuntimeError(std::format("Vertex not found: " + detail::toString(id)));
        return vertexDescriptions_.get(id);
    }

    template <typename VD = VertexDescription>
    requires(!std::is_same_v<VD, void>) bool hasVertexDescription(const VertexId& id) const
    {
        return vertexDescriptions_.has(id);
    }

    template <typename VD = VertexDescription>
    requires(!std::is_same_v<VD, void>) void removeVertexDescription(const VertexId& id)
    {
        vertexDescriptions_.erase(id);
    }

    template <typename VD = VertexDescription>
    requires(!std::is_same_v<VD, void>) const auto& vertexDescriptions() const
    {
        return vertexDescriptions_.data;
    }

    std::pair<EdgeDescriptor, bool> addEdge(const VertexId& from, const VertexId& to, WeightType weight,
                                            const EdgeProperty& prop = {})
    {
        if (!hasVertex(from))
            throw RuntimeError(std::format("Vertex not found: {}", detail::toString(from)));
        if (!hasVertex(to))
            throw RuntimeError(std::format("Vertex not found: {}", detail::toString(to)));
        auto u = findVertex(from);
        auto v = findVertex(to);
        auto [e, ok] = boost::add_edge(u, v, InternalEdgeProp{weight, prop}, graph_);
        if (ok)
            edgeProps_[{from, to}] = prop;
        return {e, ok};
    }

    bool hasEdge(const VertexId& from, const VertexId& to) const
    {
        if (!hasVertex(from) || !hasVertex(to))
            return false;
        return boost::edge(findVertex(from), findVertex(to), graph_).second;
    }

    EdgeProperty& edgeProperty(const VertexId& from, const VertexId& to)
    {
        findEdge(from, to);
        // 无向图中边可能以相反方向存储
        if (auto it = edgeProps_.find({from, to}); it != edgeProps_.end())
            return it->second;
        return edgeProps_.at({to, from});
    }

    const EdgeProperty& edgeProperty(const VertexId& from, const VertexId& to) const
    {
        findEdge(from, to);
        if (auto it = edgeProps_.find({from, to}); it != edgeProps_.end())
            return it->second;
        return edgeProps_.at({to, from});
    }

    WeightType weight(const VertexId& from, const VertexId& to) const
    {
        auto e = findEdge(from, to);
        return boost::get(boost::edge_weight, graph_)[e];
    }

    WeightType weight(EdgeDescriptor e) const { return boost::get(boost::edge_weight, graph_)[e]; }

    void setWeight(const VertexId& from, const VertexId& to, WeightType w)
    {
        auto e = findEdge(from, to);
        boost::get(boost::edge_weight, graph_)[e] = w;
    }

    std::vector<VertexId> neighbors(const VertexId& id) const
    {
        std::vector<VertexId> result;
        auto v = findVertex(id);
        for (auto [ei, ei_end] = boost::out_edges(v, graph_); ei != ei_end; ++ei)
        {
            result.push_back(vertexToId_.at(boost::target(*ei, graph_)));
        }
        return result;
    }

    std::vector<VertexId> predecessors(const VertexId& id) const requires concepts::DirectedGraphTag<DirectionTag>
    {
        std::vector<VertexId> result;
        auto v = findVertex(id);
        for (auto [ei, ei_end] = boost::in_edges(v, graph_); ei != ei_end; ++ei)
        {
            result.push_back(vertexToId_.at(boost::source(*ei, graph_)));
        }
        return result;
    }

    std::vector<VertexId> allVertices() const
    {
        std::vector<VertexId> result;
        for (auto [vi, vi_end] = boost::vertices(graph_); vi != vi_end; ++vi)
        {
            result.push_back(vertexToId_.at(*vi));
        }
        return result;
    }

    std::vector<std::pair<VertexId, VertexId>> allEdges() const
    {
        std::vector<std::pair<VertexId, VertexId>> result;
        for (auto [ei, ei_end] = boost::edges(graph_); ei != ei_end; ++ei)
        {
            auto u = boost::source(*ei, graph_);
            auto v = boost::target(*ei, graph_);
            result.push_back({vertexToId_.at(u), vertexToId_.at(v)});
        }
        return result;
    }

    std::size_t vertexCount() const { return boost::num_vertices(graph_); }
    std::size_t edgeCount() const { return boost::num_edges(graph_); }

    const GraphType& internalGraph() const { return graph_; }
    GraphType& internalGraph() { return graph_; }

    VertexDescriptor findVertex(const VertexId& id) const
    {
        auto it = idToVertex_.find(id);
        if (it == idToVertex_.end())
        {
            throw RuntimeError(std::format("Vertex not found: {}", detail::toString(id)));
        }
        return it->second;
    }

    const VertexId& vertexId(VertexDescriptor v) const { return vertexToId_.at(v); }

    const auto& idToVertexMap() const { return idToVertex_; }
    const auto& vertexToIdMap() const { return vertexToId_; }

protected:
    GraphType graph_;
    std::unordered_map<VertexId, VertexDescriptor> idToVertex_;
    std::unordered_map<VertexDescriptor, VertexId> vertexToId_;
    std::unordered_map<std::pair<VertexId, VertexId>, EdgeProperty, boost::hash<std::pair<VertexId, VertexId>>>
        edgeProps_;
    detail::VertexDescStorage<VertexDescription, VertexId> vertexDescriptions_;

    EdgeDescriptor findEdge(const VertexId& from, const VertexId& to) const
    {
        auto e = boost::edge(findVertex(from), findVertex(to), graph_);
        if (!e.second)
        {
            throw RuntimeError(std::format("Edge not found: {} -> {}", detail::toString(from), detail::toString(to)));
        }
        return e.first;
    }
};

template <concepts::VertexIdType V, typename VP = boost::no_property, typename EP = boost::no_property,
          typename W = double, typename VD = void>
class DirectedGraph : public BaseGraph<V, VP, EP, W, boost::bidirectionalS, VD>
{
public:
    using Base = BaseGraph<V, VP, EP, W, boost::bidirectionalS, VD>;
    using Base::Base;
    std::vector<V> inNeighbors(const V& id) const { return Base::predecessors(id); }
};

template <concepts::VertexIdType V, typename VP = boost::no_property, typename EP = boost::no_property,
          typename W = double, typename VD = void>
class UndirectedGraph : public BaseGraph<V, VP, EP, W, boost::undirectedS, VD>
{
public:
    using Base = BaseGraph<V, VP, EP, W, boost::undirectedS, VD>;
    using Base::Base;
};

}  // namespace graph


namespace graph::algorithm
{

using namespace graph::concepts;

template <typename Graph>
requires TSPWeight<typename Graph::WeightType>
    std::pair<std::unordered_map<typename Graph::VertexDescriptor, typename Graph::WeightType>,
              std::unordered_map<typename Graph::VertexDescriptor, typename Graph::VertexDescriptor>>
    genericDijkstra(const Graph& g, const std::vector<typename Graph::VertexDescriptor>& sources)
{
    using VD = typename Graph::VertexDescriptor;
    using W = typename Graph::WeightType;
    using Node = std::pair<W, VD>;

    struct Cmp
    {
        bool operator()(const Node& a, const Node& b) const { return b.first < a.first; }
    };

    std::unordered_map<VD, W> dist;
    std::unordered_map<VD, VD> pred;
    std::priority_queue<Node, std::vector<Node>, Cmp> pq;

    for (auto s : sources)
    {
        dist[s] = W{};
        pq.push({W{}, s});
    }

    while (!pq.empty())
    {
        auto [d, u] = pq.top();
        pq.pop();
        auto it = dist.find(u);
        if (it == dist.end())
            continue;
        if (it->second < d)
            continue;

        for (auto [ei, ei_end] = boost::out_edges(u, g.internalGraph()); ei != ei_end; ++ei)
        {
            auto v = boost::target(*ei, g.internalGraph());
            W w = boost::get(boost::edge_weight, g.internalGraph())[*ei];
            W nd = d + w;
            auto vit = dist.find(v);
            if (vit == dist.end() || nd < vit->second)
            {
                dist[v] = nd;
                pred[v] = u;
                pq.push({nd, v});
            }
        }
    }
    return {dist, pred};
}

template <typename Graph>
requires TSPWeight<typename Graph::WeightType> std::vector<typename Graph::VertexDescriptor>
genericDijkstraPath(const Graph& g, const std::vector<typename Graph::VertexDescriptor>& sources,
                    const std::vector<typename Graph::VertexDescriptor>& targets,
                    typename Graph::WeightType* outDist = nullptr)
{
    using VD = typename Graph::VertexDescriptor;
    auto [dist, pred] = genericDijkstra(g, sources);

    VD bestTarget;
    bool found = false;
    typename Graph::WeightType bestDist;
    for (auto t : targets)
    {
        auto it = dist.find(t);
        if (it != dist.end())
        {
            if (!found || it->second < bestDist)
            {
                found = true;
                bestDist = it->second;
                bestTarget = t;
            }
        }
    }
    if (!found)
        return {};
    if (outDist)
        *outDist = bestDist;

    std::vector<VD> path;
    for (auto at = bestTarget;; at = pred[at])
    {
        path.push_back(at);
        if (!pred.contains(at))
            break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// BFS
template <typename Graph>
std::vector<typename Graph::VertexId> bfs(const Graph& g, const typename Graph::VertexId& start,
                                          std::function<void(const typename Graph::VertexId&)> onDiscover = nullptr)
{
    using Id = typename Graph::VertexId;
    using VD = typename Graph::VertexDescriptor;
    std::vector<Id> order;
    std::vector<boost::default_color_type> colors(boost::num_vertices(g.internalGraph()));
    struct Visitor : boost::default_bfs_visitor
    {
        std::vector<Id>* order;
        std::function<void(const Id&)> cb;
        const Graph* graph;
        Visitor(std::vector<Id>* order, std::function<void(const Id&)> cb, const Graph* graph)
            : order(order), cb(std::move(cb)), graph(graph)
        {
        }
        void discover_vertex(VD v, const typename Graph::GraphType&) const
        {
            auto id = graph->vertexId(v);
            order->push_back(id);
            if (cb)
                cb(id);
        }
    };
    Visitor vis{&order, std::move(onDiscover), &g};
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    boost::breadth_first_search(
        g.internalGraph(), g.findVertex(start),
        boost::visitor(vis).color_map(boost::make_iterator_property_map(colors.begin(), indexMap)));
    return order;
}

// DFS
template <typename Graph>
std::vector<typename Graph::VertexId> dfs(const Graph& g, const typename Graph::VertexId& start,
                                          std::function<void(const typename Graph::VertexId&)> onDiscover = nullptr)
{
    using Id = typename Graph::VertexId;
    using VD = typename Graph::VertexDescriptor;
    std::vector<Id> order;
    std::vector<boost::default_color_type> colors(boost::num_vertices(g.internalGraph()));
    struct Visitor : boost::default_dfs_visitor
    {
        std::vector<Id>* order;
        std::function<void(const Id&)> cb;
        const Graph* graph;
        Visitor(std::vector<Id>* order, std::function<void(const Id&)> cb, const Graph* graph)
            : order(order), cb(std::move(cb)), graph(graph)
        {
        }
        void discover_vertex(VD v, const typename Graph::GraphType&) const
        {
            auto id = graph->vertexId(v);
            order->push_back(id);
            if (cb)
                cb(id);
        }
    };
    Visitor vis{&order, std::move(onDiscover), &g};
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    boost::depth_first_visit(g.internalGraph(), g.findVertex(start), vis,
                             boost::make_iterator_property_map(colors.begin(), indexMap));
    return order;
}

// Dijkstra (Boost, arithmetic only)
template <typename Graph>
requires ArithmeticWeight<typename Graph::WeightType>
    std::pair<std::unordered_map<typename Graph::VertexId, typename Graph::WeightType>,
              std::unordered_map<typename Graph::VertexId, typename Graph::VertexId>>
    dijkstraWithPath(const Graph& g, const typename Graph::VertexId& source)
{
    using W = typename Graph::WeightType;
    using Id = typename Graph::VertexId;
    using VD = typename Graph::VertexDescriptor;
    auto n = boost::num_vertices(g.internalGraph());
    std::vector<W> dist(n);
    std::vector<VD> pred(n);
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    boost::dijkstra_shortest_paths(g.internalGraph(), g.findVertex(source),
                                   boost::distance_map(boost::make_iterator_property_map(dist.begin(), indexMap))
                                       .predecessor_map(boost::make_iterator_property_map(pred.begin(), indexMap)));
    std::unordered_map<Id, W> distMap;
    std::unordered_map<Id, Id> predMap;
    for (auto [vi, vi_end] = boost::vertices(g.internalGraph()); vi != vi_end; ++vi)
    {
        auto id = g.vertexId(*vi);
        distMap[id] = dist[*vi];
        if (pred[*vi] != *vi)
            predMap[id] = g.vertexId(pred[*vi]);
    }
    return {distMap, predMap};
}

// Bellman-Ford
template <typename Graph>
requires ArithmeticWeight<typename Graph::WeightType>
    std::optional<std::unordered_map<typename Graph::VertexId, typename Graph::WeightType>>
    bellmanFord(const Graph& g, const typename Graph::VertexId& source)
{
    using W = typename Graph::WeightType;
    using Id = typename Graph::VertexId;
    auto n = boost::num_vertices(g.internalGraph());
    std::vector<W> dist(n);
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    bool ok = boost::bellman_ford_shortest_paths(
        g.internalGraph(), n,
        boost::distance_map(boost::make_iterator_property_map(dist.begin(), indexMap))
            .weight_map(boost::get(boost::edge_weight, g.internalGraph()))
            .root_vertex(g.findVertex(source)));
    if (!ok)
        return std::nullopt;
    std::unordered_map<Id, W> result;
    for (auto [vi, vi_end] = boost::vertices(g.internalGraph()); vi != vi_end; ++vi)
        result[g.vertexId(*vi)] = dist[*vi];
    return result;
}

// A* (Boost)
struct AStarFoundGoal : public std::exception
{
};

template <typename Graph, typename HeuristicFunc>
requires ArithmeticWeight<typename Graph::WeightType> std::vector<typename Graph::VertexId>
astar(const Graph& g, const typename Graph::VertexId& start, const typename Graph::VertexId& goal, HeuristicFunc h)
{
    using Id = typename Graph::VertexId;
    using VD = typename Graph::VertexDescriptor;
    using W = typename Graph::WeightType;
    using GraphType = typename Graph::GraphType;
    auto n = boost::num_vertices(g.internalGraph());
    std::vector<W> dist(n);
    std::vector<VD> pred(n);
    std::vector<boost::default_color_type> colors(n);
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    struct Heur : boost::astar_heuristic<GraphType, W>
    {
        HeuristicFunc f;
        const Graph* g;
        Heur(HeuristicFunc f, const Graph* g) : f(std::move(f)), g(g) {}
        W operator()(VD v) const { return f(g->vertexId(v)); }
    };
    struct Vis : boost::default_astar_visitor
    {
        VD goal;
        explicit Vis(VD goal) : goal(goal) {}
        void examine_vertex(VD v, const GraphType&)
        {
            if (v == goal)
                throw AStarFoundGoal{};
        }
    };
    Heur heur{std::move(h), &g};
    Vis vis{g.findVertex(goal)};
    try
    {
        boost::astar_search(g.internalGraph(), g.findVertex(start), heur,
                            boost::predecessor_map(boost::make_iterator_property_map(pred.begin(), indexMap))
                                .distance_map(boost::make_iterator_property_map(dist.begin(), indexMap))
                                .color_map(boost::make_iterator_property_map(colors.begin(), indexMap))
                                .visitor(vis));
    }
    catch (const AStarFoundGoal&)
    {
        std::vector<Id> path;
        VD cur = g.findVertex(goal), src = g.findVertex(start);
        while (cur != src)
        {
            path.push_back(g.vertexId(cur));
            cur = pred[cur];
        }
        path.push_back(start);
        std::reverse(path.begin(), path.end());
        return path;
    }
    return {};
}

// Prim (undirected)
template <typename Graph>
requires ArithmeticWeight<typename Graph::WeightType>&& UndirectedGraphTag<typename Graph::DirectionTagType>
    std::vector<std::pair<typename Graph::VertexId, typename Graph::VertexId>>
    prim(const Graph& g, const typename Graph::VertexId& start)
{
    using VD = typename Graph::VertexDescriptor;
    using Id = typename Graph::VertexId;
    auto n = boost::num_vertices(g.internalGraph());
    std::vector<VD> pred(n);
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    boost::prim_minimum_spanning_tree(g.internalGraph(), boost::make_iterator_property_map(pred.begin(), indexMap),
                                      boost::root_vertex(g.findVertex(start)));
    std::vector<std::pair<Id, Id>> mst;
    for (auto [vi, vi_end] = boost::vertices(g.internalGraph()); vi != vi_end; ++vi)
        if (pred[*vi] != *vi)
            mst.push_back({g.vertexId(pred[*vi]), g.vertexId(*vi)});
    return mst;
}

// Kruskal (undirected)
template <typename Graph>
requires ArithmeticWeight<typename Graph::WeightType>&& UndirectedGraphTag<typename Graph::DirectionTagType>
    std::vector<std::pair<typename Graph::VertexId, typename Graph::VertexId>> kruskal(const Graph& g)
{
    using EdgeDesc = typename Graph::EdgeDescriptor;
    using Id = typename Graph::VertexId;
    std::vector<EdgeDesc> mstEdges;
    boost::kruskal_minimum_spanning_tree(g.internalGraph(), std::back_inserter(mstEdges));
    std::vector<std::pair<Id, Id>> result;
    for (const auto& e : mstEdges)
        result.push_back(
            {g.vertexId(boost::source(e, g.internalGraph())), g.vertexId(boost::target(e, g.internalGraph()))});
    return result;
}

// Connected Components (undirected)
template <typename Graph>
requires UndirectedGraphTag<typename Graph::DirectionTagType> std::unordered_map<typename Graph::VertexId, int>
connectedComponents(const Graph& g)
{
    using Id = typename Graph::VertexId;
    auto n = boost::num_vertices(g.internalGraph());
    std::vector<int> comp(n);
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    boost::connected_components(g.internalGraph(), boost::make_iterator_property_map(comp.begin(), indexMap));
    std::unordered_map<Id, int> result;
    for (auto [vi, vi_end] = boost::vertices(g.internalGraph()); vi != vi_end; ++vi)
        result[g.vertexId(*vi)] = comp[*vi];
    return result;
}

// Strong Components (directed)
template <typename Graph>
requires DirectedGraphTag<typename Graph::DirectionTagType> std::unordered_map<typename Graph::VertexId, int>
strongComponents(const Graph& g)
{
    using Id = typename Graph::VertexId;
    using VD = typename Graph::VertexDescriptor;
    auto n = boost::num_vertices(g.internalGraph());
    std::vector<int> comp(n);
    std::vector<boost::default_color_type> colors(n);
    std::vector<VD> root(n);
    auto indexMap = boost::get(boost::vertex_index, g.internalGraph());
    boost::strong_components(g.internalGraph(), boost::make_iterator_property_map(comp.begin(), indexMap),
                             boost::color_map(boost::make_iterator_property_map(colors.begin(), indexMap))
                                 .root_map(boost::make_iterator_property_map(root.begin(), indexMap)));
    std::unordered_map<Id, int> result;
    for (auto [vi, vi_end] = boost::vertices(g.internalGraph()); vi != vi_end; ++vi)
        result[g.vertexId(*vi)] = comp[*vi];
    return result;
}

// Topological Sort (directed)
template <typename Graph>
requires DirectedGraphTag<typename Graph::DirectionTagType> std::vector<typename Graph::VertexId>
topologicalSort(const Graph& g)
{
    using Id = typename Graph::VertexId;
    using VD = typename Graph::VertexDescriptor;
    std::vector<VD> order;
    boost::topological_sort(g.internalGraph(), std::back_inserter(order));
    std::vector<Id> result;
    for (auto it = order.rbegin(); it != order.rend(); ++it)
        result.push_back(g.vertexId(*it));
    return result;
}

// Max Flow (directed)
template <typename Graph>
requires DirectedGraphTag<typename Graph::DirectionTagType>&&
    ArithmeticWeight<typename Graph::WeightType> typename Graph::WeightType
    maxFlow(const Graph& g, const typename Graph::VertexId& source, const typename Graph::VertexId& sink)
{
    using W = typename Graph::WeightType;
    using FlowEdgeProp = boost::property<
        boost::edge_capacity_t, W,
        boost::property<
            boost::edge_residual_capacity_t, W,
            boost::property<boost::edge_reverse_t, typename boost::graph_traits<boost::adjacency_list<
                                                       boost::vecS, boost::vecS, boost::directedS, boost::no_property,
                                                       boost::property<boost::edge_capacity_t, W>>>::edge_descriptor>>>;
    using FlowGraph =
        boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, boost::no_property, FlowEdgeProp>;
    using FVertex = typename boost::graph_traits<FlowGraph>::vertex_descriptor;
    using FEdge = typename boost::graph_traits<FlowGraph>::edge_descriptor;

    FlowGraph fg(boost::num_vertices(g.internalGraph()));
    for (auto [ei, ei_end] = boost::edges(g.internalGraph()); ei != ei_end; ++ei)
    {
        auto u = boost::source(*ei, g.internalGraph());
        auto v = boost::target(*ei, g.internalGraph());
        W cap = g.weight(*ei);
        auto [e1, ok1] = boost::add_edge(u, v, fg);
        auto [e2, ok2] = boost::add_edge(v, u, fg);
        boost::put(boost::edge_capacity, fg, e1, cap);
        boost::put(boost::edge_capacity, fg, e2, W{});
        boost::put(boost::edge_reverse, fg, e1, e2);
        boost::put(boost::edge_reverse, fg, e2, e1);
    }
    // 顶点属性为 no_property，需显式提供前驱/颜色/距离映射；
    // 前驱映射用空边描述符初始化（BK 通过内部 has_parent 标记守护前驱读取，
    // 源/汇在树中总有父边，不会读到空值）
    auto n = boost::num_vertices(fg);
    std::vector<FEdge> pred(n, FEdge{});
    std::vector<boost::default_color_type> color(n);
    std::vector<std::size_t> distance(n);
    auto indexMap = boost::get(boost::vertex_index, fg);
    return boost::boykov_kolmogorov_max_flow(
        fg, g.findVertex(source), g.findVertex(sink),
        boost::predecessor_map(boost::make_iterator_property_map(pred.begin(), indexMap))
            .vertex_color_map(boost::make_iterator_property_map(color.begin(), indexMap))
            .distance_map(boost::make_iterator_property_map(distance.begin(), indexMap)));
}


template <typename Graph>
requires TSPWeight<typename Graph::WeightType> class GeneticTSP
{
public:
    using Id = typename Graph::VertexId;
    using W = typename Graph::WeightType;

    struct Result
    {
        std::vector<Id> tour;
        W totalCost;
        std::size_t generations;
    };

    GeneticTSP(const Graph& graph, std::size_t popSize = 100, std::size_t maxGen = 1000, double mutRate = 0.02,
               double cxRate = 0.8)
        : graph_(graph), popSize_(popSize), maxGen_(maxGen), mutRate_(mutRate), cxRate_(cxRate)
    {
    }

    Result solve(const std::vector<Id>& mustVisit)
    {
        if (mustVisit.size() < 2)
            throw RuntimeError("TSP needs >= 2 cities");
        cities_ = mustVisit;
        n_ = cities_.size();
        dist_.assign(n_, std::vector<std::optional<W>>(n_, std::nullopt));
        for (std::size_t i = 0; i < n_; ++i)
        {
            for (std::size_t j = 0; j < n_; ++j)
            {
                if (i == j)
                {
                    dist_[i][j] = W{};
                    continue;
                }
                if (graph_.hasEdge(cities_[i], cities_[j]))
                    dist_[i][j] = graph_.weight(cities_[i], cities_[j]);
            }
        }
        initPopulation();
        Result bestResult{{}, W{}, 0};
        bool first = true;
        for (std::size_t gen = 0; gen < maxGen_; ++gen)
        {
            evaluate();
            W currentCost = tourCost(population_[bestIdx_]);
            if (first || currentCost < bestResult.totalCost)
            {
                first = false;
                bestResult.totalCost = currentCost;
                bestResult.tour = decode(population_[bestIdx_]);
                bestResult.generations = gen + 1;
            }
            std::vector<std::vector<std::size_t>> newPop;
            newPop.push_back(population_[bestIdx_]);
            while (newPop.size() < popSize_)
            {
                auto p1 = tournamentSelect();
                auto p2 = tournamentSelect();
                auto [c1, c2] = crossover(population_[p1], population_[p2]);
                mutate(c1);
                mutate(c2);
                newPop.push_back(std::move(c1));
                if (newPop.size() < popSize_)
                    newPop.push_back(std::move(c2));
            }
            population_ = std::move(newPop);
        }
        evaluate();
        W finalCost = tourCost(population_[bestIdx_]);
        if (first || finalCost < bestResult.totalCost)
        {
            bestResult.totalCost = finalCost;
            bestResult.tour = decode(population_[bestIdx_]);
            bestResult.generations = maxGen_;
        }
        return bestResult;
    }

private:
    void initPopulation()
    {
        population_.clear();
        std::vector<std::size_t> base(n_);
        std::iota(base.begin(), base.end(), 0);
        std::random_device rd;
        std::mt19937 gen(rd());
        for (std::size_t i = 0; i < popSize_; ++i)
        {
            auto p = base;
            std::shuffle(p.begin(), p.end(), gen);
            population_.push_back(std::move(p));
        }
    }
    void evaluate()
    {
        fitness_.resize(popSize_);
        bestIdx_ = 0;
        W bestCost{};
        bool first = true;
        for (std::size_t i = 0; i < popSize_; ++i)
        {
            W c = tourCost(population_[i]);
            if (first || c < bestCost)
            {
                first = false;
                bestCost = c;
                bestIdx_ = i;
            }
        }
    }
    W tourCost(const std::vector<std::size_t>& perm) const
    {
        W total{};
        for (std::size_t i = 0; i < perm.size(); ++i)
        {
            std::size_t from = perm[i], to = perm[(i + 1) % perm.size()];
            if (!dist_[from][to].has_value())
            {
                W bad = total;
                for (std::size_t k = 0; k < 1000; ++k)
                    bad = bad + total + W{} + W{};
                return bad;
            }
            total = total + dist_[from][to].value();
        }
        return total;
    }
    std::vector<Id> decode(const std::vector<std::size_t>& perm) const
    {
        std::vector<Id> tour;
        for (auto idx : perm)
            tour.push_back(cities_[idx]);
        return tour;
    }
    std::size_t tournamentSelect(std::size_t tsize = 3)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> dist(0, popSize_ - 1);
        std::size_t best = dist(gen);
        W bestCost = tourCost(population_[best]);
        for (std::size_t i = 1; i < tsize; ++i)
        {
            std::size_t c = dist(gen);
            W cc = tourCost(population_[c]);
            if (cc < bestCost)
            {
                bestCost = cc;
                best = c;
            }
        }
        return best;
    }
    std::pair<std::vector<std::size_t>, std::vector<std::size_t>> crossover(const std::vector<std::size_t>& p1,
                                                                            const std::vector<std::size_t>& p2)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        if (prob(gen) > cxRate_)
            return {p1, p2};
        std::uniform_int_distribution<std::size_t> dist(0, n_ - 1);
        std::size_t a = dist(gen), b = dist(gen);
        if (a > b)
            std::swap(a, b);
        auto makeChild = [&](const std::vector<std::size_t>& pa, const std::vector<std::size_t>& pb)
        {
            std::vector<std::size_t> child(n_, static_cast<std::size_t>(-1));
            std::vector<bool> used(n_, false);
            for (std::size_t i = a; i <= b; ++i)
            {
                child[i] = pa[i];
                used[pa[i]] = true;
            }
            std::size_t idx = (b + 1) % n_;
            for (std::size_t i = 0; i < n_; ++i)
            {
                std::size_t pos = (b + 1 + i) % n_;
                if (!used[pb[pos]])
                {
                    child[idx] = pb[pos];
                    used[pb[pos]] = true;
                    idx = (idx + 1) % n_;
                }
            }
            return child;
        };
        return {makeChild(p1, p2), makeChild(p2, p1)};
    }
    void mutate(std::vector<std::size_t>& perm)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> prob(0.0, 1.0);
        std::uniform_int_distribution<std::size_t> dist(0, n_ - 1);
        for (std::size_t i = 0; i < n_; ++i)
            if (prob(gen) < mutRate_)
                std::swap(perm[i], perm[dist(gen)]);
    }

    const Graph& graph_;
    std::size_t popSize_, maxGen_, n_;
    double mutRate_, cxRate_;
    std::vector<Id> cities_;
    std::vector<std::vector<std::optional<W>>> dist_;
    std::vector<std::vector<std::size_t>> population_;
    std::vector<W> fitness_;
    std::size_t bestIdx_;
};
}  // namespace graph::algorithm
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_GRAPH_HPP