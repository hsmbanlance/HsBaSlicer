/** @file polygon_fill.hpp
 * @brief Lib-side polygon fill C++ API wrapping the 2D fill engine (line/zigzag/border/Lua custom fill).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LIB_POLYGON_FILL_HPP
#define HSBA_SLICER_LIB_POLYGON_FILL_HPP

#include <string>

#include "../export.h"
#include "2D/IntPolygon.hpp"
#include "2D/PolygonFill.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Fill a polygon.
 * @param poly Input polygon.
 * @param spacing Fill line spacing.
 * @param mode Fill mode (Line/SimpleZigzag/Zigzag).
 * @param angle_deg Fill angle (degrees).
 * @return Filled result polygons.
 */
HSBA_SLICER_LIB_API Polygons FillPolygon(const Polygons& poly, double spacing, FillMode mode, double angle_deg);

/**
 * @brief Fill with border offset.
 * @param poly Input polygon.
 * @param spacing Inner fill spacing.
 * @param border_count Number of border offset loops.
 * @param fill_mode Fill mode.
 * @param angle_deg Fill angle (degrees).
 * @return Fill result with border.
 */
HSBA_SLICER_LIB_API Polygons FillWithBorder(const Polygons& poly, double spacing, int border_count, FillMode fill_mode,
                                            double angle_deg);

/**
 * @brief Randomize the start points of closed contour/offset paths (wrapper of the 2D engine).
 *        The start is a uniformly sampled point on a loop edge, inserted as a new split vertex.
 * @param paths Closed loops (contours or offset rings); open short paths pass through unchanged.
 * @param seed Deterministic seed (e.g. combine with layer index for per-layer randomization).
 * @param min_separation Optional minimum distance between start points of different loops
 *        (integer coordinate units); best-effort when unsatisfiable, 0 or less disables it.
 * @return Loops rotated to new edge start points with winding and closed form preserved.
 */
HSBA_SLICER_LIB_API Polygons RandomizePathStartPoints(const Polygons& paths, std::uint64_t seed,
                                                      double min_separation = 0.0);

/**
 * @brief Generate custom fill pattern using a Lua script file.
 *
 * Wrapper around 2D LuaCustomFill, providing the fill capability through
 * the LibHsBaSlicer C++ API.
 *
 * @param poly Input polygons.
 * @param scriptPath Path to the Lua script file.
 * @param functionName Lua function name to call (default: "generate_fill").
 * @param lineThickness Thickness of fill lines (default: 0.5).
 * @return Custom fill pattern as polygons.
 */
HSBA_SLICER_LIB_API Polygons LuaCustomFillByFile(const Polygons& poly, const std::string& scriptPath,
                                                 const std::string& functionName = "generate_fill",
                                                 double lineThickness = 0.5);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_POLYGON_FILL_HPP
