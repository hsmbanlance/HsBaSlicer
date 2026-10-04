/** @file mesh_slice.hpp
 * @brief Planar mesh slicing C++ API: safe/unsafe slice, Lua-driven slice and reusable topology slicing.
 * @author HsBa
 */
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
// Planar slicing along Z; when inter-layer path planning does not interfere, one layer's paths can be handled within a single coroutine

/**
 * @brief Safely slice a model at a given height, ignoring open contours.
 *
 * Returns closed-contour polygons. Prefer this over UnSafeSlice when closed
 * contours are required.
 *
 * @param model Source model to slice.
 * @param height Slice height (Z).
 * @param tolerance Coordinate tolerance for point merging.
 * @return Closed-contour polygons at the slice height.
 */
HSBA_SLICER_LIB_API Polygons Slice(const IModel& model, const float height, double tolerance = 0.001);

/**
 * @brief Slice a model at a given height, including open contours.
 *
 * For material-fed processes the unsafe slice may be considered; for
 * surface-forming processes such as SLA it is not. Use Slice when closed
 * contours are required.
 *
 * @param model Source model to slice.
 * @param height Slice height (Z).
 * @param tolerance Coordinate tolerance for point merging.
 * @return Polygons including open contours at the slice height.
 */
HSBA_SLICER_LIB_API UnSafePolygons UnSafeSlice(const IModel& model, const float height, double tolerance = 0.001);

/**
 * @brief Slice a model at a given height using a Lua script, ignoring open contours.
 * @param model Source model to slice.
 * @param script Inline Lua script driving the slice.
 * @param height Slice height (Z).
 * @return Closed-contour polygons produced by the script.
 */
HSBA_SLICER_LIB_API Polygons SliceLua(const IModel& model, const std::string& script, const float height);

/**
 * @brief Slice a model at a given height using a Lua script, including open contours.
 * @param model Source model to slice.
 * @param script Inline Lua script driving the slice.
 * @param height Slice height (Z).
 * @return Polygons including open contours produced by the script.
 */
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
