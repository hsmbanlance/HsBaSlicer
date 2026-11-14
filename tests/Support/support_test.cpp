#define BOOST_TEST_MODULE support_test
#include <boost/test/included/unit_test.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

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

// Helper: axis aligned box around a polygon set, used to pin where a support column lands
struct Box
{
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
};

Box BoundsOf(const PolygonsD& polys)
{
    Box box{1e18, 1e18, -1e18, -1e18};
    for (const auto& poly : polys)
    {
        for (const auto& pt : poly)
        {
            box.min_x = std::min(box.min_x, pt.x);
            box.min_y = std::min(box.min_y, pt.y);
            box.max_x = std::max(box.max_x, pt.x);
            box.max_y = std::max(box.max_y, pt.y);
        }
    }
    return box;
}

bool SameBox(const Box& box, double min_x, double min_y, double max_x, double max_y)
{
    const double tol = 1e-6;
    return std::abs(box.min_x - min_x) < tol && std::abs(box.min_y - min_y) < tol &&
           std::abs(box.max_x - max_x) < tol && std::abs(box.max_y - max_y) < tol;
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

// A support is not an answer on its own layer: the region that layer i leaves unsupported has
// to be re-deposited on layer i-1, and so on down, until solid model carries it. So the ring
// that layer 1 sticks out beyond layer 0 is supported *inside layer 0*, and the overhang never
// spawns a slab on the layer that creates it.
// layer_height 0 disables the bridging erosion, which keeps every expected coordinate exact.
BOOST_AUTO_TEST_CASE(generate_all_layers)
{
    std::vector<PolygonsD> layers = {
        {MakeSquare(2, 2, 10)},  // [2,12]
        {MakeSquare(0, 0, 15)},  // [0,15] -> overhangs as the ring between [2,12] and [0,15]
        {MakeSquare(3, 3, 9)},   // [3,12] -> inside layer 1, nothing to support
    };

    FdmPlaneSupport support;
    SupportConfig config;
    config.layer_height = 0.0f;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = support.GenerateAll(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    // Layer 0 carries the column for layer 1's overhang: the ring [2,12]..[0,15] grown by the
    // column radius 1 -> outer [-1,16], hole [1,13], so two rings come back.
    BOOST_REQUIRE_EQUAL(results[0].size(), 2u);
    BOOST_CHECK_MESSAGE(SameBox(BoundsOf(results[0]), -1.0, -1.0, 16.0, 16.0), "layer 0 support box");
    // Layer 1 is where the overhang appears, but its support belongs one layer down.
    BOOST_CHECK(results[1].empty());
    // Layer 2 is smaller than layer 1, so it needs nothing.
    BOOST_CHECK(results[2].empty());
}

// Regression for the bottom-layer misjudgement: Detect(current, {}) treats the whole footprint
// as unsupported, which used to bury a support slab inside the first slice. A straight prism
// rests on the plate everywhere, so no layer may receive support.
BOOST_AUTO_TEST_CASE(generate_all_prism_gets_no_support_at_the_plate)
{
    std::vector<PolygonsD> layers = {
        {MakeSquare(0, 0, 10)},
        {MakeSquare(0, 0, 10)},
        {MakeSquare(0, 0, 10)},
    };

    FdmPlaneSupport support;
    SupportConfig config;
    config.layer_height = 0.0f;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = support.GenerateAll(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    BOOST_CHECK(results[0].empty());
    BOOST_CHECK(results[1].empty());
    BOOST_CHECK(results[2].empty());
}

// The column stops the moment a layer below can carry the whole region: the wide bottom plate
// covers the ring, so the support is written on layer 1 only and nothing reaches the plate.
BOOST_AUTO_TEST_CASE(generate_all_column_stops_on_solid_model)
{
    std::vector<PolygonsD> layers = {
        {MakeSquare(0, 0, 20)},  // [0,20] wide enough to carry everything above
        {MakeSquare(5, 5, 10)},  // [5,15]
        {MakeSquare(0, 0, 20)},  // [0,20] again -> ring [5,15]..[0,20] overhangs layer 1
    };

    FdmPlaneSupport support;
    SupportConfig config;
    config.layer_height = 0.0f;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = support.GenerateAll(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    BOOST_CHECK(results[2].empty());
    // Layer 1 holds the column for layer 2: ring grown by radius 1 -> outer [-1,21], hole [4,16].
    BOOST_REQUIRE_EQUAL(results[1].size(), 2u);
    BOOST_CHECK_MESSAGE(SameBox(BoundsOf(results[1]), -1.0, -1.0, 21.0, 21.0), "layer 1 support box");
    // Layer 0 is solid under the whole region, so the column does not continue downward.
    BOOST_CHECK(results[0].empty());
}

// While nothing below can carry the region it keeps travelling down to the plate: the middle
// layer is narrow, so layer 2's overhang is supported on layer 1 *and* on layer 0, unchanged.
BOOST_AUTO_TEST_CASE(generate_all_column_reaches_the_plate)
{
    std::vector<PolygonsD> layers = {
        {MakeSquare(5, 5, 10)},  // [5,15]
        {MakeSquare(5, 5, 10)},  // [5,15] identical -> no new overhang, but no carrier either
        {MakeSquare(0, 0, 20)},  // [0,20] ring [5,15]..[0,20]
    };

    FdmPlaneSupport support;
    SupportConfig config;
    config.layer_height = 0.0f;
    config.support_gap = 0.0f;
    config.support_diameter = 2.0f;

    auto results = support.GenerateAll(layers, config);
    BOOST_REQUIRE_EQUAL(results.size(), layers.size());
    BOOST_CHECK(results[2].empty());
    BOOST_REQUIRE_EQUAL(results[1].size(), 2u);
    BOOST_REQUIRE_EQUAL(results[0].size(), 2u);
    const Box upper = BoundsOf(results[1]);
    const Box lower = BoundsOf(results[0]);
    BOOST_CHECK_MESSAGE(SameBox(lower, upper.min_x, upper.min_y, upper.max_x, upper.max_y),
                        "support column changes shape on its way down");
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// Support Lua bindings (support/LuaAdapter.cpp) tests
// ============================================================================
namespace
{
bool LuaBoolGlobal(lua_State* L, const char* name)
{
    lua_getglobal(L, name);
    bool v = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return v;
}

void MustRunLua(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != LUA_OK)
        BOOST_FAIL("lua dostring failed: " << lua_tostring(L, -1));
}
}  // namespace

BOOST_AUTO_TEST_SUITE(support_lua_bindings)

// Register the Support table and drive every factory / generate / detect binding.
BOOST_AUTO_TEST_CASE(register_and_drive_support_bindings)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    HsBa::Slicer::Support::RegisterLuaSupport(L);

    const char* script = R"lua(
        local cur  = { { {x=0,y=0}, {x=20,y=0}, {x=20,y=20}, {x=0,y=20} } }
        local prev = { { {x=5,y=5}, {x=15,y=5}, {x=15,y=15}, {x=5,y=15} } }
        local cfg  = { overhang_angle_threshold=45, layer_height=0.2, support_gap=0,
                       support_diameter=2, support_density=0.2, tree_branch_angle=45,
                       tree_max_branch_radius=5, honeycomb_cell_size=5 }

        local c = Support.default_config()
        _G.ok_cfg = (type(c) == 'table' and c.layer_height ~= nil and c.support_pattern ~= nil)

        _G.ok_plane = (type(Support.generate(Support.new_plane(), cur, prev, 0.2, cfg)) == 'table')
        _G.ok_tree  = (type(Support.generate(Support.new_tree(), cur, prev, 0.2, cfg)) == 'table')
        _G.ok_hc    = (type(Support.generate(Support.new_honeycomb(), cur, prev, 0.2, cfg)) == 'table')
        _G.ok_sla   = (type(Support.generate(Support.new_sla(), cur, prev, 0.2, cfg)) == 'table')

        _G.ok_overhang = (type(Support.detect_overhang(cur, prev, 0.2, 45.0)) == 'table')

        -- inline lua support with an explicit function name
        local s1 = Support.new_lua(
            'function generate_support() return { { {x=0,y=0},{x=5,y=0},{x=5,y=5},{x=0,y=5} } } end',
            'generate_support')
        local r1 = Support.generate(s1, cur, prev, 0.2, cfg)
        _G.ok_lua_fn = (type(r1) == 'table' and #r1 >= 1)

        -- single-argument form exercises the default funcName and the support_polys fallback
        local s2 = Support.new_lua('support_polys = { { {x=0,y=0},{x=2,y=0},{x=2,y=2} } }')
        _G.ok_lua_default = (type(Support.generate(s2, cur, prev, 0.2, cfg)) == 'table')

        -- an empty config table drives the "field is not a number" defaults
        _G.ok_empty_cfg = (type(Support.generate(Support.new_plane(), cur, prev, 0.2, {})) == 'table')

        -- a non-table config argument drives ReadConfigFromTable's early default return
        _G.ok_nonintable_cfg = (type(Support.generate(Support.new_plane(), cur, prev, 0.2, 5)) == 'table')
    )lua";

    MustRunLua(L, script);
    BOOST_CHECK(LuaBoolGlobal(L, "ok_cfg"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_plane"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_tree"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_hc"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_sla"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_overhang"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_lua_fn"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_lua_default"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_empty_cfg"));
    BOOST_CHECK(LuaBoolGlobal(L, "ok_nonintable_cfg"));

    lua_close(L);
}

// Argument-validation error branches across the bindings.
BOOST_AUTO_TEST_CASE(support_lua_error_branches)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    HsBa::Slicer::Support::RegisterLuaSupport(L);

    const char* script = R"lua(
        _G.err_gen    = not pcall(function() return Support.generate(42, {}, {}, 0.2, {}) end)
        _G.err_newlua = not pcall(function() return Support.new_lua() end)
        _G.err_newfile = not pcall(function() return Support.new_lua_file(1) end)
    )lua";

    MustRunLua(L, script);
    BOOST_CHECK(LuaBoolGlobal(L, "err_gen"));
    BOOST_CHECK(LuaBoolGlobal(L, "err_newlua"));
    BOOST_CHECK(LuaBoolGlobal(L, "err_newfile"));

    lua_close(L);
}

// Support.new_lua_file(path, funcName) driven from a real temporary lua file.
BOOST_AUTO_TEST_CASE(support_lua_file_binding)
{
    const auto path = std::filesystem::temp_directory_path() / "hsba_support_gen.lua";
    {
        std::ofstream f(path);
        f << "function gen()\n return { { {x=0,y=0},{x=3,y=0},{x=3,y=3},{x=0,y=3} } }\n end\n";
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    HsBa::Slicer::Support::RegisterLuaSupport(L);
    lua_pushstring(L, path.string().c_str());
    lua_setglobal(L, "support_file");

    const char* script = R"lua(
        local obj  = Support.new_lua_file(support_file, 'gen')
        local cur  = { { {x=0,y=0}, {x=20,y=0}, {x=20,y=20}, {x=0,y=20} } }
        local prev = { { {x=5,y=5}, {x=15,y=5}, {x=15,y=15}, {x=5,y=15} } }
        local r = Support.generate(obj, cur, prev, 0.2, { support_diameter=2, support_gap=0 })
        _G.ok_file = (type(r) == 'table')
    )lua";

    MustRunLua(L, script);
    BOOST_CHECK(LuaBoolGlobal(L, "ok_file"));

    lua_close(L);
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_SUITE_END()
