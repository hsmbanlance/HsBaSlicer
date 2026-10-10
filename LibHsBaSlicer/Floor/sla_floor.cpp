/** @file sla_floor.cpp
 * @brief Implementation of SLA floor/raft generation, image rendering and SLA package export.
 * @author HsBa
 */
#include "sla_floor.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <future>
#include <thread>
#include <vector>

#include <lua.hpp>

#include "2D/2Dhull.hpp"
#include "2D/ImageToPolygons.hpp"
#include "2D/LuaAdapter.hpp"
#include "2D/PolygonFill.hpp"
#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "base/error.hpp"
#include "base/thread_pool.hpp"
#include "paths/imagespath.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
namespace
{
Polygons ComputeFootprint(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    if (config.use_convex_hull)
    {
        Polygon const hull = ConvexHull(bottom_layer);
        return Polygons{hull};
    }
    if (config.concave_hull_points > 0)
    {
        Polygon const hull = ConcaveHullSimulation(bottom_layer, config.concave_hull_points);
        return Polygons{hull};
    }
    return bottom_layer;
}
}  // namespace

HSBA_SLICER_LIB_API Polygons GenerateFloorContact(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    return ComputeFootprint(bottom_layer, config);
}

HSBA_SLICER_LIB_API Polygons GenerateFloorBorder(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    Polygons const footprint = ComputeFootprint(bottom_layer, config);

    // Outward offset to expand the footprint into raft area
    Polygons const raft_outer = Offset(footprint, config.raft_offset + config.border_width,
                                       Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Inward offset to carve out the interior fill region
    Polygons const raft_inner = Offset(raft_outer, config.border_width * config.border_count,
                                       Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Border = outer - inner
    return Difference(raft_outer, raft_inner);
}

HSBA_SLICER_LIB_API Polygons GenerateFloorFill(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    Polygons const footprint = ComputeFootprint(bottom_layer, config);

    // Expand footprint to raft boundary
    Polygons const raft_outer = Offset(footprint, config.raft_offset + config.border_width,
                                       Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Carve out border region so fill stays inside the border
    Polygons const fill_region = Offset(raft_outer, -config.border_width * config.border_count,
                                        Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    if (fill_region.empty())
    {
        return {};
    }

    return ZigzagFill(fill_region, config.fill_spacing, config.fill_angle_deg);
}

HSBA_SLICER_LIB_API Polygons GenerateFloorRaft(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    Polygons const footprint = ComputeFootprint(bottom_layer, config);

    // Step 1: Outward offset to create full raft boundary
    Polygons const raft_outer = Offset(footprint, config.raft_offset + config.border_width,
                                       Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Step 2: Generate border loops using composite offset fill
    //   - border_count inward loops at border_width spacing
    //   - then zigzag fill for the interior
    Polygons border_loops =
        Offset(raft_outer, -config.border_width / 2.0, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Step 3: Interior fill region (inside the last border loop)
    Polygons const fill_region = Offset(raft_outer, -config.border_width * config.border_count,
                                        Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    Polygons result = std::move(border_loops);

    if (!fill_region.empty())
    {
        Polygons fill = ZigzagFill(fill_region, config.fill_spacing, config.fill_angle_deg);
        result.insert(result.end(), fill.begin(), fill.end());
    }

    return result;
}

namespace
{
void PushFloorConfigToLua(lua_State* L, const SlaFloorConfig& config)
{
    lua_newtable(L);

    lua_pushnumber(L, config.raft_offset);
    lua_setfield(L, -2, "raft_offset");

    lua_pushnumber(L, config.border_width);
    lua_setfield(L, -2, "border_width");

    lua_pushnumber(L, config.fill_spacing);
    lua_setfield(L, -2, "fill_spacing");

    lua_pushnumber(L, config.fill_angle_deg);
    lua_setfield(L, -2, "fill_angle_deg");

    lua_pushinteger(L, config.border_count);
    lua_setfield(L, -2, "border_count");

    lua_pushboolean(L, config.use_convex_hull ? 1 : 0);
    lua_setfield(L, -2, "use_convex_hull");

    lua_pushinteger(L, config.concave_hull_points);
    lua_setfield(L, -2, "concave_hull_points");
}
}  // anonymous namespace

HSBA_SLICER_LIB_API Polygons LuaCustomFloorByFile(const Polygons& bottom_layer, const std::string& script_path,
                                                  const std::string& function_name, const SlaFloorConfig& config,
                                                  const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
    {
        throw RuntimeError("Failed to create Lua state");
    }

    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    RegisterLuaPolygonFillFunctions(L.get());
    if (lua_reg)
    {
        lua_reg(L.get());
    }

    // Load script file
    if (luaL_loadfile(L.get(), script_path.c_str()) || lua_pcall(L.get(), 0, 0, 0))
    {
        throw RuntimeError("Failed to load Lua floor script: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Get function
    lua_getglobal(L.get(), function_name.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua floor function not found: " + function_name);
    }

    // Push bottom layer polygons as first argument
    PolygonsD const bottomD = UnIntegerization(bottom_layer);
    PushPolygonsDToLua(L.get(), bottomD);

    // Push config table as second argument
    PushFloorConfigToLua(L.get(), config);

    // Call function with 2 args, 1 result
    if (lua_pcall(L.get(), 2, 1, 0) != LUA_OK)
    {
        throw RuntimeError("Error calling Lua floor function: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Parse result table
    if (!lua_istable(L.get(), -1))
    {
        throw RuntimeError("Lua floor function did not return a table");
    }

    Polygons result;
    lua_pushnil(L.get());
    while (lua_next(L.get(), -2))
    {
        if (lua_istable(L.get(), -1))
        {
            PolygonD outpoly;
            int const n = static_cast<int>(lua_rawlen(L.get(), -1));
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L.get(), -1, i);
                if (lua_istable(L.get(), -1))
                {
                    lua_getfield(L.get(), -1, "x");
                    lua_getfield(L.get(), -2, "y");
                    double const x = lua_isnumber(L.get(), -2) ? lua_tonumber(L.get(), -2) : 0.0;
                    double const y = lua_isnumber(L.get(), -1) ? lua_tonumber(L.get(), -1) : 0.0;
                    outpoly.emplace_back(Point2D{x, y});
                    lua_pop(L.get(), 2);
                }
                lua_pop(L.get(), 1);
            }
            if (!outpoly.empty())
            {
                result.push_back(Integerization(outpoly));
            }
        }
        lua_pop(L.get(), 1);
    }
    return result;
}

HSBA_SLICER_LIB_API Polygons LuaCustomFloorByString(const Polygons& bottom_layer, const std::string& lua_script,
                                                    const std::string& function_name, const SlaFloorConfig& config,
                                                    const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
    {
        throw RuntimeError("Failed to create Lua state");
    }

    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    RegisterLuaPolygonFillFunctions(L.get());
    if (lua_reg)
    {
        lua_reg(L.get());
    }

    // Load inline script
    if (luaL_loadstring(L.get(), lua_script.c_str()) != LUA_OK)
    {
        throw RuntimeError("Failed to load Lua floor script: " + std::string(lua_tostring(L.get(), -1)));
    }
    if (lua_pcall(L.get(), 0, 0, 0) != LUA_OK)
    {
        throw RuntimeError("Exec Lua floor script failed: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Get function
    lua_getglobal(L.get(), function_name.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua floor function not found: " + function_name);
    }

    // Push bottom layer polygons as first argument
    PolygonsD const bottomD = UnIntegerization(bottom_layer);
    PushPolygonsDToLua(L.get(), bottomD);

    // Push config table as second argument
    PushFloorConfigToLua(L.get(), config);

    // Call function with 2 args, 1 result
    if (lua_pcall(L.get(), 2, 1, 0) != LUA_OK)
    {
        throw RuntimeError("Error calling Lua floor function: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Parse result table
    if (!lua_istable(L.get(), -1))
    {
        throw RuntimeError("Lua floor function did not return a table");
    }

    Polygons result;
    lua_pushnil(L.get());
    while (lua_next(L.get(), -2))
    {
        if (lua_istable(L.get(), -1))
        {
            PolygonD outpoly;
            int const n = static_cast<int>(lua_rawlen(L.get(), -1));
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L.get(), -1, i);
                if (lua_istable(L.get(), -1))
                {
                    lua_getfield(L.get(), -1, "x");
                    lua_getfield(L.get(), -2, "y");
                    double const x = lua_isnumber(L.get(), -2) ? lua_tonumber(L.get(), -2) : 0.0;
                    double const y = lua_isnumber(L.get(), -1) ? lua_tonumber(L.get(), -1) : 0.0;
                    outpoly.emplace_back(Point2D{x, y});
                    lua_pop(L.get(), 2);
                }
                lua_pop(L.get(), 1);
            }
            if (!outpoly.empty())
            {
                result.push_back(Integerization(outpoly));
            }
        }
        lua_pop(L.get(), 1);
    }
    return result;
}

// ============================================================
// SLA image rendering
// ============================================================

namespace
{
// Fits `polys` into the requested width x height, returning the pixel size (world units
// per pixel) and the effective dimensions (defaults 800 x 600 when a dimension is <= 0).
double FitPixelSize(const PolygonsD& polys, int width, int height, int& w, int& h)
{
    double min_x = 1e18;
    double min_y = 1e18;
    double max_x = -1e18;
    double max_y = -1e18;
    for (const auto& poly : polys)
    {
        for (const auto& pt : poly)
        {
            min_x = std::min(min_x, pt.x);
            min_y = std::min(min_y, pt.y);
            max_x = std::max(max_x, pt.x);
            max_y = std::max(max_y, pt.y);
        }
    }

    w = width > 0 ? width : 800;
    h = height > 0 ? height : 600;
    double range_x = max_x - min_x;
    double range_y = max_y - min_y;
    if (range_x < 1e-6)
    {
        range_x = 1.0;
    }
    if (range_y < 1e-6)
    {
        range_y = 1.0;
    }
    return std::max(range_x / w, range_y / h);
}
}  // namespace

HSBA_SLICER_LIB_API bool RenderPolygonsToImage(const PolygonsD& polys, int width, int height,
                                               const std::string& outPath)
{
    if (polys.empty())
    {
        return false;
    }
    int w = 0;
    int h = 0;
    double const pixel_size = FitPixelSize(polys, width, height, w, h);
    return ToImage(polys, w, h, pixel_size, outPath);
}

HSBA_SLICER_LIB_API bool RenderPolygonsToImageBuffer(const PolygonsD& polys, int width, int height,
                                                     const std::string& ext, std::string& outBuffer)
{
    if (polys.empty())
    {
        return false;
    }
    int w = 0;
    int h = 0;
    double const pixel_size = FitPixelSize(polys, width, height, w, h);
    return ToImageBuffer(polys, w, h, pixel_size, ext, outBuffer);
}

// ============================================================
// SLA package export
// ============================================================

namespace
{
// Encodes a single layer polygon set straight into an in-memory image buffer (no temp file).
std::string RenderToBuffer(const PolygonsD& polys, int width, int height, const std::string& ext)
{
    std::string data;
    if (!RenderPolygonsToImageBuffer(polys, width, height, ext, data))
    {
        return {};
    }
    return data;
}

// Runs per-layer functors in parallel across a thread pool on the desktop export hot path.
// Each layer is independent and work() MUST only touch its own output slot. Honors the
// HSBA_PIPELINE_THREADS override and degrades to a serial loop for a single worker or a
// single layer, keeping behavior identical on constrained platforms. Exceptions thrown
// inside work() are rethrown to the caller via the futures.
void ForLayersParallel(int total, const std::function<void(int)>& work)
{
    if (total <= 0)
    {
        return;
    }

    size_t requested = 0;
    if (const char* env = std::getenv("HSBA_PIPELINE_THREADS"))
    {
        long v = std::strtol(env, nullptr, 10);
        if (v > 0)
        {
            requested = static_cast<size_t>(v);
        }
    }

    const unsigned hw = std::thread::hardware_concurrency();
    size_t nthreads = (hw == 0) ? 2U : hw;
    if (requested > 0)
    {
        nthreads = std::min(requested, static_cast<size_t>(total));
    }
    else
    {
        nthreads = std::min(nthreads, static_cast<size_t>(total));
    }

    if (nthreads <= 1 || total <= 1)
    {
        for (int i = 0; i < total; ++i)
        {
            work(i);
        }
        return;
    }

    // Dynamic scheduling: each worker pulls the next layer index from a shared atomic
    // counter, so uneven per-layer costs do not stall the pool behind a fixed barrier.
    ThreadPool pool(nthreads);
    std::atomic<int> next{0};
    std::vector<std::future<void>> futures;
    futures.reserve(nthreads);
    for (size_t t = 0; t < nthreads; ++t)
    {
        futures.push_back(pool.submit(
            [&]
            {
                int idx = next.fetch_add(1, std::memory_order_relaxed);
                while (idx < total)
                {
                    work(idx);
                    idx = next.fetch_add(1, std::memory_order_relaxed);
                }
            }));
    }
    for (auto& f : futures)
    {
        f.get();  // blocks on completion; rethrows any worker exception
    }
}
}  // anonymous namespace

HSBA_SLICER_LIB_API bool SaveSlaPackage(const SlaPackage& pkg, const std::string& output_zip)
{
    try
    {
        ImagesPath images_path("config.json", pkg.config_json, {GetZipperEventCallback()});
        const int total_layers = static_cast<int>(pkg.layer_outlines.size());
        const std::string& ext = pkg.image_extension;

        // Layer images (rendered in parallel on desktop, then inserted serially)
        std::vector<std::string> layer_images(total_layers);
        ForLayersParallel(
            total_layers, [&](int i)
            { layer_images[i] = RenderToBuffer(pkg.layer_outlines[i], pkg.image_width, pkg.image_height, ext); });
        for (int i = 0; i < total_layers; ++i)
        {
            if (!layer_images[i].empty())
            {
                images_path.AddImage("layers/layer_" + std::to_string(i) + ext, layer_images[i]);
            }
        }

        // Floor image
        if (pkg.include_floor_images && !pkg.floor_polygons.empty())
        {
            std::string const floor_img = RenderToBuffer(pkg.floor_polygons, pkg.image_width, pkg.image_height, ext);
            if (!floor_img.empty())
            {
                images_path.AddImage("floor/floor_raft" + ext, floor_img);
            }
        }

        // Support images
        if (pkg.include_support_images)
        {
            const int support_count = std::min(static_cast<int>(pkg.layer_supports.size()), total_layers);
            std::vector<std::string> support_images(support_count);
            ForLayersParallel(support_count,
                              [&](int i)
                              {
                                  if (!pkg.layer_supports[i].empty())
                                  {
                                      support_images[i] =
                                          RenderToBuffer(pkg.layer_supports[i], pkg.image_width, pkg.image_height, ext);
                                  }
                              });
            for (int i = 0; i < support_count; ++i)
            {
                if (!support_images[i].empty())
                {
                    images_path.AddImage("supports/layer_" + std::to_string(i) + ext, support_images[i]);
                }
            }
        }

        images_path.Save(output_zip);
        return true;
    }
    catch (const RuntimeError&)
    {
        return false;
    }
}

HSBA_SLICER_LIB_API bool SaveSlaPackageLua(const SlaPackage& pkg, const std::string& output_zip,
                                           const std::string& lua_script, const std::string& lua_func)
{
    try
    {
        ImagesPath images_path("config.json", pkg.config_json);
        const int total_layers = static_cast<int>(pkg.layer_outlines.size());
        const std::string& ext = pkg.image_extension;

        // Layer images (rendered in parallel on desktop, then inserted serially)
        std::vector<std::string> layer_images(total_layers);
        ForLayersParallel(
            total_layers, [&](int i)
            { layer_images[i] = RenderToBuffer(pkg.layer_outlines[i], pkg.image_width, pkg.image_height, ext); });
        for (int i = 0; i < total_layers; ++i)
        {
            if (!layer_images[i].empty())
            {
                images_path.AddImage("layers/layer_" + std::to_string(i) + ext, layer_images[i]);
            }
        }

        // Floor image
        if (pkg.include_floor_images && !pkg.floor_polygons.empty())
        {
            std::string const floor_img = RenderToBuffer(pkg.floor_polygons, pkg.image_width, pkg.image_height, ext);
            if (!floor_img.empty())
            {
                images_path.AddImage("floor/floor_raft" + ext, floor_img);
            }
        }

        // Support images
        if (pkg.include_support_images)
        {
            const int support_count = std::min(static_cast<int>(pkg.layer_supports.size()), total_layers);
            std::vector<std::string> support_images(support_count);
            ForLayersParallel(support_count,
                              [&](int i)
                              {
                                  if (!pkg.layer_supports[i].empty())
                                  {
                                      support_images[i] =
                                          RenderToBuffer(pkg.layer_supports[i], pkg.image_width, pkg.image_height, ext);
                                  }
                              });
            for (int i = 0; i < support_count; ++i)
            {
                if (!support_images[i].empty())
                {
                    images_path.AddImage("supports/layer_" + std::to_string(i) + ext, support_images[i]);
                }
            }
        }

        std::function<void(lua_State*)> const sla_reg = [](lua_State* L)
        {
            // Register external 2D+File functions for SLA output stage
            for (auto& reg : Get2DFunctions())
            {
                reg(L);
            }
            for (auto& reg : GetFileFunctions())
            {
                reg(L);
            }
        };
        images_path.Save(std::filesystem::path(output_zip), std::string_view(lua_script), std::string_view(lua_func),
                         sla_reg);
        return true;
    }
    catch (const RuntimeError&)
    {
        return false;
    }
}

}  // namespace HsBa::Slicer
