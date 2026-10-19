#define BOOST_TEST_MODULE lua_polygon_operations_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "2D/LuaAdapter.hpp"

using namespace HsBa::Slicer;

// This test verifies the SVG polygon dump utilities (DumpPolygon/DumpPolygons and
// their Lua bindings). The macro is propagated by HsBaSlicer2D's PUBLIC compile
// definition, so the whole test case only exists when polygon dump is enabled.
#ifdef HSBA_POLYGON_DUMP
BOOST_AUTO_TEST_CASE(lua_polygon_operation_dump)
{
    auto temp_dir = std::filesystem::temp_directory_path();
    auto dump_path1 = temp_dir / "lua_polygon_dump_test1.svg";
    auto dump_path2 = temp_dir / "lua_polygon_dump_test2.svg";
    std::error_code ec;
    std::filesystem::remove(dump_path1, ec);
    std::filesystem::remove(dump_path2, ec);

    lua_State* L = luaL_newstate();
    BOOST_REQUIRE(L != nullptr);
    luaL_openlibs(L);
    RegisterLuaPolygonOperations(L);

    const char* lua_code = R"(
local poly = { { { x = 0, y = 0 }, { x = 10, y = 0 }, { x = 10, y = 10 }, { x = 0, y = 10 } } }
PolygonOperations.dumpPolygon(poly[1], "__FILENAME1__")
PolygonOperations.dumpPolygons(poly, "__FILENAME2__")
local rect = PolygonOperations.makeRectangle(0, 0, 10, 5)
local circle = PolygonOperations.makeCircle(0, 0, 5, 16)
local poly3 = PolygonOperations.makeRegularPolygon(0, 0, 5, 5)
assert(type(rect) == "table")
assert(type(circle) == "table")
assert(type(poly3) == "table")
assert(#rect == 1)
assert(#circle == 1)
assert(#poly3 == 1)
)";
    std::string script = lua_code;
    std::string filename1 = dump_path1.string();
    std::string filename2 = dump_path2.string();
    auto EscapeForLua = [](const std::string& in)
    {
        std::string out;
        out.reserve(in.size() * 2);
        for (char c : in)
        {
            switch (c)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
            }
        }
        return out;
    };
    std::string lua_filename1 = EscapeForLua(filename1);
    std::string lua_filename2 = EscapeForLua(filename2);
    size_t pos = script.find("__FILENAME1__");
    while (pos != std::string::npos)
    {
        script.replace(pos, sizeof("__FILENAME1__") - 1, lua_filename1);
        pos = script.find("__FILENAME1__", pos + lua_filename1.size());
    }
    pos = script.find("__FILENAME2__");
    while (pos != std::string::npos)
    {
        script.replace(pos, sizeof("__FILENAME2__") - 1, lua_filename2);
        pos = script.find("__FILENAME2__", pos + lua_filename2.size());
    }

    int ret = luaL_dostring(L, script.c_str());
    if (ret != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        std::cerr << (message ? message : "Lua execution failed without error message") << std::endl;
        BOOST_CHECK(false);
    }

    lua_close(L);

    BOOST_CHECK(std::filesystem::exists(dump_path1));
    BOOST_CHECK(std::filesystem::exists(dump_path2));

    std::ifstream file1(dump_path1);
    BOOST_REQUIRE(file1.is_open());
    std::string content1((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    BOOST_CHECK(content1.find("integerization=") != std::string::npos);
    BOOST_CHECK(content1.find("DumpPolygon(Polygon)") != std::string::npos);

    std::ifstream file2(dump_path2);
    BOOST_REQUIRE(file2.is_open());
    std::string content2((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    BOOST_CHECK(content2.find("integerization=") != std::string::npos);
    BOOST_CHECK(content2.find("DumpPolygons(Polygons)") != std::string::npos);

    std::filesystem::remove(dump_path1, ec);
    std::filesystem::remove(dump_path2, ec);
}
#endif  // HSBA_POLYGON_DUMP

// Drive every boolean / hull / area / shape-factory binding registered by
// RegisterLuaPolygonOperations. This path is available regardless of the
// HSBA_POLYGON_DUMP macro. Note the two scaling conventions: union/intersection/
// difference/xor/area work on raw float polygon tables, whereas offsetOperation
// routes through the integerized path (coords * 1e6), so its delta is in integer
// units (1.0 mm == 1e6). textToPolygons is intentionally skipped: it needs a font
// file on disk (environment-dependent).
BOOST_AUTO_TEST_CASE(lua_boolean_hull_area_factories)
{
    lua_State* L = luaL_newstate();
    BOOST_REQUIRE(L != nullptr);
    luaL_openlibs(L);
    RegisterLuaPolygonOperations(L);

    const char* script = R"lua(
local function areaof(polys)
    local s = 0
    for _, poly in ipairs(polys) do s = s + PolygonOperations.area(poly) end
    return math.abs(s)
end
local a = { { {x = 0, y = 0}, {x = 10, y = 0}, {x = 10, y = 10}, {x = 0, y = 10} } }
local b = { { {x = 5, y = 0}, {x = 15, y = 0}, {x = 15, y = 10}, {x = 5, y = 10} } }

-- area of a single polygon table
assert(math.abs(PolygonOperations.area(a[1]) - 100) < 1e-6)

-- dedicated boolean bindings
assert(math.abs(areaof(PolygonOperations.union(a, b)) - 150) < 1e-6)
assert(math.abs(areaof(PolygonOperations.intersection(a, b)) - 50) < 1e-6)
assert(math.abs(areaof(PolygonOperations.difference(a, b)) - 50) < 1e-6)
assert(math.abs(areaof(PolygonOperations.xor(a, b)) - 100) < 1e-6)

-- generic dispatcher covers all four operation-name branches
assert(math.abs(areaof(PolygonOperations.booleanOperation(a, b, "union")) - 150) < 1e-6)
assert(math.abs(areaof(PolygonOperations.booleanOperation(a, b, "intersection")) - 50) < 1e-6)
assert(math.abs(areaof(PolygonOperations.booleanOperation(a, b, "difference")) - 50) < 1e-6)
assert(math.abs(areaof(PolygonOperations.booleanOperation(a, b, "xor")) - 100) < 1e-6)

-- offset grows/shrinks by 1.0 unit == 1e6 in the integerized coordinate space
assert(areaof(PolygonOperations.offsetOperation(a, 1000000)) > 120)
assert(areaof(PolygonOperations.offsetOperation(a, -1000000)) < 80)

-- hulls
local hull = PolygonOperations.convexHullOperation(a)
assert(#hull == 1 and #hull[1] >= 4)
local ch = PolygonOperations.concaveHullOperation(a, 3)
assert(#ch == 1 and #ch[1] >= 3)

-- shape factories (each returns a single-element PolygonsD)
assert(#PolygonOperations.makeRectangle(0, 0, 10, 5) == 1)
assert(#PolygonOperations.makeCircle(0, 0, 5, 16) == 1)
assert(#PolygonOperations.makeEllipse(0, 0, 5, 3, 16) == 1)
assert(#PolygonOperations.makeRegularPolygon(0, 0, 5, 5) == 1)

-- argument-validation branches surface as Lua errors (caught via pcall)
assert(not pcall(function() return PolygonOperations.area(123) end))
assert(not pcall(function() return PolygonOperations.union(a, "x") end))
assert(not pcall(function() return PolygonOperations.offsetOperation(a, "x") end))
assert(not pcall(function() return PolygonOperations.booleanOperation(a, b, "nonsense") end))
)lua";

    int ret = luaL_dostring(L, script);
    if (ret != LUA_OK)
    {
        const char* message = lua_tostring(L, -1);
        std::cerr << (message ? message : "Lua execution failed without error message") << std::endl;
        BOOST_CHECK(false);
    }
    lua_close(L);
}
