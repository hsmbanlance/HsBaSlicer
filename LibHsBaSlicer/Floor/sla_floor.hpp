#pragma once
#ifndef HSBA_SLICER_LIB_SLA_FLOOR_HPP
#define HSBA_SLICER_LIB_SLA_FLOOR_HPP

#include "../export.h"
#include "2D/IntPolygon.hpp"

#include <functional>

struct lua_State;

namespace HsBa::Slicer
{
/**
 * @brief Configuration for SLA floor (raft) generation.
 */
struct SlaFloorConfig
{
    double raft_offset = 2.0;        ///< Raft outward offset from model footprint in mm
    double border_width = 1.0;       ///< Border ring width in mm
    double fill_spacing = 0.5;       ///< Internal fill line spacing in mm
    double fill_angle_deg = 0.0;     ///< Fill pattern angle in degrees
    int border_count = 2;            ///< Number of border offset loops
    bool use_convex_hull = false;    ///< Use convex hull instead of direct footprint
    int concave_hull_points = 0;     ///< Additional points for concave hull (0 = disabled)
};

/**
 * @brief Generate the floor contact area from bottom layer contours.
 *
 * Computes the footprint polygon that contacts the build plate.
 * Optionally applies convex hull or concave hull simplification.
 *
 * @param bottom_layer Bottom layer contour polygons.
 * @param config Floor generation configuration.
 * @return Floor contact area polygons.
 */
HSBA_SLICER_LIB_API Polygons GenerateFloorContact(const Polygons& bottom_layer, const SlaFloorConfig& config);

/**
 * @brief Generate a complete floor (raft) with border and internal fill.
 *
 * Creates a full raft structure:
 * 1. Compute floor contact area (with optional hull simplification).
 * 2. Outward offset to create raft border.
 * 3. Generate inward border loops.
 * 4. Fill the interior region.
 *
 * @param bottom_layer Bottom layer contour polygons.
 * @param config Floor generation configuration.
 * @return Complete raft polygons (border + fill).
 */
HSBA_SLICER_LIB_API Polygons GenerateFloorRaft(const Polygons& bottom_layer, const SlaFloorConfig& config);

/**
 * @brief Generate only the border ring around the floor.
 *
 * @param bottom_layer Bottom layer contour polygons.
 * @param config Floor generation configuration.
 * @return Border loop polygons.
 */
HSBA_SLICER_LIB_API Polygons GenerateFloorBorder(const Polygons& bottom_layer, const SlaFloorConfig& config);

/**
 * @brief Generate only the internal fill for the floor.
 *
 * @param bottom_layer Bottom layer contour polygons.
 * @param config Floor generation configuration.
 * @return Internal fill polygons.
 */
HSBA_SLICER_LIB_API Polygons GenerateFloorFill(const Polygons& bottom_layer, const SlaFloorConfig& config);

/**
 * @brief Generate floor using a custom Lua script file.
 *
 * The Lua function receives the bottom layer polygons (as a table of polygons
 * with {x,y} points) and a config table with fields: raft_offset, border_width,
 * fill_spacing, fill_angle_deg, border_count, use_convex_hull, concave_hull_points.
 * It must return a table of polygons representing the floor result.
 *
 * Built-in Lua libraries available: PolygonFill (offsetFill, lineFill, zigzagFill, etc.)
 * and polygon operations (Union, Difference, Intersection, Offset, etc.).
 *
 * @param bottom_layer Bottom layer contour polygons.
 * @param script_path Path to the Lua script file.
 * @param function_name Lua function name to call (default: "generate_floor").
 * @param config Floor generation configuration passed to Lua.
 * @param lua_reg Optional callback to register additional Lua functions.
 * @return Custom floor polygons.
 */
HSBA_SLICER_LIB_API Polygons LuaCustomFloorByFile(
    const Polygons& bottom_layer, const std::string& script_path,
    const std::string& function_name, const SlaFloorConfig& config,
    const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Generate floor using an inline Lua script string.
 *
 * Same as LuaCustomFloorByFile but accepts inline Lua source code instead of a file path.
 *
 * @param bottom_layer Bottom layer contour polygons.
 * @param lua_script Inline Lua script source code.
 * @param function_name Lua function name to call (default: "generate_floor").
 * @param config Floor generation configuration passed to Lua.
 * @param lua_reg Optional callback to register additional Lua functions.
 * @return Custom floor polygons.
 */
HSBA_SLICER_LIB_API Polygons LuaCustomFloorByString(
    const Polygons& bottom_layer, const std::string& lua_script,
    const std::string& function_name, const SlaFloorConfig& config,
    const std::function<void(lua_State*)>& lua_reg = {});

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_SLA_FLOOR_HPP
