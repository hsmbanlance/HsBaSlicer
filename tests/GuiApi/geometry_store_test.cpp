/** @file geometry_store_test.cpp
 * @brief Snapshot pool contract of DllHsBaSlicer/geometry_store.cpp (LRU, references, ownership).
 *
 * The pool is a process-internal library, so this suite compiles the translation unit
 * into the test instead of reaching for a symbol the published C ABI does not export.
 * Capture still happens through the internal entry point and every read goes through the
 * exported HsBa* functions, which keeps the forwarding layer covered as well.
 * @author HsBa
 */
#include <cstdlib>
#include <vector>

#define BOOST_TEST_MODULE GuiApiGeometryStoreTest
#include <boost/test/included/unit_test.hpp>

#include "2D/FloatPolygons.hpp"
#include "DllHsBaSlicer/geometry_export.h"

#include "../../DllHsBaSlicer/geometry_store.hpp"

using HsBa::Slicer::PolygonD;
using HsBa::Slicer::PolygonsD;
using namespace HsBa::Slicer::Pipeline;

namespace
{
/**
 * @brief Bound the pool to a single snapshot before the first capture reads the limit.
 *
 * PoolMax() caches on first use, so the variable has to be in place before any test
 * captures; a global fixture is the only hook guaranteed to run that early.
 */
struct PoolLimitFixture
{
    PoolLimitFixture()
    {
#ifdef _WIN32
        _putenv_s("HSBA_GEOMETRY_POOL_MAX", "1");
#else
        setenv("HSBA_GEOMETRY_POOL_MAX", "1", 1);
#endif
    }
};

/** @brief One closed 10x10 contour, repeating its first vertex as this project does. */
PolygonD SquareRing(double offset)
{
    return PolygonD{{offset, offset},
                    {10.0 + offset, offset},
                    {10.0 + offset, 10.0 + offset},
                    {offset, 10.0 + offset},
                    {offset, offset}};
}

/** @brief One open two-point infill line. */
PolygonD OpenLine(double offset)
{
    return PolygonD{{offset, offset}, {10.0 + offset, 10.0 + offset}};
}

/** @brief A two-layer snapshot with a contour plus an infill line on layer 0 only. */
long long CaptureTwoLayerSample()
{
    const std::vector<PolygonsD> outlines = {PolygonsD{SquareRing(0.0)}, PolygonsD{SquareRing(1.0)}};
    const std::vector<PolygonsD> fills = {PolygonsD{OpenLine(2.0)}, PolygonsD{}};
    return CaptureLayerGeometry(HSBA_PIPELINE_FDM, outlines, fills, {}, {0.2, 0.4});
}
}  // namespace

BOOST_GLOBAL_FIXTURE(PoolLimitFixture);

BOOST_AUTO_TEST_SUITE(gui_geometry_store)

BOOST_AUTO_TEST_CASE(capture_and_query_round_trip)
{
    const long long id = CaptureTwoLayerSample();
    BOOST_REQUIRE_GT(id, 0);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(id), 2);

    HsBaLayerGeometry_t geom{};
    BOOST_REQUIRE_EQUAL(HsBaGetLayerGeometry(id, 0, &geom), 1);
    BOOST_CHECK_EQUAL(geom.layer_index, 0);
    BOOST_CHECK_EQUAL(geom.z_height, 0.2);
    BOOST_CHECK_EQUAL(geom.outline_count, 1);
    BOOST_CHECK_EQUAL(geom.fill_count, 1);
    BOOST_CHECK_EQUAL(geom.support_count, 0);
    BOOST_REQUIRE(geom.outlines);
    BOOST_REQUIRE(geom.fills);
    BOOST_CHECK_EQUAL(geom.outlines[0].point_count, 5);
    BOOST_CHECK_EQUAL(geom.outlines[0].closed, 1);
    BOOST_CHECK_EQUAL(geom.fills[0].point_count, 2);
    BOOST_CHECK_EQUAL(geom.fills[0].closed, 0);
    // Interleaved xy of the contour, and the documented length of 2 * point_count.
    const double* xy = geom.outlines[0].xy;
    BOOST_REQUIRE(xy);
    BOOST_CHECK_EQUAL(xy[0], 0.0);
    BOOST_CHECK_EQUAL(xy[1], 0.0);
    BOOST_CHECK_EQUAL(xy[8], 0.0);
    BOOST_CHECK_EQUAL(xy[9], 0.0);
    // Bounds cover every ring of the layer, so they reach the infill corner at 12.
    BOOST_CHECK_EQUAL(geom.bounds_min[0], 0.0);
    BOOST_CHECK_EQUAL(geom.bounds_min[1], 0.0);
    BOOST_CHECK_EQUAL(geom.bounds_max[0], 12.0);
    BOOST_CHECK_EQUAL(geom.bounds_max[1], 12.0);
    // The three arrays are consecutive sections of one block (see geometry_export.h).
    BOOST_CHECK(geom.fills == geom.outlines + geom.outline_count);
    BOOST_CHECK(geom.supports == geom.fills + geom.fill_count);
    HsBaFreeLayerGeometry(&geom);
    BOOST_CHECK_EQUAL(geom.outline_count, 0);
    BOOST_CHECK(!geom.outlines);
    BOOST_CHECK(!geom.fills);
    BOOST_CHECK(!geom.supports);

    // Layer 1 has neither an infill nor a support slot, which still reads as empty
    // arrays inside the block rather than as dangling or unmatched pointers.
    BOOST_REQUIRE_EQUAL(HsBaGetLayerGeometry(id, 1, &geom), 1);
    BOOST_CHECK_EQUAL(geom.fill_count, 0);
    BOOST_CHECK_EQUAL(geom.outline_count, 1);
    BOOST_CHECK_EQUAL(geom.bounds_min[0], 1.0);
    HsBaFreeLayerGeometry(&geom);

    HsBaReleaseGeometrySnapshot(id);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(id), 0);
}

BOOST_AUTO_TEST_CASE(unknown_ids_and_out_of_range_layers_read_as_failure)
{
    for (const long long bad : {0LL, -1LL, 987654321LL})
    {
        BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(bad), 0);
        BOOST_CHECK_EQUAL(HsBaRetainGeometrySnapshot(bad), 0);
        HsBaReleaseGeometrySnapshot(bad);  // must not throw across the C boundary
        HsBaLayerGeometry_t geom{};
        geom.layer_index = 7;  // a failed export has to clear the caller's struct
        BOOST_CHECK_EQUAL(HsBaGetLayerGeometry(bad, 0, &geom), 0);
        BOOST_CHECK_EQUAL(geom.layer_index, 0);
        BOOST_CHECK(!geom.outlines);
        BOOST_CHECK_EQUAL(HsBaGetLayerGeometry(bad, 0, nullptr), 0);
    }

    const long long id = CaptureTwoLayerSample();
    BOOST_REQUIRE_GT(id, 0);
    HsBaLayerGeometry_t geom{};
    BOOST_CHECK_EQUAL(HsBaGetLayerGeometry(id, 2, &geom), 0);
    BOOST_CHECK_EQUAL(HsBaGetLayerGeometry(id, -1, &geom), 0);
    HsBaFreeLayerGeometry(nullptr);
    HsBaFreeLayerGeometry(&geom);  // a struct that never got data is a no-op
    HsBaReleaseGeometrySnapshot(id);
}

BOOST_AUTO_TEST_CASE(pool_limit_recycles_the_oldest_snapshot)
{
    const long long first = CaptureTwoLayerSample();
    BOOST_REQUIRE_GT(first, 0);
    const long long second = CaptureTwoLayerSample();
    BOOST_REQUIRE_GT(second, 0);
    BOOST_CHECK_NE(first, second);
    // HSBA_GEOMETRY_POOL_MAX is 1 here, so capturing the second dropped the first.
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(first), 0);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(second), 2);
    HsBaReleaseGeometrySnapshot(second);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(second), 0);
}

BOOST_AUTO_TEST_CASE(retain_pins_a_snapshot_until_the_last_release)
{
    const long long pinned = CaptureTwoLayerSample();
    BOOST_REQUIRE_GT(pinned, 0);
    BOOST_CHECK_EQUAL(HsBaRetainGeometrySnapshot(pinned), pinned);

    // The pinned snapshot is not a legal victim, so the pool temporarily exceeds its
    // limit instead of recycling data a host is still previewing.
    const long long later = CaptureTwoLayerSample();
    BOOST_REQUIRE_GT(later, 0);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(pinned), 2);

    HsBaReleaseGeometrySnapshot(pinned);  // drops the host reference, the pool keeps its own
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(pinned), 2);
    HsBaReleaseGeometrySnapshot(pinned);  // the last reference frees it right away
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(pinned), 0);
    HsBaReleaseGeometrySnapshot(pinned);  // releasing a gone snapshot is harmless

    HsBaReleaseGeometrySnapshot(later);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(later), 0);
}

BOOST_AUTO_TEST_CASE(empty_capture_holds_nothing)
{
    // A run that produced no layer at all must not occupy pool capacity, and the
    // caller stores the returned 0 the same way it stores a real id.
    BOOST_CHECK_EQUAL(CaptureLayerGeometry(HSBA_PIPELINE_SLA, {}, {}, {}, {}), 0);
    BOOST_CHECK_EQUAL(HsBaGeometryLayerCount(0), 0);
}

BOOST_AUTO_TEST_SUITE_END()
