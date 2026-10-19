#define BOOST_TEST_MODULE fdm_support_lib_test
#include <boost/test/included/unit_test.hpp>

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
    auto honeyResult =
        Slicer::GenerateFdmSupport(SquareRing(0.0, 0.0, 30.0), SquareRing(5.0, 5.0, 20.0), 0.2f, honey);
    BOOST_CHECK(!honeyResult.empty());
}

// GenerateAllFdmSupport returns one support set per input layer.
BOOST_AUTO_TEST_CASE(generate_all_fdm_layers)
{
    std::vector<Slicer::PolygonsD> layers{SquareRing(0.0, 0.0, 10.0), SquareRing(0.0, 0.0, 15.0),
                                          SquareRing(0.0, 0.0, 10.0)};
    FdmSupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = Slicer::GenerateAllFdmSupport(layers, config);
    BOOST_CHECK_EQUAL(results.size(), layers.size());
    // first layer has no predecessor, so the whole footprint is an overhang
    BOOST_CHECK(!results[0].empty());
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
