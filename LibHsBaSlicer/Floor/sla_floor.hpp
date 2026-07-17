#pragma once
#ifndef HSBA_SLICER_LIB_SLA_FLOOR_HPP
#define HSBA_SLICER_LIB_SLA_FLOOR_HPP

#include "../export.h"
#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"

#include <functional>
#include <string>
#include <vector>

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

// ============================================================
// SLA image rendering
// ============================================================

/**
 * @brief Render polygons to an image file.
 *
 * Computes pixel size automatically from polygon bounding box and requested
 * image dimensions, then writes the rendered image to outPath.
 *
 * @param polys Input polygons to render.
 * @param width Requested image width in pixels (0 = auto 800).
 * @param height Requested image height in pixels (0 = auto 600).
 * @param outPath Output image file path (extension determines format: .png, .jpg, .svg).
 * @return true if rendering succeeded, false otherwise.
 */
HSBA_SLICER_LIB_API bool RenderPolygonsToImage(const PolygonsD& polys, int width, int height,
                                                const std::string& outPath);

// ============================================================
// SLA package export
// ============================================================

/**
 * @brief Data package for SLA export (layer images, floor, supports, config).
 */
struct SlaPackage
{
    std::vector<PolygonsD> layer_outlines;       ///< Per-layer slice outlines
    std::vector<PolygonsD> layer_supports;       ///< Per-layer support polygons (empty if no support)
    PolygonsD floor_polygons;                    ///< Floor / raft polygons (empty if no floor)
    std::string config_json;                     ///< Configuration JSON content
    int image_width = 0;                         ///< Image width (0 = auto)
    int image_height = 0;                        ///< Image height (0 = auto)
    std::string image_extension = ".png";        ///< Image file extension (.png, .jpg, .svg)
    bool include_floor_images = true;            ///< Generate floor image
    bool include_support_images = true;          ///< Generate support images
};

/**
 * @brief Save SLA package as a zip archive with layer images, floor images,
 *        support images, and a config.json.
 *
 * Internally renders polygons to images and packages them using ImagesPath.
 *
 * @param pkg SLA package data.
 * @param output_zip Output zip file path.
 * @return true if save succeeded, false otherwise.
 */
HSBA_SLICER_LIB_API bool SaveSlaPackage(const SlaPackage& pkg, const std::string& output_zip);

/**
 * @brief Save SLA package with Lua-customized export logic.
 *
 * @param pkg SLA package data.
 * @param output_zip Output zip file path.
 * @param lua_script Inline Lua script source code.
 * @param lua_func Lua function name to call (default: "export_sla").
 * @return true if save succeeded, false otherwise.
 */
HSBA_SLICER_LIB_API bool SaveSlaPackageLua(const SlaPackage& pkg, const std::string& output_zip,
                                            const std::string& lua_script,
                                            const std::string& lua_func = "export_sla");

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_SLA_FLOOR_HPP
