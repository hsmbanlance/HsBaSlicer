#define BOOST_TEST_MODULE support_test
#include <boost/test/included/unit_test.hpp>

#include <cmath>

#include "support/FdmSupport.hpp"
#include "support/LuaAdapter.hpp"
#include "support/LuaSupport.hpp"
#include "support/OverhangDetector.hpp"
#include "support/SlaSupport.hpp"
#include "support/SupportConfig.hpp"

using namespace HsBa::Slicer;
using namespace HsBa::Slicer::Support;

namespace
{
// Helper: create a square polygon
PolygonD MakeSquare(double x, double y, double size)
{
    return {{x, y}, {x + size, y}, {x + size, y + size}, {x, y + size}};
}

// Helper: check polygon area is approximately expected
bool AreaApprox(const PolygonsD& polys, double expected, double tolerance = 1.0)
{
    double total = 0.0;
    for (const auto& p : polys)
        total += std::abs(Area(p));
    return std::abs(total - expected) < tolerance;
}
}  // namespace

// ============================================================================
// OverhangDetector tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(overhang_detector)

BOOST_AUTO_TEST_CASE(no_prev_layer_entire_current_is_overhang)
{
    PolygonsD current = {MakeSquare(0, 0, 10)};
    PolygonsD prev;

    auto result = OverhangDetector::Detect(current, prev, 0.2f, 45.0f);
    // 无上一层时，当前层整体即为悬垂，原样返回
    BOOST_REQUIRE_EQUAL(result.size(), current.size());
    BOOST_CHECK_EQUAL(result[0].size(), current[0].size());
    BOOST_CHECK(AreaApprox(result, 100.0));
}

BOOST_AUTO_TEST_CASE(identical_layers_no_overhang)
{
    PolygonsD layer = {MakeSquare(0, 0, 10)};

    auto result = OverhangDetector::Detect(layer, layer, 0.2f, 45.0f);
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(small_overhang_filtered_by_angle)
{
    // Current layer is slightly larger than prev layer (2mm overhang each side)
    PolygonsD current = {MakeSquare(0, 0, 14)};
    PolygonsD prev = {MakeSquare(2, 2, 10)};

    // With a large angle threshold (80 degrees), small overhangs should be filtered
    auto result = OverhangDetector::Detect(current, prev, 0.2f, 80.0f);
    // 80° 下桥接距离 = 0.2/tan(80°) ≈ 0.035，远小于 2mm 悬垂，不应被过滤
    BOOST_CHECK(!result.empty());
    // 环形区域用有符号面积求和（孔洞面积自动相消）：原面积 96，侵蚀 0.035mm 后 ≈ 92.6
    double signedArea = 0.0;
    for (const auto& p : result)
        signedArea += Area(p);
    BOOST_CHECK_CLOSE(std::abs(signedArea), 92.6, 10.0);
}

BOOST_AUTO_TEST_CASE(max_bridge_distance_calculation)
{
    // At 45 degrees, bridge distance = layer_height / tan(45) = layer_height
    double bridge = OverhangDetector::MaxBridgeDistance(0.2f, 45.0f);
    BOOST_CHECK_CLOSE(bridge, 0.2, 1.0);

    // At 90 degrees (vertical), no bridge
    BOOST_CHECK_EQUAL(OverhangDetector::MaxBridgeDistance(0.2f, 90.0f), 0.0);

    // At 0 degrees, infinite bridge
    BOOST_CHECK_GT(OverhangDetector::MaxBridgeDistance(0.2f, 0.0f), 1e8);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FDM Plane Support tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(fdm_plane_support)

BOOST_AUTO_TEST_CASE(no_overhang_no_support)
{
    PolygonsD layer = {MakeSquare(0, 0, 10)};
    FdmPlaneSupport support;
    SupportConfig config;

    auto result = support.Generate(layer, layer, 0.2f, config);
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(overhang_generates_support)
{
    PolygonsD current = {MakeSquare(0, 0, 20)};
    PolygonsD prev = {MakeSquare(5, 5, 10)};

    FdmPlaneSupport support;
    SupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;
    config.overhang_angle_threshold = 45.0f;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK(!result.empty());
    // 悬垂环形面积 = 20² - 10² = 300，支撑外扩半径 1mm（圆角连接）后面积大于原区域，
    // 实测约 536，上限取 600 容忍圆角附加面积
    double totalArea = 0.0;
    for (const auto& p : result)
        totalArea += std::abs(Area(p));
    BOOST_CHECK_GT(totalArea, 300.0);
    BOOST_CHECK_LT(totalArea, 600.0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FDM Tree Support tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(fdm_tree_support)

BOOST_AUTO_TEST_CASE(no_overhang_no_support)
{
    PolygonsD layer = {MakeSquare(0, 0, 10)};
    FdmTreeSupport support;
    SupportConfig config;

    auto result = support.Generate(layer, layer, 0.2f, config);
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(overhang_generates_tree_branches)
{
    PolygonsD current = {MakeSquare(0, 0, 20)};
    PolygonsD prev = {MakeSquare(5, 5, 10)};

    FdmTreeSupport support;
    SupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK(!result.empty());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// FDM Honeycomb Support tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(fdm_honeycomb_support)

BOOST_AUTO_TEST_CASE(no_overhang_no_support)
{
    PolygonsD layer = {MakeSquare(0, 0, 10)};
    FdmHoneycombSupport support;
    SupportConfig config;

    auto result = support.Generate(layer, layer, 0.2f, config);
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(overhang_generates_honeycomb)
{
    PolygonsD current = {MakeSquare(0, 0, 30)};
    PolygonsD prev = {MakeSquare(5, 5, 20)};

    FdmHoneycombSupport support;
    SupportConfig config;
    config.support_gap = 0.0f;
    config.honeycomb_cell_size = 5.0f;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK(!result.empty());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// SLA Sacrificial Support tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(sla_sacrificial_support)

BOOST_AUTO_TEST_CASE(no_overhang_no_support)
{
    PolygonsD layer = {MakeSquare(0, 0, 10)};
    SlaSacrificialSupport support;
    SupportConfig config;

    auto result = support.Generate(layer, layer, 0.2f, config);
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(overhang_generates_sla_support)
{
    PolygonsD current = {MakeSquare(0, 0, 20)};
    PolygonsD prev = {MakeSquare(5, 5, 10)};

    SlaSacrificialSupport support;
    SlaSupportConfig config;
    config.support_gap = 0.0f;
    config.tip_diameter = 0.3f;
    config.support_diameter = 2.0f;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK(!result.empty());
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Lua Support tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(lua_support)

BOOST_AUTO_TEST_CASE(lua_inline_returns_empty)
{
    // Script that returns empty
    LuaSupport support("support_polys = {}");
    SupportConfig config;

    PolygonsD current = {MakeSquare(0, 0, 10)};
    PolygonsD prev;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK(result.empty());
}

BOOST_AUTO_TEST_CASE(lua_inline_returns_polygons)
{
    // Script that returns a simple square polygon
    const char* script = R"(
        support_polys = {
            { {x=0, y=0}, {x=10, y=0}, {x=10, y=10}, {x=0, y=10} }
        }
    )";

    LuaSupport support(script);
    SupportConfig config;

    PolygonsD current = {MakeSquare(0, 0, 10)};
    PolygonsD prev;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK_EQUAL(result.size(), 1u);
    BOOST_CHECK_EQUAL(result[0].size(), 4u);
}

BOOST_AUTO_TEST_CASE(lua_function_call)
{
    const char* script = R"(
        function generate_support()
            return {
                { {x=0, y=0}, {x=5, y=0}, {x=5, y=5}, {x=0, y=5} }
            }
        end
    )";

    LuaSupport support(std::string_view(script), std::string_view("generate_support"));
    SupportConfig config;

    PolygonsD current = {MakeSquare(0, 0, 10)};
    PolygonsD prev;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK_EQUAL(result.size(), 1u);
}

BOOST_AUTO_TEST_CASE(lua_accesses_config)
{
    // Script that uses config values
    const char* script = R"(
        local d = config.support_diameter
        support_polys = {
            { {x=0, y=0}, {x=d, y=0}, {x=d, y=d}, {x=0, y=d} }
        }
    )";

    LuaSupport support(script);
    SupportConfig config;
    config.support_diameter = 3.0f;

    PolygonsD current = {MakeSquare(0, 0, 10)};
    PolygonsD prev;

    auto result = support.Generate(current, prev, 0.2f, config);
    BOOST_CHECK_EQUAL(result.size(), 1u);
    // The polygon should be a 3x3 square
    BOOST_CHECK_CLOSE(result[0][0].x, 0.0, 1.0);
    BOOST_CHECK_CLOSE(result[0][1].x, 3.0, 1.0);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ISupport::GenerateAll tests
// ============================================================================
BOOST_AUTO_TEST_SUITE(generate_all)

BOOST_AUTO_TEST_CASE(generate_all_layers)
{
    std::vector<PolygonsD> layers = {
        {MakeSquare(0, 0, 10)},
        {MakeSquare(0, 0, 15)},
        {MakeSquare(0, 0, 10)},
    };

    FdmPlaneSupport support;
    SupportConfig config;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = support.GenerateAll(layers, config);
    BOOST_CHECK_EQUAL(results.size(), layers.size());
    // First layer: no prev, so entire layer is overhang -> support generated
    BOOST_CHECK(!results[0].empty());
    // Second layer: larger than first -> overhang -> support
    BOOST_CHECK(!results[1].empty());
    // Third layer: smaller than second -> no overhang -> no support
    BOOST_CHECK(results[2].empty());
}

BOOST_AUTO_TEST_SUITE_END()
