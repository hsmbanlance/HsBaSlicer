#define BOOST_TEST_MODULE fdm_support_lib_test
#include <boost/test/included/unit_test.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"

namespace Slicer = HsBa::Slicer;
using namespace HsBa::Slicer::Support;

namespace
{
// A single-square layer contour, matching the shapes the support generators use.
Slicer::PolygonsD SquareRing(double x, double y, double size)
{
    return {Slicer::MakeRectangle(x, y, size, size)};
}

// Bounding box of a support cross-section, used to follow one column through the stack.
struct Bounds
{
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;

    bool operator==(const Bounds& other) const
    {
        return std::abs(min_x - other.min_x) < 1e-9 && std::abs(min_y - other.min_y) < 1e-9 &&
               std::abs(max_x - other.max_x) < 1e-9 && std::abs(max_y - other.max_y) < 1e-9;
    }
};

Bounds BoundsOf(const Slicer::PolygonsD& polys)
{
    Bounds box{1e18, 1e18, -1e18, -1e18};
    for (const auto& ring : polys)
    {
        for (const auto& pt : ring)
        {
            box.min_x = std::min(box.min_x, pt.x);
            box.min_y = std::min(box.min_y, pt.y);
            box.max_x = std::max(box.max_x, pt.x);
            box.max_y = std::max(box.max_y, pt.y);
        }
    }
    return box;
}
}  // namespace

BOOST_AUTO_TEST_SUITE(fdm_support_lib)

// GenerateFdmSupport dispatches on config.support_pattern; exercise the Plane
// (default) branch and confirm an overhang produces support cross-sections.
BOOST_AUTO_TEST_CASE(generate_fdm_plane_pattern)
{
    auto current = SquareRing(0.0, 0.0, 20.0);
    auto prev = SquareRing(5.0, 5.0, 10.0);

    FdmSupportConfig config;  // support_pattern defaults to 0 (Plane)
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto result = Slicer::GenerateFdmSupport(current, prev, 0.2f, config);
    BOOST_CHECK(!result.empty());
}

// The Tree (1) and Honeycomb (2) dispatch branches.
BOOST_AUTO_TEST_CASE(generate_fdm_tree_and_honeycomb_patterns)
{
    FdmSupportConfig tree;
    tree.support_pattern = 1;
    tree.support_gap = 0.0f;
    tree.support_diameter = 2.0f;
    auto treeResult = Slicer::GenerateFdmSupport(SquareRing(0.0, 0.0, 20.0), SquareRing(5.0, 5.0, 10.0), 0.2f, tree);
    BOOST_CHECK(!treeResult.empty());

    FdmSupportConfig honey;
    honey.support_pattern = 2;
    honey.support_gap = 0.0f;
    honey.honeycomb_cell_size = 5.0f;
    auto honeyResult = Slicer::GenerateFdmSupport(SquareRing(0.0, 0.0, 30.0), SquareRing(5.0, 5.0, 20.0), 0.2f, honey);
    BOOST_CHECK(!honeyResult.empty());
}

// GenerateAllFdmSupport returns one support set per input layer, and the support of a layer
// lands on the layer BELOW the overhang: the widened middle layer is held up by layer 0, while
// layer 2 (back to the narrow footprint) and the top of the stack carry nothing.
BOOST_AUTO_TEST_CASE(generate_all_fdm_layers)
{
    std::vector<Slicer::PolygonsD> layers{SquareRing(0.0, 0.0, 10.0), SquareRing(0.0, 0.0, 15.0),
                                          SquareRing(0.0, 0.0, 10.0)};
    FdmSupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = Slicer::GenerateAllFdmSupport(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    BOOST_CHECK(!results[0].empty());
    BOOST_CHECK(results[1].empty());
    BOOST_CHECK(results[2].empty());
}

// A prism has no overhang anywhere: every layer sits on an identical one. The bottom layer in
// particular rests on the build plate and has no predecessor to be compared with, so treating
// "no previous layer" as "the whole footprint hangs" buries a support slab inside the part.
BOOST_AUTO_TEST_CASE(generate_all_prism_gets_no_support)
{
    std::vector<Slicer::PolygonsD> layers(4, SquareRing(0.0, 0.0, 10.0));
    FdmSupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    const auto results = Slicer::GenerateAllFdmSupport(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    for (std::size_t i = 0; i < results.size(); ++i)
    {
        BOOST_CHECK_MESSAGE(results[i].empty(), "layer " << i << " of a prism carries support");
    }
}

// An overhang is not an isolated cross-section at its own height: material has to be deposited
// on every layer below it until the column reaches the plate. Here the top layer sticks out 5mm
// all around a four-layer column, so four support layers must appear underneath it.
BOOST_AUTO_TEST_CASE(generate_all_overhang_column_reaches_the_plate)
{
    std::vector<Slicer::PolygonsD> layers(4, SquareRing(0.0, 0.0, 10.0));
    layers.push_back(SquareRing(-5.0, -5.0, 20.0));
    FdmSupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    const auto results = Slicer::GenerateAllFdmSupport(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    BOOST_CHECK_MESSAGE(results[4].empty(), "the overhanging layer supports itself instead of being supported");
    for (std::size_t i = 0; i < 4; ++i)
    {
        BOOST_CHECK_MESSAGE(!results[i].empty(), "support column stops above layer " << i);
    }

    // The column is carried downward as a region, so it must not drift or widen on the way: the
    // pattern cross-section repeats the same outline on every layer.
    const Bounds reference = BoundsOf(results[0]);
    for (std::size_t i = 1; i < 4; ++i)
    {
        BOOST_CHECK_MESSAGE(BoundsOf(results[i]) == reference, "support column changes shape at layer " << i);
    }
}

// The column may stop early where the model itself already carries the load: the wide slab on
// the plate takes the weight, so only the layer directly under the overhang needs support.
BOOST_AUTO_TEST_CASE(generate_all_column_stops_on_solid_model)
{
    std::vector<Slicer::PolygonsD> layers{SquareRing(-10.0, -10.0, 30.0), SquareRing(0.0, 0.0, 10.0),
                                          SquareRing(-5.0, -5.0, 20.0)};
    FdmSupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    const auto results = Slicer::GenerateAllFdmSupport(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    BOOST_CHECK(!results[1].empty());  // the shelf hangs over the narrow column, so it needs one support layer
    BOOST_CHECK(results[0].empty());   // one layer down the wide slab already carries the load
    BOOST_CHECK(results[2].empty());   // the overhanging layer is supported from below, not by itself
}

// GenerateAllSlaSupport drives the SLA sacrificial generator through the wrapper.
BOOST_AUTO_TEST_CASE(generate_all_sla_layers)
{
    std::vector<Slicer::PolygonsD> layers{SquareRing(0.0, 0.0, 20.0), SquareRing(5.0, 5.0, 10.0)};
    SlaSupportConfig config;
    config.support_gap = 0.0f;
    config.tip_diameter = 0.3f;
    config.support_diameter = 2.0f;

    auto results = Slicer::GenerateAllSlaSupport(layers, config);
    BOOST_CHECK_EQUAL(results.size(), layers.size());
}

// GenerateAllLuaSupport runs a user script per layer, wiring in the 2D/3D Lua
// functions; the fixed script returns one square for every layer.
BOOST_AUTO_TEST_CASE(generate_all_lua_layers)
{
    std::vector<Slicer::PolygonsD> layers{SquareRing(0.0, 0.0, 10.0), SquareRing(0.0, 0.0, 10.0)};
    SupportConfig config;

    const std::string script = R"(
        function generate_support()
            return { { {x=0, y=0}, {x=5, y=0}, {x=5, y=5}, {x=0, y=5} } }
        end
    )";

    auto results = Slicer::GenerateAllLuaSupport(layers, config, script, "generate_support");
    BOOST_CHECK_EQUAL(results.size(), layers.size());
    BOOST_CHECK_EQUAL(results[0].size(), 1u);
}

BOOST_AUTO_TEST_SUITE_END()
