#ifndef HSBA_SLICER_AREAGRAPH_HPP
#define HSBA_SLICER_AREAGRAPH_HPP
#pragma once

#include "Graph.hpp"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/container_hash/hash.hpp>

#include <algorithm>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HsBa::Slicer
{
namespace graph
{

template <typename GateId, typename Weight>
struct GateInfo
{
    GateId id;
    bool canEnter = true;
    bool canExit = true; 
};

template <typename GateId, typename Weight>
struct AreaConfig
{
    std::vector<GateInfo<GateId, Weight>> gates;

    bool allowSameGateInOut = true;

    Weight sameGateInternalCost = Weight{};

    std::map<std::pair<GateId, GateId>, Weight> internalCosts;
};

template <typename AreaId, typename GateId, typename Weight>
struct AreaPathResult
{
    std::vector<AreaId> areaPath;
    std::vector<GateId> entryGates;
    std::vector<GateId> exitGates;
    Weight totalCost;

    bool empty() const { return areaPath.empty(); }
    std::size_t size() const { return areaPath.size(); }

    friend std::ostream& operator<<(std::ostream& os, const AreaPathResult& r)
    {
        os << "AreaPathResult{cost=" << detail::toString(r.totalCost) << ", path=[";
        for (std::size_t i = 0; i < r.areaPath.size(); ++i)
        {
            os << r.areaPath[i] << "(in:" << r.entryGates[i] << ",out:" << r.exitGates[i] << ")";
            if (i + 1 < r.areaPath.size())
                os << " -> ";
        }
        os << "]}";
        return os;
    }
};

template <typename AreaId, typename GateId, typename Weight>
struct AreaTSPResult
{
    std::vector<AreaId> tour;
    std::vector<std::vector<GateId>> entryGates;
    std::vector<std::vector<GateId>> exitGates;
    Weight totalCost;
    std::size_t generations;
};


template <concepts::VertexIdType AreaId, concepts::VertexIdType GateId, typename Weight,
          typename AreaProperty = boost::no_property, typename RouteProperty = boost::no_property,
          typename AreaDescription = void>
requires concepts::TSPWeight<Weight> class AreaGraph
{
public:
    using Config = AreaConfig<GateId, Weight>;
    using PathResult = AreaPathResult<AreaId, GateId, Weight>;
    using TSPResult = AreaTSPResult<AreaId, GateId, Weight>;
    using AreaDescriptionType = AreaDescription;

    void addArea(const AreaId& id, const Config& config, const AreaProperty& prop = {})
    {
        areas_[id] = InternalArea{config, prop};
        dirty_ = true;
    }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) void addArea(const AreaId& id, const Config& config, const AreaProperty& prop,
                                                     const AD& desc)
    {
        areas_[id] = InternalArea{config, prop};
        areaDescriptions_.set(id, desc);
        dirty_ = true;
    }

    bool hasArea(const AreaId& id) const { return areas_.contains(id); }

    AreaProperty& areaProperty(const AreaId& id) { return areas_.at(id).property; }
    const AreaProperty& areaProperty(const AreaId& id) const { return areas_.at(id).property; }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) void setAreaDescription(const AreaId& id, const AD& desc)
    {
        if (!hasArea(id))
            throw RuntimeError(std::format("Area not found: {}", detail::toString(id)));
        areaDescriptions_.set(id, desc);
    }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) AD& areaDescription(const AreaId& id)
    {
        if (!hasArea(id))
            throw RuntimeError(std::format("Area not found: {}", detail::toString(id)));
        return areaDescriptions_.get(id);
    }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) const AD& areaDescription(const AreaId& id) const
    {
        if (!hasArea(id))
            throw RuntimeError(std::format("Area not found: {}", detail::toString(id)));
        return areaDescriptions_.get(id);
    }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) bool hasAreaDescription(const AreaId& id) const
    {
        return areaDescriptions_.has(id);
    }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) void removeAreaDescription(const AreaId& id)
    {
        areaDescriptions_.erase(id);
    }

    template <typename AD = AreaDescription>
    requires(!std::is_same_v<AD, void>) const auto& areaDescriptions() const
    {
        return areaDescriptions_.data;
    }

    void addRoute(const AreaId& from, const AreaId& to, const std::map<std::pair<GateId, GateId>, Weight>& gateWeights,
                  const RouteProperty& prop = {})
    {
        routes_[{from, to}] = InternalRoute{gateWeights, prop};
        dirty_ = true;
    }

    PathResult shortestPath(const AreaId& from, const AreaId& to)
    {
        ensureExpanded();

        // 起点区域视为已在内部：从各出口出发，初始代价为"入口->该出口"的最小内部代价
        // （起点不受 allowSameGateInOut 限制；无入口时直接以零代价出发）
        const auto& fromCfg = areas_.at(from).config;
        bool hasEntry = false;
        for (const auto& g : fromCfg.gates)
        {
            if (g.canEnter)
            {
                hasEntry = true;
                break;
            }
        }

        std::vector<std::pair<ExpandedVD, Weight>> sources;
        for (const auto& gOut : fromCfg.gates)
        {
            if (!gOut.canExit)
                continue;
            Weight startCost{};
            if (hasEntry)
            {
                bool found = false;
                for (const auto& gIn : fromCfg.gates)
                {
                    if (!gIn.canEnter)
                        continue;
                    Weight w = fromCfg.sameGateInternalCost;
                    auto it = fromCfg.internalCosts.find({gIn.id, gOut.id});
                    if (it != fromCfg.internalCosts.end())
                        w = it->second;
                    if (!found || w < startCost)
                    {
                        found = true;
                        startCost = w;
                    }
                }
            }
            sources.push_back({findExpandedVertex({from, gOut.id, true}), startCost});
        }
        if (sources.empty())
            throw RuntimeError("Source area has no exit gates");

        // 终点区域到达入口即结束，无需再穿越其内部
        std::vector<ExpandedVD> targets;
        for (const auto& g : areas_.at(to).config.gates)
        {
            if (g.canEnter)
                targets.push_back(findExpandedVertex({to, g.id, false}));
        }
        if (targets.empty())
            throw RuntimeError("Target area has no entry gates");

        return solveShortestPath(sources, targets);
    }

    PathResult shortestPath(const AreaId& from, const GateId& fromExit, const AreaId& to, const GateId& toEntry)
    {
        ensureExpanded();
        auto src = findExpandedVertex({from, fromExit, true});
        auto tgt = findExpandedVertex({to, toEntry, false});
        return solveShortestPath({{src, Weight{}}}, {tgt});
    }

    TSPResult solveTSP(const std::vector<AreaId>& mustVisit, std::size_t populationSize = 150,
                       std::size_t maxGenerations = 500, double mutationRate = 0.03, double crossoverRate = 0.85)
    {
        if (mustVisit.size() < 2)
            throw RuntimeError("TSP needs >= 2 areas");

        const std::size_t n = mustVisit.size();

        std::vector<std::vector<std::optional<Weight>>> dist(n, std::vector<std::optional<Weight>>(n));
        std::vector<std::vector<PathResult>> pathCache(n, std::vector<PathResult>(n));

        for (std::size_t i = 0; i < n; ++i)
        {
            for (std::size_t j = 0; j < n; ++j)
            {
                if (i == j)
                {
                    dist[i][j] = Weight{};
                    continue;
                }
                auto path = shortestPath(mustVisit[i], mustVisit[j]);
                if (!path.empty())
                {
                    dist[i][j] = path.totalCost;
                    pathCache[i][j] = std::move(path);
                }
            }
        }

        DirectedGraph<AreaId, AreaProperty, RouteProperty, Weight> compressed;
        for (const auto& id : mustVisit)
            compressed.addVertex(id);
        for (std::size_t i = 0; i < n; ++i)
        {
            for (std::size_t j = 0; j < n; ++j)
            {
                if (i != j && dist[i][j].has_value())
                {
                    compressed.addEdge(mustVisit[i], mustVisit[j], dist[i][j].value());
                }
            }
        }

        algorithm::GeneticTSP<decltype(compressed)> solver(compressed, populationSize, maxGenerations, mutationRate,
                                                           crossoverRate);
        auto gaResult = solver.solve(mustVisit);

        TSPResult result;
        result.tour = gaResult.tour;
        result.totalCost = gaResult.totalCost;
        result.generations = gaResult.generations;
        result.entryGates.resize(result.tour.size());
        result.exitGates.resize(result.tour.size());

        for (std::size_t i = 0; i < result.tour.size(); ++i)
        {
            const auto& from = result.tour[i];
            const auto& to = result.tour[(i + 1) % result.tour.size()];

            std::size_t fromIdx = std::distance(mustVisit.begin(), std::find(mustVisit.begin(), mustVisit.end(), from));
            std::size_t toIdx = std::distance(mustVisit.begin(), std::find(mustVisit.begin(), mustVisit.end(), to));

            const auto& path = pathCache[fromIdx][toIdx];

            if (!path.empty())
            {
                auto fromPos =
                    std::distance(path.areaPath.begin(), std::find(path.areaPath.begin(), path.areaPath.end(), from));
                auto toPos =
                    std::distance(path.areaPath.begin(), std::find(path.areaPath.begin(), path.areaPath.end(), to));

                result.exitGates[i].push_back(path.exitGates[fromPos]);
                result.entryGates[(i + 1) % result.tour.size()].push_back(path.entryGates[toPos]);
            }
        }

        return result;
    }

    std::vector<AreaId> allAreas() const
    {
        std::vector<AreaId> result;
        for (const auto& [id, _] : areas_)
            result.push_back(id);
        return result;
    }

    std::size_t areaCount() const { return areas_.size(); }

private:
    struct InternalArea
    {
        Config config;
        AreaProperty property;
    };

    struct InternalRoute
    {
        std::map<std::pair<GateId, GateId>, Weight> gateWeights;
        RouteProperty property;
    };

    struct ExpandedVertex
    {
        AreaId areaId;
        GateId gateId;
        bool isExit;  // false = entry, true = exit
        bool operator==(const ExpandedVertex& o) const
        {
            return areaId == o.areaId && gateId == o.gateId && isExit == o.isExit;
        }
    };

    struct ExpandedVertexHash
    {
        std::size_t operator()(const ExpandedVertex& v) const
        {
            std::size_t h1 = std::hash<AreaId>{}(v.areaId);
            std::size_t h2 = std::hash<GateId>{}(v.gateId);
            std::size_t h3 = std::hash<bool>{}(v.isExit);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    using ExpandedGraphType = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, ExpandedVertex,
                                                    boost::property<boost::edge_weight_t, Weight>>;
    using ExpandedVD = typename boost::graph_traits<ExpandedGraphType>::vertex_descriptor;

    std::unordered_map<AreaId, InternalArea> areas_;

    std::unordered_map<std::pair<AreaId, AreaId>, InternalRoute, boost::hash<std::pair<AreaId, AreaId>>> routes_;

    ExpandedGraphType expandedGraph_;
    std::unordered_map<ExpandedVertex, ExpandedVD, ExpandedVertexHash> expandedVertexMap_;
    bool dirty_ = true;
    detail::VertexDescStorage<AreaDescription, AreaId> areaDescriptions_;

    void ensureExpanded()
    {
        if (!dirty_)
            return;
        buildExpandedGraph();
        dirty_ = false;
    }

    void buildExpandedGraph()
    {
        expandedGraph_ = ExpandedGraphType();
        expandedVertexMap_.clear();

        for (const auto& [areaId, area] : areas_)
        {
            for (const auto& gate : area.config.gates)
            {
                if (gate.canEnter)
                    addExpandedVertex({areaId, gate.id, false});
                if (gate.canExit)
                    addExpandedVertex({areaId, gate.id, true});
            }
        }

        for (const auto& [areaId, area] : areas_)
        {
            const auto& cfg = area.config;
            for (const auto& gIn : cfg.gates)
            {
                if (!gIn.canEnter)
                    continue;
                for (const auto& gOut : cfg.gates)
                {
                    if (!gOut.canExit)
                        continue;
                    if (gIn.id == gOut.id && !cfg.allowSameGateInOut)
                        continue;

                    Weight w = cfg.sameGateInternalCost;
                    auto it = cfg.internalCosts.find({gIn.id, gOut.id});
                    if (it != cfg.internalCosts.end())
                        w = it->second;

                    auto u = expandedVertexMap_.at({areaId, gIn.id, false});
                    auto v = expandedVertexMap_.at({areaId, gOut.id, true});
                    boost::add_edge(u, v, boost::property<boost::edge_weight_t, Weight>(w), expandedGraph_);
                }
            }
        }

        for (const auto& [key, route] : routes_)
        {
            const auto& [fromArea, toArea] = key;
            for (const auto& [gatePair, w] : route.gateWeights)
            {
                const auto& [exitGate, entryGate] = gatePair;
                auto u = expandedVertexMap_.at({fromArea, exitGate, true});
                auto v = expandedVertexMap_.at({toArea, entryGate, false});
                boost::add_edge(u, v, boost::property<boost::edge_weight_t, Weight>(w), expandedGraph_);
            }
        }
    }

    ExpandedVD addExpandedVertex(const ExpandedVertex& v)
    {
        auto vd = boost::add_vertex(v, expandedGraph_);
        expandedVertexMap_[v] = vd;
        return vd;
    }

    ExpandedVD findExpandedVertex(const ExpandedVertex& v) const
    {
        auto it = expandedVertexMap_.find(v);
        if (it == expandedVertexMap_.end())
        {
            std::ostringstream oss;
            oss << "Expanded vertex not found: area=" << detail::toString(v.areaId)
                << " gate=" << detail::toString(v.gateId) << " type=" << (v.isExit ? "exit" : "entry");
            throw RuntimeError(oss.str());
        }
        return it->second;
    }

    std::pair<std::unordered_map<ExpandedVD, Weight>, std::unordered_map<ExpandedVD, ExpandedVD>>
    dijkstraOnExpanded(const std::vector<std::pair<ExpandedVD, Weight>>& sources) const
    {
        using Node = std::pair<Weight, ExpandedVD>;
        struct Cmp
        {
            bool operator()(const Node& a, const Node& b) const { return b.first < a.first; }
        };

        std::unordered_map<ExpandedVD, Weight> dist;
        std::unordered_map<ExpandedVD, ExpandedVD> pred;
        std::priority_queue<Node, std::vector<Node>, Cmp> pq;

        for (auto [s, startCost] : sources)
        {
            auto it = dist.find(s);
            if (it == dist.end() || startCost < it->second)
                dist[s] = startCost;
            pq.push({startCost, s});
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

            for (auto [ei, ei_end] = boost::out_edges(u, expandedGraph_); ei != ei_end; ++ei)
            {
                auto v = boost::target(*ei, expandedGraph_);
                Weight w = boost::get(boost::edge_weight, expandedGraph_)[*ei];
                Weight nd = d + w;
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

    PathResult solveShortestPath(const std::vector<std::pair<ExpandedVD, Weight>>& sources,
                                 const std::vector<ExpandedVD>& targets)
    {
        auto [dist, pred] = dijkstraOnExpanded(sources);

        ExpandedVD bestTarget;
        bool found = false;
        Weight bestCost{};
        for (auto t : targets)
        {
            auto it = dist.find(t);
            if (it != dist.end())
            {
                if (!found || it->second < bestCost)
                {
                    found = true;
                    bestCost = it->second;
                    bestTarget = t;
                }
            }
        }
        if (!found)
            return {};

        std::vector<ExpandedVD> expPath;
        for (auto at = bestTarget;; at = pred.at(at))
        {
            expPath.push_back(at);
            if (!pred.contains(at))
                break;
        }
        std::reverse(expPath.begin(), expPath.end());

        PathResult result;
        result.totalCost = bestCost;

        std::optional<AreaId> currentArea;
        for (auto vd : expPath)
        {
            const auto& v = expandedGraph_[vd];
            if (!currentArea.has_value() || !(v.areaId == *currentArea))
            {
                result.areaPath.push_back(v.areaId);
                result.entryGates.push_back(v.gateId);
                result.exitGates.push_back(v.gateId);
                currentArea = v.areaId;
            }
            else
            {
                result.exitGates.back() = v.gateId;
            }
        }

        return result;
    }
};

}  // namespace graph
} // namespace HsBa::Slicer
#endif // !HSBA_SLICER_AREAGRAPH_HPP