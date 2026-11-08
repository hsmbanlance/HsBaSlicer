/** @file geometry_export.h
 * @brief C ABI for reading the per-layer geometry produced by a pipeline run.
 *
 * A pipeline only keeps layer contours inside its own translation unit, so the
 * library stores a copy of them in a process-wide snapshot pool whenever the
 * config sets capture_geometry. The result struct then carries an opaque
 * geometry_id which is the handle for the four query functions below. Geometry
 * becomes available once the pipeline reached its final stage, it is read-only
 * and it is recycled either by the pool size limit (HSBA_GEOMETRY_POOL_MAX) or
 * by HsBaReleaseGeometrySnapshot.
 *
 * Memory contract: HsBaGetLayerGeometry fills the caller struct from a single
 * malloc block; all ring and coordinate pointers point inside that block, so the
 * host must not free them individually and must call HsBaFreeLayerGeometry once
 * it is done copying the data out. The three ring arrays are consecutive sections
 * of that block and outlines is always its first byte, which is why an empty
 * section still hands back a valid (shared) address rather than NULL.
 *
 * The continuous path read by HsBaGetGeometryPath follows the same contract: one
 * malloc block, one matching HsBaFreeGeometryPath call, and the same lifetime as
 * the layer geometry it belongs to (one snapshot, one reference count, recycled
 * together by HSBA_GEOMETRY_POOL_MAX or HsBaReleaseGeometrySnapshot).
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_GEOMETRY_EXPORT_H
#define HSBA_SLICER_GEOMETRY_EXPORT_H

#include "dllexport.h"
#include "pipelinetypes/pipeline_types.h"

#ifdef __cplusplus
extern "C"
{
#endif  // __cplusplus

    /**
     * @brief One closed or open planar ring, coordinates in millimetres.
     */
    typedef struct HsBaRing
    {
        int point_count;    ///< Number of stored vertices
        const double* xy;   ///< Flattened interleaved coordinates, length 2 * point_count
        int closed;         ///< 1 = the ring repeats its first vertex (contour), 0 = open polyline (fill line)
    } HsBaRing_t;

    /**
     * @brief Geometry of a single layer. Which slots carry data depends on the process:
     *
     * FDM fills outlines / fills / supports; SLA puts the raft of layer 0 into fills;
     * SLS, SLM, LOM, TDP and WAAM only fill outlines.
     */
    typedef struct HsBaLayerGeometry
    {
        int layer_index;            ///< Zero-based layer index asked for
        double z_height;            ///< Platform height of the layer (mm)
        int outline_count;          ///< Outer / inner part contours
        const HsBaRing_t* outlines; ///< outline_count rings, always the start of the owned block
        int fill_count;             ///< Infill lines / SLA floor rings
        const HsBaRing_t* fills;    ///< fill_count rings inside the same block
        int support_count;          ///< Support cross-section rings
        const HsBaRing_t* supports; ///< support_count rings inside the same block
        double bounds_min[2];       ///< [min_x, min_y] over every ring of the layer
        double bounds_max[2];       ///< [max_x, max_y] over every ring of the layer
    } HsBaLayerGeometry_t;

    /**
     * @brief The continuous, Z-rising polyline a spiral/vase run deposited (millimetres).
     *
     * Unlike the per-layer rings, the polyline is already ordered and carries one
     * distinct Z per point, so there is no open/closed flag and no layer slot to
     * index. It is empty for every run that only emits per-layer rings.
     */
    typedef struct HsBaGeometryPath
    {
        int point_count;      ///< Number of xyz triples; 0 when nothing was stored
        const double* xyz;    ///< Flattened x0,y0,z0,x1,y1,z1,...; length 3 * point_count
        double bounds_min[2]; ///< min_x, min_y over the polyline
        double bounds_max[2]; ///< max_x, max_y over the polyline
        double z_min;         ///< Lowest point of the polyline (mm)
        double z_max;         ///< Highest point of the polyline (mm)
    } HsBaGeometryPath_t;

    /**
     * @brief Layer count held by a snapshot.
     * @param geometry_id Snapshot id from a pipeline result (may be 0).
     * @return Layer count, 0 when the id is unknown or already recycled.
     */
    HSBA_SLICER_API int HsBaGeometryLayerCount(long long geometry_id);

    /**
     * @brief Copy one layer of a snapshot into a freshly allocated block.
     * @param geometry_id Snapshot id from a pipeline result.
     * @param layer_index Layer to read, must satisfy 0 <= layer_index < HsBaGeometryLayerCount.
     * @param out Destination struct, overwritten on success and left zeroed on failure.
     * @return 1 on success, 0 for an unknown snapshot, an out-of-range layer or a NULL argument.
     */
    HSBA_SLICER_API int HsBaGetLayerGeometry(long long geometry_id, int layer_index, HsBaLayerGeometry_t* out);

    /**
     * @brief Release the block handed out by HsBaGetLayerGeometry and zero the struct.
     * @param geom Layer geometry to free; may be NULL or already freed (then a no-op).
     */
    HSBA_SLICER_API void HsBaFreeLayerGeometry(HsBaLayerGeometry_t* geom);

    /**
     * @brief Point count of the continuous path a snapshot holds.
     * @param geometry_id Snapshot id from a pipeline result (may be 0).
     * @return Number of xyz triples, 0 when the id is unknown, recycled or the run
     *         deposited no continuous path (every non-spiral run).
     */
    HSBA_SLICER_API int HsBaGeometryPathPointCount(long long geometry_id);

    /**
     * @brief Copy the continuous path of a snapshot into a freshly allocated block.
     * @param geometry_id Snapshot id from a pipeline result.
     * @param path Destination struct, overwritten on success and left zeroed on failure.
     * @return 1 on success, 0 for an unknown snapshot, a run without a path or a NULL argument.
     */
    HSBA_SLICER_API int HsBaGetGeometryPath(long long geometry_id, HsBaGeometryPath_t* path);

    /**
     * @brief Release the block handed out by HsBaGetGeometryPath and zero the struct.
     * @param path Path to free; may be NULL or already freed (then a no-op).
     */
    HSBA_SLICER_API void HsBaFreeGeometryPath(HsBaGeometryPath_t* path);

    /**
     * @brief Pin a snapshot so that the pool size limit cannot recycle it.
     * @param geometry_id Snapshot id to keep alive.
     * @return The same id when pinned, 0 when the snapshot is unknown or already gone.
     */
    HSBA_SLICER_API long long HsBaRetainGeometrySnapshot(long long geometry_id);

    /**
     * @brief Drop one reference taken with HsBaRetainGeometrySnapshot.
     *
     * The snapshot itself owns one reference, so releasing without a matching
     * retain recycles the data immediately. Call this once the preview widget is
     * done with the job, otherwise the snapshot keeps occupying pool capacity.
     * @param geometry_id Snapshot id to unpin.
     */
    HSBA_SLICER_API void HsBaReleaseGeometrySnapshot(long long geometry_id);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // !HSBA_SLICER_GEOMETRY_EXPORT_H
