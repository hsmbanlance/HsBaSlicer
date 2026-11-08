/** @file geometry_store.cpp
 * @brief Snapshot pool implementation plus the C ABI of geometry_export.h.
 * @author HsBa
 */
#include "geometry_store.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "base/error.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

/** @brief One pooled snapshot together with its reference count and insertion order. */
struct PoolEntry
{
    GeometrySnapshot snapshot;
    int refs = 1;       ///< Pool's own reference plus one per host Retain
    long long seq = 0;  ///< Insertion order, used to pick the LRU eviction victim
};

/** @brief Guards every access to Pool() and NextId(). */
std::mutex& PoolMutex()
{
    static std::mutex mutex;
    return mutex;
}

/** @brief Live snapshots keyed by their id. */
std::unordered_map<long long, PoolEntry>& Pool()
{
    static std::unordered_map<long long, PoolEntry> pool;
    return pool;
}

/** @brief Monotonic id source; ids are never reused, so stale handles simply stop resolving. */
long long& NextId()
{
    static long long id = 0;
    return id;
}

/**
 * @brief Maximum number of pooled snapshots, read once from HSBA_GEOMETRY_POOL_MAX.
 *
 * A non-positive or unset value keeps the default of 4, which is enough for a
 * handful of full-machine previews while bounding the memory a GUI can consume
 * without releasing. Parsed the same way as HSBA_PIPELINE_THREADS.
 */
std::size_t PoolMax()
{
    static const std::size_t max_count = []()
    {
        std::size_t count = 4;
        if (const char* env = std::getenv("HSBA_GEOMETRY_POOL_MAX"))
        {
            const long v = std::strtol(env, nullptr, 10);
            if (v > 0)
            {
                count = static_cast<std::size_t>(v);
            }
        }
        return count;
    }();
    return max_count;
}

/** @brief Round @p value up to a multiple of @p align. */
std::size_t AlignUp(std::size_t value, std::size_t align)
{
    return (value + align - 1) / align * align;
}

/** @brief Number of layers a snapshot describes, tolerating slots of different lengths. */
std::size_t LayerCountOf(const GeometrySnapshot& snapshot)
{
    std::size_t count = snapshot.zs.size();
    count = std::max(count, snapshot.outlines.size());
    count = std::max(count, snapshot.fills.size());
    count = std::max(count, snapshot.supports.size());
    return count;
}

/** @brief Read one layer of a slot, yielding an empty ring list past its end. */
const PolygonsD& SlotAt(const std::vector<PolygonsD>& slot, std::size_t layer_index)
{
    static const PolygonsD kEmpty;
    return layer_index < slot.size() ? slot[layer_index] : kEmpty;
}

/** @brief Axis-aligned extent of every ring of one layer; all zeroes when the layer is empty. */
BoundsD ComputeBounds(const PolygonsD& outlines, const PolygonsD& fills, const PolygonsD& supports)
{
    BoundsD bounds = {0.0, 0.0, 0.0, 0.0};
    bool any = false;
    const auto absorb = [&bounds, &any](const PolygonD& ring)
    {
        for (const Point2D& point : ring)
        {
            if (!any)
            {
                bounds = {point.x, point.y, point.x, point.y};
                any = true;
                continue;
            }
            bounds[0] = std::min(bounds[0], point.x);
            bounds[1] = std::min(bounds[1], point.y);
            bounds[2] = std::max(bounds[2], point.x);
            bounds[3] = std::max(bounds[3], point.y);
        }
    };
    for (const PolygonD& ring : outlines)
    {
        absorb(ring);
    }
    for (const PolygonD& ring : fills)
    {
        absorb(ring);
    }
    for (const PolygonD& ring : supports)
    {
        absorb(ring);
    }
    return bounds;
}

/** @brief Whether a ring explicitly closes itself.
 *
 * Clipper2 paths are plain vertex vectors and carry no open/closed flag, but this
 * project repeats the first vertex at the end of every closed contour (see
 * MakeRectangle and CloseContour), so endpoint equality is the available signal.
 */
bool IsClosedRing(const PolygonD& ring)
{
    return ring.size() >= 4 && ring.front() == ring.back();
}

/**
 * @brief Drop the oldest unreferenced snapshots until the pool fits HSBA_GEOMETRY_POOL_MAX.
 * @param just_captured Id of the snapshot that triggered the check; it is never a victim,
 *        because returning an id that the very same call recycled would be useless.
 */
void EvictOverflow(long long just_captured)
{
    const std::size_t max_count = PoolMax();
    while (Pool().size() > max_count)
    {
        auto victim = Pool().end();
        for (auto it = Pool().begin(); it != Pool().end(); ++it)
        {
            if (it->second.refs != 1 || it->first == just_captured)
            {
                continue;  // a host still reads this one, or it is the caller's own result
            }
            if (victim == Pool().end() || it->second.seq < victim->second.seq)
            {
                victim = it;
            }
        }
        if (victim == Pool().end())
        {
            return;  // everything else is pinned: tolerate a temporary overflow instead of breaking callers
        }
        Pool().erase(victim);
    }
}

}  // namespace

long long CaptureLayerGeometry(HsBaPipelineKind kind, const std::vector<PolygonsD>& outlines,
                               const std::vector<PolygonsD>& fills, const std::vector<PolygonsD>& supports,
                               const std::vector<double>& zs, const std::vector<double>& path_xyz)
{
    GeometrySnapshot snapshot;
    snapshot.kind = kind;
    snapshot.outlines = outlines;
    snapshot.fills = fills;
    snapshot.supports = supports;
    snapshot.zs = zs;

    const std::size_t layer_count = LayerCountOf(snapshot);
    if (layer_count == 0)
    {
        return 0;  // nothing was produced: the caller reports geometry_id 0 and no memory is held
    }
    // A path without layers has nothing to belong to, so it is only stored once the
    // snapshot itself is going to exist.
    snapshot.path_xyz = path_xyz;
    snapshot.bounds.reserve(layer_count);
    for (std::size_t i = 0; i < layer_count; ++i)
    {
        snapshot.bounds.push_back(ComputeBounds(SlotAt(outlines, i), SlotAt(fills, i), SlotAt(supports, i)));
    }

    const std::lock_guard<std::mutex> lock(PoolMutex());
    const long long id = ++NextId();
    Pool().emplace(id, PoolEntry{std::move(snapshot), 1, id});
    EvictOverflow(id);
    return id;
}

int SnapshotLayerCount(long long geometry_id)
{
    if (geometry_id <= 0)
    {
        return 0;
    }
    const std::lock_guard<std::mutex> lock(PoolMutex());
    const auto it = Pool().find(geometry_id);
    if (it == Pool().end())
    {
        return 0;
    }
    return static_cast<int>(LayerCountOf(it->second.snapshot));
}

int ExportLayerGeometry(long long geometry_id, int layer_index, HsBaLayerGeometry_t* out)
{
    if (!out)
    {
        return 0;
    }
    std::memset(out, 0, sizeof(HsBaLayerGeometry_t));
    if (geometry_id <= 0 || layer_index < 0)
    {
        return 0;
    }

    const std::lock_guard<std::mutex> lock(PoolMutex());
    const auto it = Pool().find(geometry_id);
    if (it == Pool().end())
    {
        return 0;
    }
    const GeometrySnapshot& snapshot = it->second.snapshot;
    const std::size_t count = LayerCountOf(snapshot);
    if (static_cast<std::size_t>(layer_index) >= count)
    {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(layer_index);
    const PolygonsD* slots[3] = {&SlotAt(snapshot.outlines, index), &SlotAt(snapshot.fills, index),
                                 &SlotAt(snapshot.supports, index)};

    std::size_t ring_total = 0;
    std::size_t point_total = 0;
    for (const PolygonsD* slot : slots)
    {
        ring_total += slot->size();
        for (const PolygonD& ring : *slot)
        {
            point_total += ring.size();
        }
    }

    // One block: the three ring arrays first (naturally 8-byte spaced), then all coordinates.
    const std::size_t xy_align = alignof(double);
    const std::size_t ring_bytes = AlignUp(ring_total * sizeof(HsBaRing_t), xy_align);
    const std::size_t xy_bytes = point_total * 2 * sizeof(double);
    void* block = std::malloc(ring_bytes + xy_bytes + xy_align);
    if (!block)
    {
        return 0;
    }
    auto* rings = static_cast<HsBaRing_t*>(block);
    double* xy = reinterpret_cast<double*>(static_cast<unsigned char*>(block) + ring_bytes);

    out->layer_index = layer_index;
    out->z_height = index < snapshot.zs.size() ? snapshot.zs[index] : 0.0;
    if (index < snapshot.bounds.size())
    {
        const BoundsD& bounds = snapshot.bounds[index];
        out->bounds_min[0] = bounds[0];
        out->bounds_min[1] = bounds[1];
        out->bounds_max[0] = bounds[2];
        out->bounds_max[1] = bounds[3];
    }

    std::size_t ring_offset = 0;
    HsBaRing_t* slot_out[3] = {rings, rings, rings};
    for (int s = 0; s < 3; ++s)
    {
        const std::size_t slot_rings = slots[s]->size();
        slot_out[s] = rings + ring_offset;
        ring_offset += slot_rings;
        for (std::size_t r = 0; r < slot_rings; ++r)
        {
            const PolygonD& ring = (*slots[s])[r];
            HsBaRing_t& dst = slot_out[s][r];
            dst.point_count = static_cast<int>(ring.size());
            dst.xy = ring.empty() ? nullptr : xy;
            dst.closed = IsClosedRing(ring) ? 1 : 0;
            for (const Point2D& point : ring)
            {
                *xy++ = point.x;
                *xy++ = point.y;
            }
        }
    }
    out->outline_count = static_cast<int>(slots[0]->size());
    out->outlines = rings;
    out->fill_count = static_cast<int>(slots[1]->size());
    out->fills = slot_out[1];
    out->support_count = static_cast<int>(slots[2]->size());
    out->supports = slot_out[2];
    return 1;
}

int PathPointCount(long long geometry_id)
{
    if (geometry_id <= 0)
    {
        return 0;
    }
    const std::lock_guard<std::mutex> lock(PoolMutex());
    const auto it = Pool().find(geometry_id);
    if (it == Pool().end())
    {
        return 0;
    }
    return static_cast<int>(it->second.snapshot.path_xyz.size() / 3);
}

int ExportGeometryPath(long long geometry_id, HsBaGeometryPath_t* out)
{
    if (!out)
    {
        return 0;
    }
    std::memset(out, 0, sizeof(HsBaGeometryPath_t));
    if (geometry_id <= 0)
    {
        return 0;
    }

    const std::lock_guard<std::mutex> lock(PoolMutex());
    const auto it = Pool().find(geometry_id);
    if (it == Pool().end())
    {
        return 0;
    }
    // A trailing partial triple is not a point: only whole xyz triples are exported.
    const std::vector<double>& stored = it->second.snapshot.path_xyz;
    const std::size_t point_total = stored.size() / 3;
    if (point_total == 0)
    {
        return 0;
    }

    // One block holds every coordinate, so the caller frees it exactly once.
    const std::size_t xyz_bytes = point_total * 3 * sizeof(double);
    void* block = std::malloc(xyz_bytes + alignof(double));
    if (!block)
    {
        return 0;
    }
    double* xyz = static_cast<double*>(block);

    double bounds_min[2] = {0.0, 0.0};
    double bounds_max[2] = {0.0, 0.0};
    double z_min = 0.0;
    double z_max = 0.0;
    for (std::size_t i = 0; i < point_total; ++i)
    {
        const double x = stored[3 * i];
        const double y = stored[3 * i + 1];
        const double z = stored[3 * i + 2];
        xyz[3 * i] = x;
        xyz[3 * i + 1] = y;
        xyz[3 * i + 2] = z;
        if (i == 0)
        {
            bounds_min[0] = bounds_max[0] = x;
            bounds_min[1] = bounds_max[1] = y;
            z_min = z_max = z;
            continue;
        }
        bounds_min[0] = std::min(bounds_min[0], x);
        bounds_min[1] = std::min(bounds_min[1], y);
        bounds_max[0] = std::max(bounds_max[0], x);
        bounds_max[1] = std::max(bounds_max[1], y);
        z_min = std::min(z_min, z);
        z_max = std::max(z_max, z);
    }

    out->point_count = static_cast<int>(point_total);
    out->xyz = xyz;
    out->bounds_min[0] = bounds_min[0];
    out->bounds_min[1] = bounds_min[1];
    out->bounds_max[0] = bounds_max[0];
    out->bounds_max[1] = bounds_max[1];
    out->z_min = z_min;
    out->z_max = z_max;
    return 1;
}

long long RetainSnapshot(long long geometry_id)
{
    if (geometry_id <= 0)
    {
        return 0;
    }
    const std::lock_guard<std::mutex> lock(PoolMutex());
    const auto it = Pool().find(geometry_id);
    if (it == Pool().end())
    {
        return 0;
    }
    ++it->second.refs;
    return geometry_id;
}

void ReleaseSnapshot(long long geometry_id)
{
    if (geometry_id <= 0)
    {
        return;
    }
    const std::lock_guard<std::mutex> lock(PoolMutex());
    const auto it = Pool().find(geometry_id);
    if (it == Pool().end())
    {
        return;
    }
    if (--it->second.refs <= 0)
    {
        Pool().erase(it);
    }
}

}  // namespace HsBa::Slicer::Pipeline

/* ============================================================================
 *  C ABI forwarding layer
 * ========================================================================== */

extern "C"
{

    HSBA_SLICER_API int HsBaGeometryLayerCount(long long geometry_id)
    {
        try
        {
            return HsBa::Slicer::Pipeline::SnapshotLayerCount(geometry_id);
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
            return 0;
        }
    }

    HSBA_SLICER_API int HsBaGetLayerGeometry(long long geometry_id, int layer_index, HsBaLayerGeometry_t* out)
    {
        try
        {
            return HsBa::Slicer::Pipeline::ExportLayerGeometry(geometry_id, layer_index, out);
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
            return 0;
        }
    }

    HSBA_SLICER_API void HsBaFreeLayerGeometry(HsBaLayerGeometry_t* geom)
    {
        if (!geom)
        {
            return;
        }
        // The block starts at outlines, so freeing it releases the ring arrays and all coordinates.
        std::free(const_cast<HsBaRing_t*>(geom->outlines));
        std::memset(geom, 0, sizeof(HsBaLayerGeometry_t));
    }

    HSBA_SLICER_API int HsBaGeometryPathPointCount(long long geometry_id)
    {
        try
        {
            return HsBa::Slicer::Pipeline::PathPointCount(geometry_id);
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
            return 0;
        }
    }

    HSBA_SLICER_API int HsBaGetGeometryPath(long long geometry_id, HsBaGeometryPath_t* path)
    {
        try
        {
            return HsBa::Slicer::Pipeline::ExportGeometryPath(geometry_id, path);
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
            return 0;
        }
    }

    HSBA_SLICER_API void HsBaFreeGeometryPath(HsBaGeometryPath_t* path)
    {
        if (!path || !path->xyz)
        {
            return;
        }
        // xyz is the first and only section of the block handed out by HsBaGetGeometryPath.
        std::free(const_cast<double*>(path->xyz));
        std::memset(path, 0, sizeof(HsBaGeometryPath_t));
    }

    HSBA_SLICER_API long long HsBaRetainGeometrySnapshot(long long geometry_id)
    {
        try
        {
            return HsBa::Slicer::Pipeline::RetainSnapshot(geometry_id);
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
            return 0;
        }
    }

    HSBA_SLICER_API void HsBaReleaseGeometrySnapshot(long long geometry_id)
    {
        try
        {
            HsBa::Slicer::Pipeline::ReleaseSnapshot(geometry_id);
        }
        catch (const HsBa::Slicer::RuntimeError&)
        {
            // NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow, do not propagate across C boundary
        }
    }
}
