/** @file PolygonFill.hpp
 * @brief Polygon infill pattern generators (offset, line, zigzag, composite, hybrid and Lua-custom fills).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_POLYGONFILL_HPP
#define HSBA_SLICER_POLYGONFILL_HPP

#include "FloatPolygons.hpp"
#include "IntPolygon.hpp"
#include "base/contracts.hpp"
#include <cstdint>
#include <functional>

// forward-declare lua state to avoid including lua.hpp in this header
struct lua_State;

namespace HsBa::Slicer
{
// only one outer polygon and multiple holes supported

/**
 * @brief Generate offset-based fill patterns for polygons.
 * @param poly Input polygons (supports one outer polygon with multiple holes).
 * @param spacing Distance between offset lines.
 * @param join_type Type of join for corners (default: Square).
 * @return Fill pattern as polygons.
 */
Polygons OffsetFill(const Polygons& poly, double spacing,
                    Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square)
    HSBA_PRE(Contracts::is_positive(spacing));

/**
 * @brief Generate line fill pattern for polygons.
 * @param poly Input polygons.
 * @param spacing Distance between parallel lines.
 * @param angle_deg Angle of lines in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Fill pattern as polygons.
 */
Polygons LineFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5)
    HSBA_PRE(Contracts::is_positive(spacing));

/**
 * @brief Generate simple zigzag fill pattern for polygons.
 * @param poly Input polygons.
 * @param spacing Distance between zigzag lines.
 * @param angle_deg Angle of zigzag pattern in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Fill pattern as polygons.
 */
Polygons SimpleZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5)
    HSBA_PRE(Contracts::is_positive(spacing));

/**
 * @brief Generate zigzag fill pattern for polygons with improved corner handling.
 * @param poly Input polygons.
 * @param spacing Distance between zigzag lines.
 * @param angle_deg Angle of zigzag pattern in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Fill pattern as polygons.
 */
Polygons ZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5)
    HSBA_PRE(Contracts::is_positive(spacing));

/**
 * @brief Enumeration of fill modes.
 */
enum class FillMode
{
    Line,          ///< Parallel line fill
    SimpleZigzag,  ///< Simple zigzag fill
    Zigzag         ///< Advanced zigzag fill
};

/**
 * @brief Generate composite fill with offset borders and internal fill.
 * @param poly Input polygons.
 * @param spacing Spacing for internal fill lines.
 * @param offsetStep Step size for offset operations.
 * @param outwardCount Number of outward offset layers.
 * @param inwardCount Number of inward offset layers.
 * @param mode Fill mode for the interior region.
 * @param angle_deg Angle of fill pattern in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @param join_type Type of join for offset corners (default: Square).
 * @return Composite fill pattern as polygons.
 */
Polygons CompositeOffsetFill(const Polygons& poly, double spacing, double offsetStep, int outwardCount, int inwardCount,
                             FillMode mode, double angle_deg, double lineThickness = 0.5,
                             Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square)
    HSBA_PRE(Contracts::is_positive(spacing));

/**
 * @brief Generate hybrid fill: first apply offsets, then fill the last offset polygons.
 * @param poly Input polygons.
 * @param spacing Spacing for internal fill lines.
 * @param offsetStep Step size for offset operations.
 * @param outwardCount Number of outward offset layers.
 * @param inwardCount Number of inward offset layers.
 * @param mode Fill mode for the interior region.
 * @param angle_deg Angle of fill pattern in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @param join_type Type of join for offset corners (default: Square).
 * @return Hybrid fill pattern as polygons.
 */
Polygons HybridFill(const Polygons& poly, double spacing, double offsetStep, int outwardCount, int inwardCount,
                    FillMode mode, double angle_deg, double lineThickness = 0.5,
                    Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square)
    HSBA_PRE(Contracts::is_positive(spacing));

/**
 * @brief Randomize the start points of closed contour and offset paths.
 *
 * Each closed loop is rotated onto a pseudo-random start point sampled uniformly along its
 * perimeter (a new split vertex is inserted on the chosen edge, so the seam is not confined
 * to original vertices); the original vertex sequence keeps its cyclic order and winding, and
 * the closed form (duplicated closing vertex) is preserved.
 * Typical use: stagger extrusion seams of contours/offset rings across layers or nested
 * rings so they do not pile up into a visible weak line.
 * @param paths Closed loops (outlines from Offset/OffsetOnly, rings from OffsetFill, ...);
 *              paths with fewer than 3 ring points pass through unchanged.
 * @param seed Seed for the random engine; the same seed gives the same result
 *             (e.g. seed by layer index for deterministic per-layer randomization).
 * @param min_separation Minimum distance to keep between the start points of different
 *                       loops (same integer coordinate units as paths); best-effort when no
 *                       candidate satisfies it. Pass 0 or less for unconstrained randomization.
 * @return Loops starting at their new edge split point; vertex content, winding direction
 *         and closed form are preserved (vertex count grows by one per rotated loop).
 */
Polygons RandomizeStartPoints(const Polygons& paths, std::uint64_t seed, double min_separation = 0.0);

// others defined in lua script

/**
 * @brief Generate custom fill pattern using a Lua script file.
 * @param poly Input polygons.
 * @param scriptPath Path to the Lua script file.
 * @param functionName Lua function name to call (default: "generate_fill").
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @param lua_reg Optional Lua registration callback.
 * @return Custom fill pattern as polygons.
 */
Polygons LuaCustomFill(const Polygons& poly, const std::string& scriptPath,
                       const std::string& functionName = "generate_fill", double lineThickness = 0.5,
                       const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Generate custom fill pattern using inline Lua script code.
 * @param poly Input polygons.
 * @param script Inline Lua script code string.
 * @param functionName Lua function name to call (default: "generate_fill").
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @param lua_reg Optional Lua registration callback.
 * @return Custom fill pattern as polygons.
 */
Polygons LuaCustomFillString(const Polygons& poly, const std::string& script,
                             const std::string& functionName = "generate_fill", double lineThickness = 0.5,
                             const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Register Lua polygon fill functions (offsetFill, lineFill, zigzagFill, etc.).
 * @param L Lua state pointer.
 */
void RegisterLuaPolygonFillFunctions(lua_State* L);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_POLYGONFILL_HPP
