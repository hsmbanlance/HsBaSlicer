#pragma once
#ifndef HSBA_SLICER_MESH_SLICE_HPP
#define HSBA_SLICER_MESH_SLICE_HPP

#include <string>

#include "../export.h"
#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"
#include "base/IModel.hpp"
#include "meshmodel/FullTopoModel.hpp"

namespace HsBa::Slicer
{
// Z方向平面切片，在层间路径规划不干涉的情况下可以考虑在一个协程内处理一层的路径

// 安全切片，忽略不封闭轮廓
HSBA_SLICER_LIB_API Polygons Slice(const IModel& model, const float height, double tolerance = 0.001);
// 不安全的切片，包含不封闭轮廓。如果需要封闭的轮廓，请使用Slice。
// 在送丝的工艺下可以考虑使用不安全切片，使用SLA等面成型工艺时不考虑使用
HSBA_SLICER_LIB_API UnSafePolygons UnSafeSlice(const IModel& model, const float height, double tolerance = 0.001);

HSBA_SLICER_LIB_API Polygons SliceLua(const IModel& model, const std::string& script, const float height);
HSBA_SLICER_LIB_API UnSafePolygons UnSafeSliceLua(const IModel& model, const std::string& script, const float height);

/**
 * @brief Normalize UnSafePolygons to clean PolygonsD (double-precision).
 *
 * Filters out open polylines and non-simple polygons, then converts
 * from integer to floating-point coordinates. This is the standard
 * post-processing step after UnSafeSlice.
 *
 * @param unsafe_polys Raw unsafe polygons from slicing.
 * @return Cleaned double-precision polygons suitable for downstream processing.
 */
HSBA_SLICER_LIB_API PolygonsD NormalizeUnSafePolygons(const UnSafePolygons& unsafe_polys);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_MESH_SLICE_HPP
