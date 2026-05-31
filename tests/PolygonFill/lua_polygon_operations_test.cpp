#define BOOST_TEST_MODULE lua_polygon_operations_test
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "2D/LuaAdapter.hpp"

using namespace HsBa::Slicer;

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
