#include "path_optimizer.hpp"

/**
 * @file path_optimizer.cpp
 * @brief Implementation of the region path optimizer and its Lua bindings.
 */

#include <algorithm>
#include <lua.hpp>

#include <cmath>
#include <format>
#include <limits>
#include <unordered_map>
#include <utility>

#include "2D/LuaAdapter.hpp"
#include "2D/PolygonFill.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "base/error.hpp"
#include "utils/AreaGraph.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
namespace
{
// Penalty cost when regions are unreachable (far larger than any real air-move distance)
constexpr double UNREACHABLE_COST = 1e30;

double PointDist(const Point2D& a, const Point2D& b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

}  // namespace

// =============================================================================
// RegionPathOptimizer implementation: independent polygon regions -> AreaGraph area vertices
// Two modes (cannot be mixed):
// - Polygon mode (before fill): gates = all polygon vertices, outputs the polygon set in optimized order;
// - Fill-result mode (after fill, supports multi-point polylines): gates = head/tail endpoint of each path, outputs the
// complete fill paths.
// =============================================================================
struct RegionPathOptimizer::Impl
{
    // Region modeling: AreaGraph<int(region), int(gate), double(cost)>
    using RegionAreaGraph = graph::AreaGraph<int, int, double>;

    struct RegionData
    {
        int id = 0;
        bool polygonMode = false;  // true: the polygon itself (before fill); false: fill result (multi-point polylines)
        PolygonsD paths;
        std::vector<Point2D> gates;  // gate point cache (in index order)
        std::vector<std::pair<std::size_t, std::size_t>>
            gateOwners;  // polygon mode: gate -> (polygon index, vertex index)
    };

    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes) pimpl aggregate
    std::vector<RegionData> regions;  // in insertion order
    std::unordered_map<std::pair<int, int>, double, boost::hash<std::pair<int, int>>> manualRoutes;
    std::vector<int> tour;                    // optimizeOrder result
    std::unordered_map<int, int> entryGates;  // region id -> entry gate solved by TSP
    bool optimized = false;
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

    void addRegion(int regionId, const PolygonsD& paths)
    {
        checkDuplicate(regionId);
        if (modeSet && polygonMode)
        {
            throw RuntimeError("Cannot mix path regions with polygon regions in one optimizer");
        }
        modeSet = true;
        polygonMode = false;

        RegionData r;
        r.id = regionId;
        r.paths = paths;
        // gates: path i's head point = 2i, tail point = 2i+1
        for (const auto& p : paths)
        {
            if (p.empty())
            {
                continue;
            }
            r.gates.push_back(p.front());
            r.gates.push_back(p.back());
        }
        regions.push_back(std::move(r));
        optimized = false;
    }

    void addPolygonRegion(int regionId, const PolygonsD& polygons)
    {
        checkDuplicate(regionId);
        if (modeSet && !polygonMode)
        {
            throw RuntimeError("Cannot mix polygon regions with path regions in one optimizer");
        }
        modeSet = true;
        polygonMode = true;

        RegionData r;
        r.id = regionId;
        r.polygonMode = true;
        r.paths = polygons;
        // gates: all polygon vertices, indexed in (polygon index, vertex index) order
        for (std::size_t pi = 0; pi < polygons.size(); ++pi)
        {
            for (std::size_t vi = 0; vi < polygons[pi].size(); ++vi)
            {
                r.gates.push_back(polygons[pi][vi]);
                r.gateOwners.emplace_back(pi, vi);
            }
        }
        regions.push_back(std::move(r));
        optimized = false;
    }

    void addRoute(int fromId, int toId, double cost)
    {
        manualRoutes[{fromId, toId}] = cost;
        manualRoutes[{toId, fromId}] = cost;
        optimized = false;
    }

    std::vector<int> optimizeOrder()
    {
        tour.clear();
        entryGates.clear();

        const std::size_t n = regions.size();
        for (const auto& r : regions)
        {
            tour.push_back(r.id);
        }

        if (n < 2)
        {
            optimized = true;
            return tour;
        }

        RegionAreaGraph ag;
        buildAreaGraph(ag);

        std::vector<int> mustVisit;
        mustVisit.reserve(n);
        for (const auto& r : regions)
        {
            mustVisit.push_back(r.id);
        }

        if (n == 2)
        {
            // Genetic algorithm is meaningless for two vertices, compare both directions directly
            auto ab = ag.shortestPath(mustVisit[0], mustVisit[1]);
            if (!ab.empty())
            {
                entryGates[mustVisit[1]] = ab.entryGates.back();
            }
            optimized = true;
            return tour;
        }

        auto tsp = ag.solveTSP(mustVisit);
        if (tsp.tour.size() != n)
        {
            // Keep insertion order when TSP fails
            optimized = true;
            return tour;
        }
        tour = tsp.tour;
        for (std::size_t i = 0; i < tsp.tour.size(); ++i)
        {
            if (!tsp.entryGates[i].empty())
            {
                entryGates[tsp.tour[i]] = tsp.entryGates[i].front();
            }
        }
        optimized = true;
        return tour;
    }

    PolygonsD buildPaths()
    {
        if (polygonMode)
        {
            throw RuntimeError("buildPaths is only available for fill-result (path) regions");
        }
        if (!optimized)
        {
            optimizeOrder();
        }

        PolygonsD result;
        for (int id : tour)
        {
            auto it = std::ranges::find_if(regions, [&](const RegionData& r) { return r.id == id; });
            if (it == regions.end())
            {
                continue;
            }
            auto arranged = arrangeRegionPaths(*it);
            result.insert(result.end(), arranged.begin(), arranged.end());
        }
        return result;
    }

    PolygonsD buildPolygons()
    {
        if (!polygonMode)
        {
            throw RuntimeError("buildPolygons is only available for polygon regions");
        }
        if (!optimized)
        {
            optimizeOrder();
        }

        PolygonsD result;
        for (int id : tour)
        {
            auto it = std::ranges::find_if(regions, [&](const RegionData& r) { return r.id == id; });
            if (it == regions.end())
            {
                continue;
            }
            auto arranged = arrangeRegionPolygons(*it);
            result.insert(result.end(), arranged.begin(), arranged.end());
        }
        return result;
    }

    // Area-level full combination graph: cheapest gate-aware shortest path between every ordered pair
    RegionPathOptimizer::FullGraphData getFullGraph()
    {
        RegionAreaGraph ag;
        buildAreaGraph(ag);

        std::vector<int> areas;
        areas.reserve(regions.size());
        for (const auto& r : regions)
        {
            areas.push_back(r.id);
        }

        auto dump = ag.dumpPairwiseGraph(areas);
        RegionPathOptimizer::FullGraphData data;
        data.vertices = dump.areas;
        const std::size_t n = dump.areas.size();
        data.distMatrix.assign(n, std::vector<double>(n, -1.0));
        for (std::size_t i = 0; i < n; ++i)
        {
            for (std::size_t j = 0; j < n; ++j)
            {
                if (dump.distMatrix[i][j].has_value())
                {
                    data.distMatrix[i][j] = dump.distMatrix[i][j].value();
                }
            }
        }
        for (const auto& e : dump.edges)
        {
            data.edges.push_back({e.from, e.to, e.cost});
        }
        return data;
    }

    // Gate-level expanded graph: all entry/exit vertices and every transit/route edge
    RegionPathOptimizer::ExpandedGraphData getExpandedGraph()
    {
        RegionAreaGraph ag;
        buildAreaGraph(ag);

        auto dump = ag.dumpExpandedGraph();
        RegionPathOptimizer::ExpandedGraphData data;
        data.vertices.reserve(dump.vertices.size());
        for (const auto& v : dump.vertices)
        {
            data.vertices.push_back({v.areaId, v.gateId, v.isExit});
        }
        data.edges.reserve(dump.edges.size());
        for (const auto& e : dump.edges)
        {
            data.edges.push_back({e.fromIdx, e.toIdx, e.weight});
        }
        return data;
    }

    // Accept a user-computed order and entry gates, skipping the built-in genetic TSP
    void setCustomTour(const std::vector<int>& newTour, const std::unordered_map<int, int>& newEntryGates)
    {
        if (newTour.empty())
        {
            throw RuntimeError("Custom tour cannot be empty");
        }
        tour = newTour;
        entryGates = newEntryGates;
        optimized = true;
    }

private:
    bool modeSet = false;  // whether the optimization mode is set (decided by the first region)
    bool polygonMode = false;

    void checkDuplicate(int regionId) const
    {
        auto it = std::ranges::find_if(regions, [&](const RegionData& r) { return r.id == regionId; });
        if (it != regions.end())
        {
            throw RuntimeError(std::format("Region already exists: {}", regionId));
        }
    }

    static Point2D gatePoint(const RegionData& r, int gate) { return r.gates.at(static_cast<std::size_t>(gate)); }

    void buildAreaGraph(RegionAreaGraph& ag) const
    {
        for (const auto& r : regions)
        {
            RegionAreaGraph::Config cfg;
            cfg.allowSameGateInOut = true;
            cfg.sameGateInternalCost = 0.0;
            const int gateCount = static_cast<int>(r.gates.size());
            for (int g = 0; g < gateCount; ++g)
            {
                cfg.gates.push_back({.id = g, .canEnter = true, .canExit = true});
            }
            // intra-region gate-to-gate cost = straight-line gate distance (movement cost inside the region)
            for (int i = 0; i < gateCount; ++i)
            {
                for (int j = 0; j < gateCount; ++j)
                {
                    if (i == j)
                    {
                        continue;
                    }
                    cfg.internalCosts[{i, j}] = PointDist(r.gates[i], r.gates[j]);
                }
            }
            ag.addArea(r.id, cfg);
        }

        // inter-region routing: straight-line distance of all gate pairs (air-move cost); manually specified costs
        // override the auto-computed ones
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            for (std::size_t j = 0; j < regions.size(); ++j)
            {
                if (i == j)
                {
                    continue;
                }
                const auto& a = regions[i];
                const auto& b = regions[j];
                std::map<std::pair<int, int>, double> gateWeights;
                auto mit = manualRoutes.find({a.id, b.id});
                if (mit != manualRoutes.end())
                {
                    gateWeights[{0, 0}] = mit->second;
                }
                else
                {
                    const int ga = static_cast<int>(a.gates.size());
                    const int gb = static_cast<int>(b.gates.size());
                    for (int ea = 0; ea < ga; ++ea)
                    {
                        for (int eb = 0; eb < gb; ++eb)
                        {
                            gateWeights[{ea, eb}] = PointDist(a.gates[ea], b.gates[eb]);
                        }
                    }
                }
                ag.addRoute(a.id, b.id, gateWeights);
            }
        }
    }

    // Fill-result mode: intra-region greedy arrangement - start from the entry-gate endpoint, each time pick the
    // nearest path endpoint, reversing paths when needed
    PolygonsD arrangeRegionPaths(const RegionData& r) const
    {
        PolygonsD out;
        if (r.paths.empty())
        {
            return out;
        }

        Point2D current{};
        auto eit = entryGates.find(r.id);
        if (eit != entryGates.end() && eit->second >= 0 && std::cmp_less(eit->second, r.gates.size()))
        {
            current = gatePoint(r, eit->second);
        }
        else
        {
            current = r.paths.front().front();
        }

        std::vector<bool> used(r.paths.size(), false);
        for (std::size_t step = 0; step < r.paths.size(); ++step)
        {
            std::size_t best = r.paths.size();
            bool bestRev = false;
            double bestD = std::numeric_limits<double>::infinity();
            for (std::size_t k = 0; k < r.paths.size(); ++k)
            {
                if (used[k] || r.paths[k].empty())
                {
                    continue;
                }
                double const df = PointDist(current, r.paths[k].front());
                double const db = PointDist(current, r.paths[k].back());
                if (df < bestD)
                {
                    bestD = df;
                    best = k;
                    bestRev = false;
                }
                if (db < bestD)
                {
                    bestD = db;
                    best = k;
                    bestRev = true;
                }
            }
            if (best == r.paths.size())
            {
                break;
            }
            used[best] = true;
            PolygonD p = r.paths[best];
            if (bestRev)
            {
                std::ranges::reverse(p);
            }
            current = p.back();
            out.push_back(std::move(p));
        }
        return out;
    }

    // Polygon mode: intra-region greedy arrangement - start from the entry-gate vertex, each time pick the polygon
    // whose nearest vertex is closest, rotate that polygon's start to the nearest vertex (no reversal, preserving
    // winding); a closed contour returns to its start after one loop.
    PolygonsD arrangeRegionPolygons(const RegionData& r) const
    {
        PolygonsD out;
        if (r.paths.empty())
        {
            return out;
        }

        Point2D current{};
        auto eit = entryGates.find(r.id);
        if (eit != entryGates.end() && eit->second >= 0 && std::cmp_less(eit->second, r.gates.size()))
        {
            current = gatePoint(r, eit->second);
        }
        else
        {
            current = r.paths.front().front();
        }

        std::vector<bool> used(r.paths.size(), false);
        for (std::size_t step = 0; step < r.paths.size(); ++step)
        {
            std::size_t best = r.paths.size();
            std::size_t bestVi = 0;
            double bestD = std::numeric_limits<double>::infinity();
            for (std::size_t k = 0; k < r.paths.size(); ++k)
            {
                if (used[k])
                {
                    continue;
                }
                for (std::size_t vi = 0; vi < r.paths[k].size(); ++vi)
                {
                    const double d = PointDist(current, r.paths[k][vi]);
                    if (d < bestD)
                    {
                        bestD = d;
                        best = k;
                        bestVi = vi;
                    }
                }
            }
            if (best == r.paths.size())
            {
                break;
            }
            used[best] = true;
            PolygonD p = r.paths[best];
            if (bestVi > 0 && bestVi < p.size())
            {
                std::ranges::rotate(p, p.begin() + static_cast<std::ptrdiff_t>(bestVi));
            }
            // a closed contour returns to its start, used as the current position for the next polygon
            current = p.front();
            out.push_back(std::move(p));
        }
        return out;
    }
};

RegionPathOptimizer::RegionPathOptimizer() : impl_(std::make_unique<Impl>())
{
}
RegionPathOptimizer::~RegionPathOptimizer() = default;

void RegionPathOptimizer::addRegion(int regionId, const PolygonsD& paths)
{
    impl_->addRegion(regionId, paths);
}

void RegionPathOptimizer::addPolygonRegion(int regionId, const PolygonsD& polygons)
{
    impl_->addPolygonRegion(regionId, polygons);
}

void RegionPathOptimizer::addRoute(int fromId, int toId, double cost)
{
    impl_->addRoute(fromId, toId, cost);
}

std::vector<int> RegionPathOptimizer::optimizeOrder()
{
    return impl_->optimizeOrder();
}

PolygonsD RegionPathOptimizer::buildPaths()
{
    return impl_->buildPaths();
}

PolygonsD RegionPathOptimizer::buildPolygons()
{
    return impl_->buildPolygons();
}

RegionPathOptimizer::FullGraphData RegionPathOptimizer::getFullGraph()
{
    return impl_->getFullGraph();
}

RegionPathOptimizer::ExpandedGraphData RegionPathOptimizer::getExpandedGraph()
{
    return impl_->getExpandedGraph();
}

void RegionPathOptimizer::setCustomTour(const std::vector<int>& tour, const std::unordered_map<int, int>& entryGates)
{
    impl_->setCustomTour(tour, entryGates);
}

// =============================================================================
// Lua bindings: global table PathOptimize + RegionPathOptimizer userdata
// =============================================================================
namespace
{
constexpr Utils::TemplateString OPTIMIZER_MT = "HsBa.RegionPathOptimizer";

RegionPathOptimizer* CheckOptimizer(lua_State* L, int idx)
{
    return static_cast<RegionPathOptimizer*>(luaL_checkudata(L, idx, OPTIMIZER_MT.str));
}

int l_optimizer_new(lua_State* L)
{
    NewLuaObject<RegionPathOptimizer, OPTIMIZER_MT>(L);
    return 1;
}

int l_optimizer_gc(lua_State* L)
{
    return LuaGC<RegionPathOptimizer, OPTIMIZER_MT>(L);
}

int l_optimizer_addRegion(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    int const regionId = static_cast<int>(luaL_checkinteger(L, 2));
    PolygonsD const paths = LuaTableToPolygonsD(L, 3);
    opt->addRegion(regionId, paths);
    return 0;
}

int l_optimizer_addPolygons(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    int const regionId = static_cast<int>(luaL_checkinteger(L, 2));
    PolygonsD const polygons = LuaTableToPolygonsD(L, 3);
    opt->addPolygonRegion(regionId, polygons);
    return 0;
}

int l_optimizer_addRoute(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    int const fromId = static_cast<int>(luaL_checkinteger(L, 2));
    int const toId = static_cast<int>(luaL_checkinteger(L, 3));
    double const cost = luaL_checknumber(L, 4);
    opt->addRoute(fromId, toId, cost);
    return 0;
}

int l_optimizer_optimizeOrder(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    auto order = opt->optimizeOrder();
    lua_newtable(L);
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        lua_pushinteger(L, order[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

int l_optimizer_buildPaths(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    auto paths = opt->buildPaths();
    PushPolygonsDToLua(L, paths);
    return 1;
}

int l_optimizer_buildPolygons(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    auto polygons = opt->buildPolygons();
    PushPolygonsDToLua(L, polygons);
    return 1;
}

// Push the area-level full combination graph onto the Lua stack as a table
void PushFullGraphToLua(lua_State* L, const RegionPathOptimizer::FullGraphData& g)
{
    lua_newtable(L);

    // vertices = {areaId...}
    lua_newtable(L);
    for (std::size_t i = 0; i < g.vertices.size(); ++i)
    {
        lua_pushinteger(L, g.vertices[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "vertices");

    // distMatrix = {{cost...}...}; unreachable -> false
    lua_newtable(L);
    for (std::size_t i = 0; i < g.distMatrix.size(); ++i)
    {
        lua_newtable(L);
        for (std::size_t j = 0; j < g.distMatrix[i].size(); ++j)
        {
            if (g.distMatrix[i][j] < 0.0)
            {
                lua_pushboolean(L, 0);  // false = unreachable
            }
            else
            {
                lua_pushnumber(L, g.distMatrix[i][j]);
            }
            lua_rawseti(L, -2, static_cast<lua_Integer>(j + 1));
        }
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "distMatrix");

    // edges = {{from=.., to=.., cost=..}, ...}
    lua_newtable(L);
    for (std::size_t i = 0; i < g.edges.size(); ++i)
    {
        lua_newtable(L);
        lua_pushinteger(L, g.edges[i].from);
        lua_setfield(L, -2, "from");
        lua_pushinteger(L, g.edges[i].to);
        lua_setfield(L, -2, "to");
        lua_pushnumber(L, g.edges[i].cost);
        lua_setfield(L, -2, "cost");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "edges");

    lua_pushinteger(L, static_cast<lua_Integer>(g.vertices.size()));
    lua_setfield(L, -2, "areaCount");
}

// Push the gate-level expanded graph onto the Lua stack as a table
void PushExpandedGraphToLua(lua_State* L, const RegionPathOptimizer::ExpandedGraphData& g)
{
    lua_newtable(L);

    // vertices = {{areaId=.., gateId=.., isExit=..}, ...}
    lua_newtable(L);
    for (std::size_t i = 0; i < g.vertices.size(); ++i)
    {
        lua_newtable(L);
        lua_pushinteger(L, g.vertices[i].areaId);
        lua_setfield(L, -2, "areaId");
        lua_pushinteger(L, g.vertices[i].gateId);
        lua_setfield(L, -2, "gateId");
        lua_pushboolean(L, g.vertices[i].isExit ? 1 : 0);
        lua_setfield(L, -2, "isExit");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "vertices");

    // edges = {{from=.., to=.., weight=..}, ...} (1-based vertex indices for Lua convenience)
    lua_newtable(L);
    for (std::size_t i = 0; i < g.edges.size(); ++i)
    {
        lua_newtable(L);
        lua_pushinteger(L, static_cast<lua_Integer>(g.edges[i].fromIdx + 1));
        lua_setfield(L, -2, "from");
        lua_pushinteger(L, static_cast<lua_Integer>(g.edges[i].toIdx + 1));
        lua_setfield(L, -2, "to");
        lua_pushnumber(L, g.edges[i].weight);
        lua_setfield(L, -2, "weight");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "edges");

    lua_pushinteger(L, static_cast<lua_Integer>(g.vertices.size()));
    lua_setfield(L, -2, "vertexCount");
    lua_pushinteger(L, static_cast<lua_Integer>(g.edges.size()));
    lua_setfield(L, -2, "edgeCount");
}

int l_optimizer_getFullGraph(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    auto g = opt->getFullGraph();
    PushFullGraphToLua(L, g);
    return 1;
}

int l_optimizer_getExpandedGraph(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    auto g = opt->getExpandedGraph();
    PushExpandedGraphToLua(L, g);
    return 1;
}

// opt:setCustomTour(tour [, entryGates])
// tour: ordered array of region ids; entryGates: optional table {[areaId]=gateIndex}
int l_optimizer_setCustomTour(lua_State* L)
{
    auto* opt = CheckOptimizer(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    std::vector<int> tour;
    const int tourLen = static_cast<int>(lua_rawlen(L, 2));
    tour.reserve(static_cast<std::size_t>(tourLen));
    for (int i = 1; i <= tourLen; ++i)
    {
        lua_rawgeti(L, 2, i);
        tour.push_back(static_cast<int>(luaL_checkinteger(L, -1)));
        lua_pop(L, 1);
    }

    std::unordered_map<int, int> entryGates;
    if (lua_istable(L, 3))
    {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0)
        {
            int const areaId = static_cast<int>(luaL_checkinteger(L, -2));
            int const gate = static_cast<int>(luaL_checkinteger(L, -1));
            entryGates[areaId] = gate;
            lua_pop(L, 1);  // pop value, keep key
        }
    }

    opt->setCustomTour(tour, entryGates);
    return 0;
}

const luaL_Reg optimizerMethods[] = {{.name = "addRegion", .func = l_optimizer_addRegion},
                                     {.name = "addPolygons", .func = l_optimizer_addPolygons},
                                     {.name = "addRoute", .func = l_optimizer_addRoute},
                                     {.name = "optimizeOrder", .func = l_optimizer_optimizeOrder},
                                     {.name = "buildPaths", .func = l_optimizer_buildPaths},
                                     {.name = "buildPolygons", .func = l_optimizer_buildPolygons},
                                     {.name = "getFullGraph", .func = l_optimizer_getFullGraph},
                                     {.name = "getExpandedGraph", .func = l_optimizer_getExpandedGraph},
                                     {.name = "setCustomTour", .func = l_optimizer_setCustomTour},
                                     {.name = "__gc", .func = l_optimizer_gc},
                                     {.name = nullptr, .func = nullptr}};

// PathOptimize.optimizeRegions(regions): one-shot fill-result-mode optimization, returns the complete fill paths
// (supports multi-point polylines)
int l_optimizeRegions(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const int regionCount = static_cast<int>(lua_rawlen(L, 1));

    RegionPathOptimizer opt;
    for (int i = 1; i <= regionCount; ++i)
    {
        lua_rawgeti(L, 1, i);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }
        PolygonsD const paths = LuaTableToPolygonsD(L, -1);
        lua_pop(L, 1);
        if (!paths.empty())
        {
            opt.addRegion(i, paths);
        }
    }

    opt.optimizeOrder();
    auto result = opt.buildPaths();
    PushPolygonsDToLua(L, result);
    return 1;
}

// PathOptimize.optimizePolygons(regions): one-shot polygon-mode optimization (runs before filling), returns the polygon
// set in optimized order
int l_optimizePolygons(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const int regionCount = static_cast<int>(lua_rawlen(L, 1));

    RegionPathOptimizer opt;
    for (int i = 1; i <= regionCount; ++i)
    {
        lua_rawgeti(L, 1, i);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }
        PolygonsD const polygons = LuaTableToPolygonsD(L, -1);
        lua_pop(L, 1);
        if (!polygons.empty())
        {
            opt.addPolygonRegion(i, polygons);
        }
    }

    opt.optimizeOrder();
    auto result = opt.buildPolygons();
    PushPolygonsDToLua(L, result);
    return 1;
}

const luaL_Reg pathOptimizeLib[] = {{.name = "new", .func = l_optimizer_new},
                                    {.name = "optimizeRegions", .func = l_optimizeRegions},
                                    {.name = "optimizePolygons", .func = l_optimizePolygons},
                                    {.name = nullptr, .func = nullptr}};

// Push the region collection onto the Lua stack: region array -> polyline array -> {x=..,y=..} point array
void PushRegionsToLua(lua_State* L, const std::vector<PolygonsD>& regions)
{
    lua_newtable(L);
    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        PushPolygonsDToLua(L, regions[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
}

// Create a Lua environment and register polygon-operation, fill, and path-optimization functions
UniqueLua MakeOptimizeLuaState(const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
    {
        throw RuntimeError("Failed to create Lua state");
    }
    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    RegisterLuaPolygonFillFunctions(L.get());
    RegisterLuaPathOptimizeFunctions(L.get());
    if (lua_reg)
    {
        lua_reg(L.get());
    }
    return L;
}

// Call the optimization function in the script and retrieve the complete fill paths
PolygonsD CallOptimizeFunction(lua_State* L, const std::vector<PolygonsD>& regions, const std::string& functionName)
{
    lua_getglobal(L, functionName.c_str());
    if (!lua_isfunction(L, -1))
    {
        throw RuntimeError("Lua function not found: " + functionName);
    }

    PushRegionsToLua(L, regions);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK)
    {
        std::string const err = lua_tostring(L, -1);
        throw RuntimeError("Error calling Lua function: " + err);
    }
    if (!lua_istable(L, -1))
    {
        throw RuntimeError("Lua function did not return a table");
    }

    return LuaTableToPolygonsD(L, -1);
}

// Load the script (file or inline string) and call its optimization function, retrieving the result set (paths or
// polygons)
PolygonsD LoadAndCallOptimize(lua_State* L, const char* source, bool isFile, const std::vector<PolygonsD>& regions,
                              const std::string& functionName)
{
    const int loadResult = isFile ? luaL_loadfile(L, source) : luaL_loadstring(L, source);
    if (loadResult != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK)
    {
        std::string const err = lua_tostring(L, -1);
        throw RuntimeError((isFile ? "Failed to load Lua script: " : "Failed to load Lua string: ") + err);
    }
    return CallOptimizeFunction(L, regions, functionName);
}

}  // namespace

void RegisterLuaPathOptimizeFunctions(lua_State* L)
{
    luaL_newmetatable(L, OPTIMIZER_MT.str);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, optimizerMethods, 0);
    lua_pop(L, 1);

    luaL_newlib(L, pathOptimizeLib);
    lua_setglobal(L, "PathOptimize");
}

PolygonsD LuaOptimizeRegionPaths(const std::vector<PolygonsD>& regions, const std::string& scriptPath,
                                 const std::string& functionName, const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeOptimizeLuaState(lua_reg);
    return LoadAndCallOptimize(L.get(), scriptPath.c_str(), true, regions, functionName);
}

PolygonsD LuaOptimizeRegionPathsString(const std::vector<PolygonsD>& regions, const std::string& script,
                                       const std::string& functionName, const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeOptimizeLuaState(lua_reg);
    return LoadAndCallOptimize(L.get(), script.c_str(), false, regions, functionName);
}

PolygonsD LuaOptimizeRegionPolygons(const std::vector<PolygonsD>& regions, const std::string& scriptPath,
                                    const std::string& functionName, const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeOptimizeLuaState(lua_reg);
    return LoadAndCallOptimize(L.get(), scriptPath.c_str(), true, regions, functionName);
}

PolygonsD LuaOptimizeRegionPolygonsString(const std::vector<PolygonsD>& regions, const std::string& script,
                                          const std::string& functionName,
                                          const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeOptimizeLuaState(lua_reg);
    return LoadAndCallOptimize(L.get(), script.c_str(), false, regions, functionName);
}

}  // namespace HsBa::Slicer
