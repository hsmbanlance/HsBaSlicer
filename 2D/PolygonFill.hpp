#pragma once
#ifndef HSBA_SLICER_POLYGONFILL_HPP
#define HSBA_SLICER_POLYGONFILL_HPP

#include "FloatPolygons.hpp"
#include "IntPolygon.hpp"
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
                    Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);

/**
 * @brief Generate line fill pattern for polygons.
 * @param poly Input polygons.
 * @param spacing Distance between parallel lines.
 * @param angle_deg Angle of lines in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Fill pattern as polygons.
 */
Polygons LineFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5);

/**
 * @brief Generate simple zigzag fill pattern for polygons.
 * @param poly Input polygons.
 * @param spacing Distance between zigzag lines.
 * @param angle_deg Angle of zigzag pattern in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Fill pattern as polygons.
 */
Polygons SimpleZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5);

/**
 * @brief Generate zigzag fill pattern for polygons with improved corner handling.
 * @param poly Input polygons.
 * @param spacing Distance between zigzag lines.
 * @param angle_deg Angle of zigzag pattern in degrees.
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Fill pattern as polygons.
 */
Polygons ZigzagFill(const Polygons& poly, double spacing, double angle_deg, double lineThickness = 0.5);

/**
 * @brief Enumeration of fill modes.
 */
enum class FillMode
{
    Line,           ///< Parallel line fill
    SimpleZigzag,   ///< Simple zigzag fill
    Zigzag          ///< Advanced zigzag fill
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
                             Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);

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
                    Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);

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
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_POLYGONFILL_HPP
