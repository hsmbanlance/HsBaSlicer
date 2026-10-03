#pragma once
#ifndef HSBA_SLICER_MESH_SLICE_HPP
#define HSBA_SLICER_MESH_SLICE_HPP

#include <memory>
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

/**
 * @brief Build a reusable slicing topology once from a model.
 *
 * UnSafeSlice(model, z) rebuilds the full topology on every call, which is
 * O(total_faces) per layer. For multi-layer pipelines, build the topology once
 * with this function and reuse it via SliceLayer to avoid the repeated rebuild.
 *
 * @param model Source model to extract the triangle mesh from.
 * @return Shared, self-contained topology independent of the source model's lifetime.
 */
HSBA_SLICER_LIB_API std::shared_ptr<FullTopoModel> BuildSliceTopology(const IModel& model);

/**
 * @brief Slice a single layer from a prebuilt topology and normalize to PolygonsD.
 *
 * Equivalent to NormalizeUnSafePolygons(topo.UnSafeSlice(z, tolerance)). The
 * topology is only read (const), so concurrent calls sharing the same topo from
 * different threads are safe as long as each writes to its own output slot.
 *
 * @param topo Prebuilt topology from BuildSliceTopology.
 * @param z Layer height to slice at.
 * @param tolerance Coordinate tolerance for point merging.
 * @return Cleaned double-precision polygons for this layer.
 */
HSBA_SLICER_LIB_API PolygonsD SliceLayer(const FullTopoModel& topo, float z, double tolerance = 0.001);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_MESH_SLICE_HPP
