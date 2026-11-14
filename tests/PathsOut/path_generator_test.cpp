#define BOOST_TEST_MODULE path_generator_test
#include <boost/test/included/unit_test.hpp>

#include <cstddef>
#include <string>
#include <string_view>
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
    Slicer::PolygonD ring{Slicer::Point2D{0.0, 0.0}, Slicer::Point2D{size, 0.0}, Slicer::Point2D{size, size},
                          Slicer::Point2D{0.0, size}};
    return {ring};
}

// Count where a substring appears; the G-code assertions below are all about ordering and
// occurrences, so they need to count rather than merely find.
std::size_t CountOccurrences(const std::string& text, std::string_view needle)
{
    std::size_t count = 0;
    for (std::size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + 1))
    {
        ++count;
    }
    return count;
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

// A helix alone leaves the bottom of the object open: every layer is a single ring, so the
// "vase" has no floor. Requesting a solid base must print the leading layers as normal
// layers (contour + fill), and those have to reach the plate *before* the wall starts rising.
BOOST_AUTO_TEST_CASE(generate_gcode_path_spiral_with_solid_base)
{
    std::vector<Slicer::PolygonsD> outlines(5, SquareContour());
    std::vector<double> zs{0.0, 0.2, 0.4, 0.6, 0.8};
    const std::vector<Slicer::PolygonsD> fills(5, Slicer::PolygonsD{Slicer::PolygonD{Slicer::Point2D{2.0, 2.0},
                                                                                    Slicer::Point2D{8.0, 2.0}}});

    Slicer::GCodePrinterConfig printer;
    auto path = Slicer::GenerateGCodePathSpiral(outlines, zs, printer, 2, fills);
    BOOST_REQUIRE(path != nullptr);
    BOOST_REQUIRE(path->hasContinuousWall());
    BOOST_CHECK(path->continuousWallPlacement() == Slicer::ContinuousWallPlacement::AfterLayers);

    const std::string gcode = path->ToGCode(Slicer::GCodeFirmware::Marlin);
    // Only the base layers are written as layers; the remaining three become the helix.
    BOOST_CHECK_EQUAL(CountOccurrences(gcode, ";LAYER:"), 2u);
    const auto last_base = gcode.find(";LAYER:1");
    BOOST_REQUIRE(last_base != std::string::npos);

    // The seam of the helix is the lowest-left vertex of the base square, so this rapid move
    // is unique to the wall: at Z 0.400 it can only be the first point of the helix, and it
    // must sit after the last solid layer rather than at the head of the file.
    const auto helix_start = gcode.find("G0 X0.000 Y0.000 Z0.400 ");
    BOOST_REQUIRE(helix_start != std::string::npos);
    BOOST_CHECK_GT(helix_start, last_base);

    // The base is filled, not just ringed, otherwise there is still no floor to print on.
    BOOST_CHECK(gcode.find("G0 X2.000 Y2.000 ") != std::string::npos);
}

// Zero regression for the pure vase mode: leaving solid_bottom_layers at its default must not
// grow a single layer block, and must be byte-identical to an explicit request for no base.
BOOST_AUTO_TEST_CASE(generate_gcode_path_spiral_without_base_is_pure_vase)
{
    std::vector<Slicer::PolygonsD> outlines(3, SquareContour());
    std::vector<double> zs{0.0, 0.2, 0.4};
    const std::vector<Slicer::PolygonsD> fills(3, Slicer::PolygonsD{Slicer::PolygonD{Slicer::Point2D{2.0, 2.0},
                                                                                    Slicer::Point2D{8.0, 2.0}}});

    Slicer::GCodePrinterConfig printer;
    auto vase = Slicer::GenerateGCodePathSpiral(outlines, zs, printer);
    BOOST_REQUIRE(vase != nullptr);
    BOOST_CHECK(vase->continuousWallPlacement() == Slicer::ContinuousWallPlacement::BeforeLayers);
    const std::string gcode = vase->ToGCode(Slicer::GCodeFirmware::Marlin);
    BOOST_CHECK_EQUAL(CountOccurrences(gcode, ";LAYER:"), 0u);
    // The helix starts at the very first layer of the stack.
    BOOST_CHECK(gcode.find("G0 X0.000 Y0.000 Z0.000 ") != std::string::npos);

    auto same = Slicer::GenerateGCodePathSpiral(outlines, zs, printer, 0, fills);
    BOOST_REQUIRE(same != nullptr);
    BOOST_CHECK_EQUAL(same->ToGCode(Slicer::GCodeFirmware::Marlin), gcode);
}

// A base covering the whole stack degenerates to ordinary per-layer output: nothing is left to
// spiralize, and the empty wall must not turn into a stray rapid move at Z 0.
BOOST_AUTO_TEST_CASE(generate_gcode_path_spiral_fully_solid_has_no_wall)
{
    std::vector<Slicer::PolygonsD> outlines(3, SquareContour());
    std::vector<double> zs{0.0, 0.2, 0.4};

    Slicer::GCodePrinterConfig printer;
    auto path = Slicer::GenerateGCodePathSpiral(outlines, zs, printer, outlines.size(), {});
    BOOST_REQUIRE(path != nullptr);
    BOOST_CHECK(!path->hasContinuousWall());

    const std::string gcode = path->ToGCode(Slicer::GCodeFirmware::Marlin);
    BOOST_CHECK_EQUAL(CountOccurrences(gcode, ";LAYER:"), 3u);
    BOOST_CHECK_EQUAL(CountOccurrences(gcode, "G0 X0.000 Y0.000 Z"), 0u);
}

BOOST_AUTO_TEST_SUITE_END()
