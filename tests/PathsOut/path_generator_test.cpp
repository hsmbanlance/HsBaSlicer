#define BOOST_TEST_MODULE path_generator_test
#include <boost/test/included/unit_test.hpp>

#include <string>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "paths/gcodepath.hpp"
#include "paths/pointspath.hpp"

namespace Slicer = HsBa::Slicer;

namespace
{
// A square ring used as a single per-layer contour. It is stored as an *open*
// vertex loop (PolygonsToGPoints closes it implicitly via wrap-around), so every
// emitted segment is non-degenerate and carries positive extrusion.
Slicer::PolygonsD SquareContour(double size = 10.0)
{
    Slicer::PolygonD ring{
        Slicer::Point2D{0.0, 0.0}, Slicer::Point2D{size, 0.0}, Slicer::Point2D{size, size}, Slicer::Point2D{0.0, size}};
    return {ring};
}
}  // namespace

BOOST_AUTO_TEST_SUITE(path_generator)

// PolygonsToGPoints emits one G0 travel to the ring start, then one G1 per vertex;
// printing (is_extrude) fills extrusion and print speed, traveling zeroes them.
BOOST_AUTO_TEST_CASE(polygons_to_gpoints_extrude)
{
    const auto rect = SquareContour()[0];  // open 4-vertex ring
    Slicer::PolygonsD polys{rect};
    Slicer::FdmPathConfig config;  // print_speed 50, travel_speed 100

    auto points = Slicer::PolygonsToGPoints(polys, 0.3f, config, true);

    // 1 leading travel + one move per vertex
    BOOST_REQUIRE_EQUAL(points.size(), rect.size() + 1);
    BOOST_CHECK(points[0].type == Slicer::GcodeType::G0);
    BOOST_CHECK_EQUAL(points[0].extrusion, 0.0);
    BOOST_CHECK_EQUAL(points[0].velocity, config.travel_speed);

    for (size_t i = 1; i < points.size(); ++i)
    {
        BOOST_CHECK(points[i].type == Slicer::GcodeType::G1);
        BOOST_CHECK_EQUAL(points[i].velocity, config.print_speed);
        BOOST_CHECK_GT(points[i].extrusion, 0.0);
        BOOST_CHECK_EQUAL(points[i].p1.z, 0.3f);
    }
}

BOOST_AUTO_TEST_CASE(polygons_to_gpoints_travel)
{
    Slicer::PolygonsD polys{Slicer::MakeRectangle(0.0, 0.0, 5.0, 5.0)};
    Slicer::FdmPathConfig config;

    auto points = Slicer::PolygonsToGPoints(polys, 1.0f, config, false);
    BOOST_REQUIRE(!points.empty());
    for (const auto& pt : points)
    {
        BOOST_CHECK_EQUAL(pt.extrusion, 0.0);
        BOOST_CHECK_EQUAL(pt.velocity, config.travel_speed);
    }
}

// Empty rings are skipped, so an all-empty input yields no points.
BOOST_AUTO_TEST_CASE(polygons_to_gpoints_skips_empty)
{
    Slicer::PolygonsD polys{Slicer::PolygonD{}};  // one empty contour
    Slicer::FdmPathConfig config;
    auto points = Slicer::PolygonsToGPoints(polys, 0.0f, config, true);
    BOOST_CHECK(points.empty());
}

// GenerateGCodePath stacks outlines + fills + supports for every layer into a PointsPath.
BOOST_AUTO_TEST_CASE(generate_gcode_path_builds_points_path)
{
    std::vector<Slicer::LayerPathData> layers(2);
    layers[0].outlines = SquareContour();
    layers[0].fills = SquareContour(5.0);
    layers[0].z_height = 0.0f;
    layers[1].supports = SquareContour();
    layers[1].z_height = 0.2f;

    Slicer::FdmPathConfig config;
    auto path = Slicer::GenerateGCodePath(layers, config);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK(!path->ToString().empty());
}

// GenerateGCodePathV2 produces a per-layer GCodePath (no continuous wall).
BOOST_AUTO_TEST_CASE(generate_gcode_path_v2_per_layer)
{
    std::vector<Slicer::LayerPathData> layers(1);
    layers[0].outlines = SquareContour();
    layers[0].z_height = 0.4f;

    Slicer::FdmPathConfig config;
    Slicer::GCodePrinterConfig printer;
    auto path = Slicer::GenerateGCodePathV2(layers, config, printer);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK(!path->hasContinuousWall());
    BOOST_CHECK(!path->ToGCode(Slicer::GCodeFirmware::Marlin).empty());
}

// GenerateGCodePathSpiral merges per-layer outer contours into one rising wall.
BOOST_AUTO_TEST_CASE(generate_gcode_path_spiral_continuous_wall)
{
    std::vector<Slicer::PolygonsD> outlines{SquareContour(), SquareContour()};
    std::vector<double> zs{0.0, 0.4};

    Slicer::GCodePrinterConfig printer;
    auto path = Slicer::GenerateGCodePathSpiral(outlines, zs, printer);
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK(path->hasContinuousWall());
    BOOST_CHECK(!path->ToGCode(Slicer::GCodeFirmware::Marlin).empty());
}

BOOST_AUTO_TEST_SUITE_END()
