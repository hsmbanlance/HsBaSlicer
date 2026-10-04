#define BOOST_TEST_MODULE spiral_path_test
#include <boost/test/included/unit_test.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"

using namespace HsBa::Slicer;

namespace
{
// Build a closed axis-aligned square loop; winding == true => CCW.
PolygonD Square(double side, bool ccw)
{
    PolygonD p{Point2D{0.0, 0.0}, Point2D{side, 0.0}, Point2D{side, side}, Point2D{0.0, side}};
    if (!ccw)
        std::reverse(p.begin(), p.end());
    return p;
}

// A prismatic wall: the same square outer contour on every layer.
std::vector<PolygonsD> PrismLayers(size_t n, double side)
{
    std::vector<PolygonsD> layers;
    layers.reserve(n);
    for (size_t i = 0; i < n; ++i)
        layers.push_back(PolygonsD{Square(side, true)});
    return layers;
}

std::vector<double> RangedZ(size_t n, double h, double start = 0.0)
{
    std::vector<double> zs(n);
    for (size_t i = 0; i < n; ++i)
        zs[i] = start + static_cast<double>(i) * h;
    return zs;
}
}  // namespace

BOOST_AUTO_TEST_CASE(empty_and_single_layer)
{
    // No layers -> empty path.
    BOOST_CHECK(SpiralizeOuterWall({}, {}).empty());

    // Mismatched sizes -> empty path (defensive).
    auto layers = PrismLayers(2, 10.0);
    BOOST_CHECK(SpiralizeOuterWall(layers, std::vector<double>{0.0}).empty());

    // A single layer cannot ramp: it is emitted as one closed loop at its own Z.
    auto one = PrismLayers(1, 10.0);
    auto path = SpiralizeOuterWall(one, RangedZ(1, 0.4));
    BOOST_REQUIRE_EQUAL(path.size(), 5u);  // 4 vertices + closing point
    for (const auto& pt : path)
        BOOST_CHECK_CLOSE(pt.z, 0.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(prismatic_wall_is_continuous_helix)
{
    const size_t n = 6;
    const double h = 0.4;
    const double side = 10.0;
    auto layers = PrismLayers(n, side);
    auto zs = RangedZ(n, h);
    auto path = SpiralizeOuterWall(layers, zs);

    // Each layer contributes 4 vertices + 1 closing point.
    BOOST_REQUIRE_EQUAL(path.size(), n * 5u);

    // 1) Z is monotonic non-decreasing across the whole path.
    for (size_t i = 1; i < path.size(); ++i)
        BOOST_CHECK_GE(path[i].z, path[i - 1].z - 1e-9);

    // 2) Z starts at the base and the interior ramps up to the top layer height.
    BOOST_CHECK_CLOSE(path.front().z, 0.0, 1e-9);
    BOOST_CHECK_CLOSE(path.back().z, (n - 1) * h, 1e-9);

    // 3) Ramp per revolution: at each loop boundary Z has advanced by exactly h.
    for (size_t li = 0; li < n; ++li)
    {
        const auto& loopStart = path[li * 5];
        BOOST_CHECK_CLOSE(loopStart.z, li * h, 1e-6);
    }

    // 4) Seam continuity: the closing point of every loop coincides (XY and Z)
    //    with the start of the next loop -> a single unbroken extrusion line.
    for (size_t li = 0; li + 1 < n; ++li)
    {
        const auto& loopEnd = path[li * 5 + 4];
        const auto& nextStart = path[(li + 1) * 5];
        BOOST_CHECK_CLOSE(loopEnd.x, nextStart.x, 1e-6);
        BOOST_CHECK_CLOSE(loopEnd.y, nextStart.y, 1e-6);
        BOOST_CHECK_CLOSE(loopEnd.z, nextStart.z, 1e-6);
    }

    // 5) The last loop is flat at its own height (nothing above to connect to).
    for (size_t k = 0; k <= 4; ++k)
        BOOST_CHECK_CLOSE(path[(n - 1) * 5 + k].z, (n - 1) * h, 1e-6);

    // 6) XY stays on the square outline (all coordinates within [0, side]).
    for (const auto& pt : path)
    {
        BOOST_CHECK_GE(pt.x, -1e-9);
        BOOST_CHECK_LE(pt.x, side + 1e-9);
        BOOST_CHECK_GE(pt.y, -1e-9);
        BOOST_CHECK_LE(pt.y, side + 1e-9);
    }
}

BOOST_AUTO_TEST_CASE(orientation_and_outer_loop_selection)
{
    // Input orientation (CW vs CCW) must not change the geometry of the result.
    const size_t n = 4;
    const double h = 0.5;
    const double side = 12.0;

    std::vector<PolygonsD> ccw_layers;
    std::vector<PolygonsD> cw_layers;
    for (size_t i = 0; i < n; ++i)
    {
        ccw_layers.push_back(PolygonsD{Square(side, true)});
        cw_layers.push_back(PolygonsD{Square(side, false)});
    }
    auto zs = RangedZ(n, h);
    auto p_ccw = SpiralizeOuterWall(ccw_layers, zs);
    auto p_cw = SpiralizeOuterWall(cw_layers, zs);

    // Same XY set and same per-revolution Z progression regardless of winding.
    BOOST_REQUIRE_EQUAL(p_ccw.size(), p_cw.size());
    for (size_t i = 0; i < p_ccw.size(); ++i)
        BOOST_CHECK_CLOSE(p_ccw[i].z, p_cw[i].z, 1e-6);

    // Outer loop selection: a layer carrying an outer square + a smaller inner
    // hole must spiralize the OUTER contour (largest |area|), not the hole.
    PolygonD hole{Point2D{2, 2}, Point2D{4, 2}, Point2D{4, 4}, Point2D{2, 4}};  // side 4, well inside
    std::vector<PolygonsD> layers;
    for (size_t i = 0; i < n; ++i)
        layers.push_back(PolygonsD{Square(side, true), hole});
    auto path = SpiralizeOuterWall(layers, zs);
    BOOST_REQUIRE_EQUAL(path.size(), n * 5u);
    // Points must reach the outer boundary (side 12), never clamp to the hole.
    double max_xy = 0.0;
    for (const auto& pt : path)
        max_xy = std::max({max_xy, std::abs(pt.x), std::abs(pt.y)});
    BOOST_CHECK_CLOSE(max_xy, side, 1e-6);
}

BOOST_AUTO_TEST_CASE(skips_layers_without_closed_contour)
{
    // Layers 0 and 2 have a square, layer 1 is empty -> only two revolutions,
    // with Z taken from the provided per-layer heights (0.0 and 0.8).
    std::vector<PolygonsD> layers;
    layers.push_back(PolygonsD{Square(10.0, true)});
    layers.push_back(PolygonsD{});  // no contour
    layers.push_back(PolygonsD{Square(10.0, true)});
    auto zs = RangedZ(3, 0.4);
    auto path = SpiralizeOuterWall(layers, zs);

    BOOST_REQUIRE_EQUAL(path.size(), 2u * 5u);
    BOOST_CHECK_CLOSE(path.front().z, 0.0, 1e-9);          // layer 0 start
    BOOST_CHECK_CLOSE(path[5].z, 0.8, 1e-6);               // layer 2 start = zs[2]
    BOOST_CHECK_CLOSE(path.back().z, 0.8, 1e-6);           // last loop flat at 0.8
}
