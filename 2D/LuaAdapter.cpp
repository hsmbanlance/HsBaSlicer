/** @file LuaAdapter.cpp
 * @brief Implementations of Lua <-> polygon stack conversion and registration of polygon operations.
 * @author HsBa
 */
#include "LuaAdapter.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>

#include "2Dhull.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer
{
namespace
{
/** @brief Raises a Lua error carrying "file:line: Error in Lua function 'fname': msg".
 *
 *  lua_error longjmps, and the two platforms then disagree: MSVC x64 runs the C++
 *  destructors of every skipped frame during the unwind, POSIX longjmp does not. So a
 *  message object destroyed in a nested scope would be freed twice on Windows, while one
 *  still alive at the raise would leak on Linux. The text is therefore formatted into a
 *  trivially destructible buffer, copied into Lua's own memory, and nothing needs cleanup.
 *  @param quoted_detail if not empty, appended to the message as `'quoted_detail'`.
 */
void l_booleanError(lua_State* L, std::string_view fname, std::string_view msg, std::string_view quoted_detail = {})
{
    std::array<char, 512> buffer{};
    // replicate luaL_error: position prefix, then the message, then raise
    const auto formatted =
        quoted_detail.empty()
            ? std::format_to_n(buffer.data(), buffer.size() - 1, "Error in Lua function '{}': {}", fname, msg)
            : std::format_to_n(buffer.data(), buffer.size() - 1, "Error in Lua function '{}': {} '{}'", fname, msg,
                               quoted_detail);
    const size_t length = std::min(static_cast<size_t>(formatted.size), buffer.size() - 1);
    luaL_where(L, 1);
    lua_pushlstring(L, buffer.data(), length);
    lua_concat(L, 2);
    lua_error(L);
}


int l_booleanOperation(lua_State* L)
{
    if (lua_gettop(L) != 3 || !lua_istable(L, 1) || !lua_istable(L, 2) || !lua_isstring(L, 3))
    {
        l_booleanError(L, "booleanOperation", "Expected two polygon tables and a string operation name");
    }
    // Resolve the operation before converting any polygon: an unknown name raised afterwards
    // would longjmp past the live PolygonsD objects below and leak their buffers.
    std::string_view const operation = lua_tostring(L, 3);
    if (operation != "union" && operation != "intersection" && operation != "difference" && operation != "xor")
    {
        l_booleanError(L, "booleanOperation", "Unknown operation", operation);
    }
    PolygonsD const left = LuaTableToPolygonsD(L, 1);
    PolygonsD const right = LuaTableToPolygonsD(L, 2);
    PolygonsD result;
    if (operation == "intersection")
    {
        result = Intersection(left, right);
    }
    else if (operation == "difference")
    {
        result = Difference(left, right);
    }
    else if (operation == "xor")
    {
        result = Xor(left, right);
    }
    else  // "union", validated above
    {
        result = Union(left, right);
    }
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_union(lua_State* L)
{
    if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
    {
        l_booleanError(L, "union", "Expected two polygon tables");
    }
    PolygonsD const left = LuaTableToPolygonsD(L, 1);
    PolygonsD const right = LuaTableToPolygonsD(L, 2);
    PolygonsD const result = Union(left, right);
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_intersection(lua_State* L)
{
    if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
    {
        l_booleanError(L, "intersection", "Expected two polygon tables");
    }
    PolygonsD const left = LuaTableToPolygonsD(L, 1);
    PolygonsD const right = LuaTableToPolygonsD(L, 2);
    PolygonsD const result = Intersection(left, right);
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_difference(lua_State* L)
{
    if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
    {
        l_booleanError(L, "difference", "Expected two polygon tables");
    }
    PolygonsD const left = LuaTableToPolygonsD(L, 1);
    PolygonsD const right = LuaTableToPolygonsD(L, 2);
    PolygonsD const result = Difference(left, right);
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_xor(lua_State* L)
{
    if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_istable(L, 2))
    {
        l_booleanError(L, "xor", "Expected two polygon tables");
    }
    PolygonsD const left = LuaTableToPolygonsD(L, 1);
    PolygonsD const right = LuaTableToPolygonsD(L, 2);
    PolygonsD const result = Xor(left, right);
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_offsetOperation(lua_State* L)
{
    if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_isnumber(L, 2))
    {
        l_booleanError(L, "offsetOperation", "Expected a polygon table and a number offset");
    }
    Polygons const polys = LuaTableToPolygons(L, 1);
    double const delta = lua_tonumber(L, 2);
    auto resultI = Offset(polys, delta);
    PushPolygonsToLua(L, resultI);
    return 1;  // return the result table
}

int l_convexHullOperation(lua_State* L)
{
    if (lua_gettop(L) != 1 || !lua_istable(L, 1))
    {
        l_booleanError(L, "convexHullOperation", "Expected a polygon table");
    }
    PolygonsD const polys = LuaTableToPolygonsD(L, 1);
    PolygonsD result;
    for (const auto& p : polys)
    {
        result.push_back(ConvexHull(p));
    }
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_concaveHullOperation(lua_State* L)
{
    if (lua_gettop(L) != 2 || !lua_istable(L, 1) || !lua_isnumber(L, 2))
    {
        l_booleanError(L, "concaveHullOperation",
                       "Expected a polygon table and an integer number of additional points");
    }
    PolygonsD const polys = LuaTableToPolygonsD(L, 1);
    int const numAdditionalPoints = static_cast<int>(lua_tointeger(L, 2));
    PolygonsD result;
    for (const auto& p : polys)
    {
        result.push_back(ConcaveHullSimulation(p, numAdditionalPoints));
    }
    PushPolygonsDToLua(L, result);
    return 1;  // return the result table
}

int l_area(lua_State* L)
{
    if (lua_gettop(L) != 1 || !lua_istable(L, 1))
    {
        l_booleanError(L, "area", "Expected a polygon table");
    }
    PolygonD const poly = LuaTableToPolygonD(L, 1);
    double const area = Clipper2Lib::Area(poly);
    lua_pushnumber(L, area);
    return 1;  // return the area
}

int l_makeRectangle(lua_State* L)
{
    if (lua_gettop(L) != 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
    {
        l_booleanError(L, "makeRectangle", "Expected x, y, width, height");
    }
    double const x = lua_tonumber(L, 1);
    double const y = lua_tonumber(L, 2);
    double const width = lua_tonumber(L, 3);
    double const height = lua_tonumber(L, 4);
    PolygonD const result = MakeRectangle(x, y, width, height);
    PushPolygonsDToLua(L, PolygonsD{result});
    return 1;
}

int l_makeCircle(lua_State* L)
{
    if (lua_gettop(L) < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
    {
        l_booleanError(L, "makeCircle", "Expected cx, cy, radius [, segments]");
    }
    double const cx = lua_tonumber(L, 1);
    double const cy = lua_tonumber(L, 2);
    double const radius = lua_tonumber(L, 3);
    int segments = 64;
    if (lua_gettop(L) >= 4)
    {
        segments = static_cast<int>(lua_tointeger(L, 4));
    }
    PolygonD const result = MakeCircle(cx, cy, radius, segments);
    PushPolygonsDToLua(L, PolygonsD{result});
    return 1;
}

int l_makeEllipse(lua_State* L)
{
    if (lua_gettop(L) < 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
    {
        l_booleanError(L, "makeEllipse", "Expected cx, cy, rx, ry [, segments [, rotation]]");
    }
    double const cx = lua_tonumber(L, 1);
    double const cy = lua_tonumber(L, 2);
    double const rx = lua_tonumber(L, 3);
    double const ry = lua_tonumber(L, 4);
    int segments = 64;
    double rotation = 0.0;
    if (lua_gettop(L) >= 5)
    {
        segments = static_cast<int>(lua_tointeger(L, 5));
    }
    if (lua_gettop(L) >= 6)
    {
        rotation = lua_tonumber(L, 6);
    }
    PolygonD const result = MakeEllipse(cx, cy, rx, ry, segments, rotation);
    PushPolygonsDToLua(L, PolygonsD{result});
    return 1;
}

int l_makeRegularPolygon(lua_State* L)
{
    if (lua_gettop(L) < 4 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
    {
        l_booleanError(L, "makeRegularPolygon", "Expected cx, cy, radius, sides [, rotation]");
    }
    double const cx = lua_tonumber(L, 1);
    double const cy = lua_tonumber(L, 2);
    double const radius = lua_tonumber(L, 3);
    int const sides = static_cast<int>(lua_tointeger(L, 4));
    double rotation = 0.0;
    if (lua_gettop(L) >= 5)
    {
        rotation = lua_tonumber(L, 5);
    }
    PolygonD const result = MakeRegularPolygon(cx, cy, radius, sides, rotation);
    PushPolygonsDToLua(L, PolygonsD{result});
    return 1;
}

int l_textToPolygons(lua_State* L)
{
    if (lua_gettop(L) < 3 || !lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
    {
        l_booleanError(L, "textToPolygons", "Expected text, font_file, font_size [, x [, y [, curve_segments]]]");
    }
    std::string const text = lua_tostring(L, 1);
    std::string const font_file = lua_tostring(L, 2);
    double const font_size = lua_tonumber(L, 3);
    double x = 0.0;
    double y = 0.0;
    int curve_segments = 8;
    if (lua_gettop(L) >= 4)
    {
        x = lua_tonumber(L, 4);
    }
    if (lua_gettop(L) >= 5)
    {
        y = lua_tonumber(L, 5);
    }
    if (lua_gettop(L) >= 6)
    {
        curve_segments = static_cast<int>(lua_tointeger(L, 6));
    }
    PolygonsD const result = TextToPolygons(text, font_file, font_size, x, y, curve_segments);
    PushPolygonsDToLua(L, result);
    return 1;
}

#ifdef HSBA_POLYGON_DUMP
int l_dumpPolygon(lua_State* L)
{
    if (lua_gettop(L) < 2 || !lua_istable(L, 1) || !lua_isstring(L, 2))
    {
        l_booleanError(L, "dumpPolygon", "Expected a polygon table and a filename string");
    }
    Polygon const poly = LuaTableToPolygon(L, 1);
    const char* filename = lua_tostring(L, 2);
    bool close_path = true;
    if (lua_gettop(L) >= 3)
    {
        close_path = (lua_toboolean(L, 3) != 0);
    }
    DumpPolygon(poly, filename, close_path);
    return 0;
}

int l_dumpPolygons(lua_State* L)
{
    if (lua_gettop(L) < 2 || !lua_istable(L, 1) || !lua_isstring(L, 2))
    {
        l_booleanError(L, "dumpPolygons", "Expected a polygons table and a filename string");
    }
    Polygons const polys = LuaTableToPolygons(L, 1);
    const char* filename = lua_tostring(L, 2);
    bool close_path = true;
    if (lua_gettop(L) >= 3)
    {
        close_path = (lua_toboolean(L, 3) != 0);
    }
    DumpPolygons(polys, filename, close_path);
    return 0;
}
#endif

const luaL_Reg booleanLib[] = {{.name = "booleanOperation", .func = l_booleanOperation},
                               {.name = "union", .func = l_union},
                               {.name = "intersection", .func = l_intersection},
                               {.name = "difference", .func = l_difference},
                               {.name = "xor", .func = l_xor},
                               {.name = "offsetOperation", .func = l_offsetOperation},
                               {.name = "convexHullOperation", .func = l_convexHullOperation},
                               {.name = "concaveHullOperation", .func = l_concaveHullOperation},
                               {.name = "area", .func = l_area},
                               {.name = "makeRectangle", .func = l_makeRectangle},
                               {.name = "makeCircle", .func = l_makeCircle},
                               {.name = "makeEllipse", .func = l_makeEllipse},
                               {.name = "makeRegularPolygon", .func = l_makeRegularPolygon},
                               {.name = "textToPolygons", .func = l_textToPolygons},
#ifdef HSBA_POLYGON_DUMP
                               {.name = "dumpPolygon", .func = l_dumpPolygon},
                               {.name = "dumpPolygons", .func = l_dumpPolygons},
#endif
                               {.name = nullptr, .func = nullptr}};
}  // namespace

void PushPolygonDToLua(lua_State* L, const PolygonD& poly)
{
    lua_newtable(L);
    int idx = 1;
    for (const auto& p : poly)
    {
        lua_newtable(L);  // point
        lua_pushnumber(L, p.x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, p.y);
        lua_setfield(L, -2, "y");
        lua_rawseti(L, -2, idx++);
    }
}

void PushPolygonsDToLua(lua_State* L, const PolygonsD& poly)
{
    lua_createtable(L, static_cast<int>(poly.size()), 0);
    int idx = 1;
    for (const auto& pt : poly)
    {
        PushPolygonDToLua(L, pt);
        lua_rawseti(L, -2, idx++);
    }
}

void PushPolygonToLua(lua_State* L, const Polygon& poly)
{
    lua_newtable(L);
    int idx = 1;
    for (const auto& p : poly)
    {
        lua_newtable(L);  // point
        lua_pushnumber(L, static_cast<double>(p.x) / integerization);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, static_cast<double>(p.y) / integerization);
        lua_setfield(L, -2, "y");
        lua_rawseti(L, -2, idx++);
    }
}

void PushPolygonsToLua(lua_State* L, const Polygons& poly)
{
    lua_createtable(L, static_cast<int>(poly.size()), 0);
    int idx = 1;
    for (const auto& pt : poly)
    {
        PushPolygonToLua(L, pt);
        lua_rawseti(L, -2, idx++);
    }
}


// Helper: convert Lua table at given index to PolygonD
PolygonD LuaTableToPolygonD(lua_State* L, int index)
{
    PolygonD poly;
    if (!lua_istable(L, index))
    {
        return poly;
    }
    size_t const len = lua_rawlen(L, index);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, index, static_cast<int>(i));  // point table
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "x");
            lua_getfield(L, -2, "y");
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1))
            {
                double const x = lua_tonumber(L, -2);
                double const y = lua_tonumber(L, -1);
                poly.emplace_back(Point2D{x, y});
            }
            lua_pop(L, 2);  // pop x,y
        }
        lua_pop(L, 1);  // pop point table
    }
    return poly;
}

// Helper: convert Lua table at given index to PolygonsD
PolygonsD LuaTableToPolygonsD(lua_State* L, int index)
{
    PolygonsD polys;
    if (!lua_istable(L, index))
    {
        return polys;
    }
    size_t const len = lua_rawlen(L, index);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, index, static_cast<int>(i));  // polygon table
        PolygonD poly = LuaTableToPolygonD(L, -1);
        if (!poly.empty())
        {
            polys.emplace_back(std::move(poly));
        }
        lua_pop(L, 1);  // pop polygon table
    }
    return polys;
}


Polygon LuaTableToPolygon(lua_State* L, int index)
{
    Polygon poly;
    if (!lua_istable(L, index))
    {
        return poly;
    }
    size_t const len = lua_rawlen(L, index);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, index, static_cast<int>(i));  // point table
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "x");
            lua_getfield(L, -2, "y");
            if (lua_isnumber(L, -2) && lua_isnumber(L, -1))
            {
                double const x = lua_tonumber(L, -2);
                double const y = lua_tonumber(L, -1);
                poly.emplace_back(Point2{static_cast<int64_t>(std::llround(x * integerization)),
                                         static_cast<int64_t>(std::llround(y * integerization))});
            }
            lua_pop(L, 2);  // pop x,y
        }
        lua_pop(L, 1);  // pop point table
    }
    return poly;
}

Polygons LuaTableToPolygons(lua_State* L, int index)
{
    Polygons polys;
    if (!lua_istable(L, index))
    {
        return polys;
    }
    size_t const len = lua_rawlen(L, index);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, index, static_cast<int>(i));  // polygon table
        Polygon poly = LuaTableToPolygon(L, -1);
        if (!poly.empty())
        {
            polys.emplace_back(std::move(poly));
        }
        lua_pop(L, 1);  // pop polygon table
    }
    return polys;
}

void RegisterLuaPolygonOperations(lua_State* L)
{
    luaL_newlib(L, booleanLib);
    lua_setglobal(L, "PolygonOperations");
}

}  // namespace HsBa::Slicer