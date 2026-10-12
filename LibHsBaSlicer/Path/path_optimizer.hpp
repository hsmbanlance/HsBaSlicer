#pragma once
#ifndef HSBA_SLICER_LIB_PATH_OPTIMIZER_HPP
#define HSBA_SLICER_LIB_PATH_OPTIMIZER_HPP

/**
 * @file path_optimizer.hpp
 * @brief Region-based path pre-optimizer and its Lua bindings.
 */

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"

// forward-declare lua state to avoid including lua.hpp in this header
struct lua_State;

namespace HsBa::Slicer
{
/**
 * @brief Path output pre-optimizer: treats each independent polygon region as a
 *        graph vertex (an AreaGraph region) and solves the visit order that
 *        minimizes total travel (air-move) cost.
 *
 * Two optimization modes are available depending on when the optimizer runs
 * (they cannot be mixed within one optimizer instance):
 * - Polygon mode (before fill): regions are polygons themselves (outlines); all
 *   vertices act as entry/exit gates; outputs the polygon set in optimized
 *   order for subsequent filling.
 * - Fill-result mode (after fill): regions are fill paths, supporting multi-point
 *   polylines; the head/tail endpoint of each polyline acts as a gate; outputs the
 *   complete fill paths.
 *
 * Modeling:
 * - Each region is one area vertex in the graph; gates are candidate entry/exit points;
 * - Intra-region gate-to-gate cost is the straight-line gate distance (movement inside
 *   the region); inter-region routing cost is the straight-line gate distance (air-move);
 * - The region visit order is solved by a genetic TSP; intra-region entry/exit order is
 *   arranged by a nearest-neighbor greedy scheme.
 */
class HSBA_SLICER_LIB_API RegionPathOptimizer
{
public:
    /**
     * @brief Area-level full combination graph: the gate-aware shortest-path cost between every
     *        ordered pair of regions, provided both as an N x N distance matrix and a flat edge list.
     *        Enables user-defined Lua algorithms (custom TSP, greedy, annealing, ...) over region order.
     */
    struct FullGraphData
    {
        std::vector<int> vertices;                    // region ids, in matrix order
        std::vector<std::vector<double>> distMatrix;  // [i][j] cost; -1 marks unreachable
        struct EdgeInfo
        {
            int from;     // source region id
            int to;       // target region id
            double cost;  // shortest-path cost between the regions
        };
        std::vector<EdgeInfo> edges;  // flat edge list over reachable pairs
    };

    /**
     * @brief Gate-level expanded graph: every entry/exit gate vertex and all internal transit /
     *        inter-region route edges, enabling fine-grained custom path planning.
     */
    struct ExpandedGraphData
    {
        struct VertexInfo
        {
            int areaId;   // region id this vertex belongs to
            int gateId;   // gate index of the vertex
            bool isExit;  // true = exit side, false = entry side
        };
        struct EdgeInfo
        {
            std::size_t fromIdx;  // index into the vertices array
            std::size_t toIdx;    // index into the vertices array
            double weight;        // edge cost
        };
        std::vector<VertexInfo> vertices;
        std::vector<EdgeInfo> edges;
    };

    RegionPathOptimizer();
    ~RegionPathOptimizer();
    RegionPathOptimizer(const RegionPathOptimizer&) = delete;
    RegionPathOptimizer& operator=(const RegionPathOptimizer&) = delete;

    /**
     * @brief Add a fill-result based region (post-fill optimization, supports
     *        multi-point polylines).
     * @param regionId Unique region identifier.
     * @param paths The region's fill-path set (each a multi-point polyline).
     * @note Cannot be mixed with addPolygonRegion within the same optimizer.
     */
    void addRegion(int regionId, const PolygonsD& paths);

    /**
     * @brief Add a polygon-based region (pre-fill optimization).
     * @param regionId Unique region identifier.
     * @param polygons The region's polygon (outline) set; all vertices act as entry/exit gates.
     * @note Cannot be mixed with addRegion within the same optimizer.
     */
    void addPolygonRegion(int regionId, const PolygonsD& polygons);

    /**
     * @brief Manually specify the symmetric air-move cost between regions,
     *        overriding the auto-computed minimum endpoint distance.
     */
    void addRoute(int fromId, int toId, double cost);

    /**
     * @brief Solve the region visit order (uses TSP when region count >= 2, otherwise
     *        keeps insertion order).
     * @return The ordered list of region ids.
     */
    std::vector<int> optimizeOrder();

    /**
     * @brief Output the complete fill paths in optimized order (fill-result mode;
     *        intra-region path direction/order is greedily arranged to reduce jumps).
     * @note Requires optimizeOrder() first; only valid in fill-result mode.
     */
    PolygonsD buildPaths();

    /**
     * @brief Output the polygon set in optimized order (polygon mode; intra-region polygon
     *        order is greedily arranged, and each polygon's start vertex is rotated to the
     *        entry gate without changing its winding direction).
     * @note Requires optimizeOrder() first; only valid in polygon mode.
     */
    PolygonsD buildPolygons();

    /**
     * @brief Get the area-level full combination graph (cheapest gate-aware shortest-path cost
     *        between every ordered pair of regions). Builds the underlying AreaGraph on demand.
     * @return The complete graph as a distance matrix plus a flat edge list.
     */
    FullGraphData getFullGraph();

    /**
     * @brief Get the gate-level expanded graph (all entry/exit vertices and every internal transit
     *        and inter-region route edge). Builds the underlying AreaGraph on demand.
     * @return The expanded graph vertices and edges.
     */
    ExpandedGraphData getExpandedGraph();

    /**
     * @brief Set a user-computed region visit order and per-region entry gates, bypassing the
     *        built-in genetic TSP. After calling this, buildPaths()/buildPolygons() use this order.
     * @param tour The ordered list of region ids.
     * @param entryGates Optional map region id -> entry gate index (regions absent here use the
     *        default entry strategy).
     */
    void setCustomTour(const std::vector<int>& tour, const std::unordered_map<int, int>& entryGates = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Register Lua path-optimization functions (global table PathOptimize).
 *
 * After registration the following are available in Lua:
 * - PathOptimize.new()                ->
 * optimizer object
 * (addRegion/addPolygons/addRoute/optimizeOrder/buildPaths/buildPolygons/getFullGraph/getExpandedGraph/setCustomTour)
 * - PathOptimize.optimizeRegions(regions)  -> one-shot fill-result-mode optimization, returns the complete fill-path
 * table (supports multi-point polylines)
 * - PathOptimize.optimizePolygons(regions) -> one-shot polygon-mode optimization, returns the polygon table in
 * optimized order (runs before filling) where regions = an array of regions, each region = an array of
 * polylines/polygons, each polyline/polygon = an array of {x=.., y=..} points.
 *
 * The optimizer object additionally exposes the full combination graphs so users can design
 * arbitrary path-optimization algorithms in Lua:
 * - opt:getFullGraph()      -> {vertices={id..}, distMatrix={{cost..}..} (false=unreachable),
 *                               edges={{from,to,cost}..}, areaCount}
 * - opt:getExpandedGraph()  -> {vertices={{areaId,gateId,isExit}..}, edges={{from,to,weight}..}
 *                               (1-based vertex indices), vertexCount, edgeCount}
 * - opt:setCustomTour(tour [, entryGates])  -> feed a user order (array of ids) and optional
 *                               {[areaId]=gateIndex} map, then buildPaths()/buildPolygons() use it.
 *
 * @param L Lua state pointer.
 */
HSBA_SLICER_LIB_API void RegisterLuaPathOptimizeFunctions(lua_State* L);

/**
 * @brief Pre-optimize the fill paths of independent regions via a Lua script file
 *        (embedded Lua script approach).
 *
 * Lua function signature: function optimize_paths(regions) return paths end
 * - regions: an array of regions, each region an array of polylines (a polyline being
 *   an array of {x=.., y=..} points)
 * - return value: the optimized complete fill paths (an array of polylines)
 * The script environment already has polygon-operation functions, fill functions, and
 * the PathOptimize optimization functions registered.
 *
 * @param regions Independent polygon regions (each region a set of fill paths).
 * @param scriptPath Lua script file path.
 * @param functionName Lua function name (default "optimize_paths").
 * @param lua_reg Optional extra Lua registration callback.
 * @return The optimized complete fill paths.
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPaths(const std::vector<PolygonsD>& regions,
                                                     const std::string& scriptPath,
                                                     const std::string& functionName = "optimize_paths",
                                                     const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Pre-optimize the fill paths of independent regions via inline Lua script code.
 * @param regions Independent polygon regions (each region a set of fill paths).
 * @param script Inline Lua script code.
 * @param functionName Lua function name (default "optimize_paths").
 * @param lua_reg Optional extra Lua registration callback.
 * @return The optimized complete fill paths.
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPathsString(const std::vector<PolygonsD>& regions,
                                                           const std::string& script,
                                                           const std::string& functionName = "optimize_paths",
                                                           const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Pre-optimize the polygons of independent regions via a Lua script file
 *        (runs before filling, embedded Lua script approach).
 *
 * Lua function signature: function optimize_polygons(regions) return polygons end
 * - regions: an array of regions, each region an array of polygons (a polygon being
 *   an array of {x=.., y=..} points)
 * - return value: the polygon set in optimized order (an array of polygons)
 * The script environment already has polygon-operation functions, fill functions, and
 * the PathOptimize optimization functions registered.
 *
 * @param regions Independent polygon regions (each region a set of polygons).
 * @param scriptPath Lua script file path.
 * @param functionName Lua function name (default "optimize_polygons").
 * @param lua_reg Optional extra Lua registration callback.
 * @return The polygon set in optimized order.
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPolygons(const std::vector<PolygonsD>& regions,
                                                        const std::string& scriptPath,
                                                        const std::string& functionName = "optimize_polygons",
                                                        const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Pre-optimize the polygons of independent regions via inline Lua script code
 *        (runs before filling).
 * @param regions Independent polygon regions (each region a set of polygons).
 * @param script Inline Lua script code.
 * @param functionName Lua function name (default "optimize_polygons").
 * @param lua_reg Optional extra Lua registration callback.
 * @return The polygon set in optimized order.
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPolygonsString(const std::vector<PolygonsD>& regions,
                                                              const std::string& script,
                                                              const std::string& functionName = "optimize_polygons",
                                                              const std::function<void(lua_State*)>& lua_reg = {});

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_PATH_OPTIMIZER_HPP
