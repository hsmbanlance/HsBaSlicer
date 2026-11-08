/** @file geometry_store.hpp
 * @brief Process-wide snapshot pool holding the layer geometry of finished runs.
 *
 * Pipelines keep their layer contours in function-local vectors, so a GUI cannot
 * reach them through the C ABI. When a config sets capture_geometry the pipeline
 * copies the contours into a GeometrySnapshot here and returns the id in the result
 * struct; geometry_export.h then serves the data back to the host.
 *
 * The pool is protected by one mutex and is safe to call from any thread. Ids are
 * monotonically increasing and never reused. Each snapshot owns one reference; a
 * host that wants to keep a preview open across runs calls Retain and later
 * Release. Once the pool holds more snapshots than HSBA_GEOMETRY_POOL_MAX allows,
 * the oldest snapshot that nobody retained is dropped, and its id stops resolving.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_GEOMETRY_STORE_HPP
#define HSBA_SLICER_GEOMETRY_STORE_HPP

#include <array>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "DllHsBaSlicer/geometry_export.h"
#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::Pipeline
{

/** @brief Axis-aligned extent of one layer as {min_x, min_y, max_x, max_y} in millimetres. */
using BoundsD = std::array<double, 4>;

/**
 * @brief Copy of the layer geometry a single pipeline run produced.
 *
 * Every slot is indexed by layer. Slots a process does not produce stay empty
 * vectors, and a slot shorter than the layer count is read as empty past its end.
 */
struct GeometrySnapshot
{
    HsBaPipelineKind kind = HSBA_PIPELINE_FDM;  ///< Process the snapshot was captured from
    std::vector<PolygonsD> outlines;            ///< Part contours per layer
    std::vector<PolygonsD> fills;               ///< FDM infill / SLA floor rings per layer
    std::vector<PolygonsD> supports;            ///< Support cross sections per layer
    std::vector<double> zs;                     ///< Layer platform heights (mm)
    std::vector<BoundsD> bounds;                ///< Layer bounds, same size as the layer count
    std::vector<double> path_xyz;               ///< Flattened x,y,z triples of a continuous path (mm).
                                                ///< A spiral/vase run stores the helix it deposited; empty =
                                                ///< this run had no continuous path at all.
};

/**
 * @brief Copy the per-layer geometry of a run into the pool.
 * @param kind Process kind, stored for diagnostics and to document the slot mapping.
 * @param outlines Part contours per layer (may be empty).
 * @param fills Infill or floor rings per layer; pass an empty vector when the process has none.
 * @param supports Support rings per layer; pass an empty vector when the process has none.
 * @param zs Platform height of each layer, normally the pipeline's layer_z_heights.
 * @param path_xyz Flattened x,y,z triples of the continuous path the same run deposited
 *        (the spiralized wall of a vase-mode run). The path must belong to the layers
 *        passed in: it is stored together with them and cannot be changed afterwards.
 *        Leave empty when the run only emits per-layer rings.
 * @return Snapshot id to store in the result struct, or 0 when there is nothing to show.
 */
long long CaptureLayerGeometry(HsBaPipelineKind kind, const std::vector<PolygonsD>& outlines,
                               const std::vector<PolygonsD>& fills, const std::vector<PolygonsD>& supports,
                               const std::vector<double>& zs, const std::vector<double>& path_xyz = {});

/** @brief Layer count of a live snapshot, 0 when the id is unknown or recycled. */
int SnapshotLayerCount(long long geometry_id);

/**
 * @brief Serialise one layer into a single malloc block and point @p out at it.
 * @return 1 on success; 0 leaves @p out zeroed for an unknown id or an out-of-range layer.
 */
int ExportLayerGeometry(long long geometry_id, int layer_index, HsBaLayerGeometry_t* out);

/** @brief Point count of a live snapshot's continuous path, 0 when unknown or it has none. */
int PathPointCount(long long geometry_id);

/**
 * @brief Serialise a snapshot's continuous path into a single malloc block and point @p out at it.
 * @return 1 on success; 0 leaves @p out zeroed for an unknown id or a run without a path.
 */
int ExportGeometryPath(long long geometry_id, HsBaGeometryPath_t* out);

/** @brief Add a host reference to a live snapshot; returns the id, or 0 when it is gone. */
long long RetainSnapshot(long long geometry_id);

/** @brief Drop a reference; the last reference (including the pool's own) frees the snapshot. */
void ReleaseSnapshot(long long geometry_id);

}  // namespace HsBa::Slicer::Pipeline

#endif  // !HSBA_SLICER_GEOMETRY_STORE_HPP
