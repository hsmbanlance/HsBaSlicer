#define BOOST_TEST_MODULE lua_common_types_test
#include <boost/test/included/unit_test.hpp>

#include <lua.hpp>

#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"

BOOST_AUTO_TEST_SUITE(lua_common_types)

// Reads a boolean global set by the Lua script.
static bool GetBoolGlobal(lua_State* L, const char* name)
{
    lua_getglobal(L, name);
    bool value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

BOOST_AUTO_TEST_CASE(common_types_round_trip)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    HsBa::Slicer::RegisterCommonAnyObjectTypes(L);

    const char* script = R"(
        -- Eigen fixed vector (named-coordinate map, double scalar)
        local v = AnyObject.new_Vector3d({x = 1.0, y = 2.0, z = 3.0})
        local vb = v:cast_Vector3d()
        _G.ok_vec = (vb.x == 1 and vb.y == 2 and vb.z == 3)

        -- field reflection through ForeachField (x/y/z registered as doubles)
        local sum = 0
        v:foreach_field(function(name, value) sum = sum + value:cast_double() end)
        _G.ok_field = (sum == 6)

        -- sequence form is also accepted on input
        local v2 = AnyObject.new_Vector3d({4.0, 5.0, 6.0})
        local v2b = v2:cast_Vector3d()
        _G.ok_seq = (v2b.x == 4 and v2b.y == 5 and v2b.z == 6)

        -- integer vector
        local vi = AnyObject.new_Vector3i({x = 7, y = 8, z = 9})
        local vib = vi:cast_Vector3i()
        _G.ok_veci = (vib.x == 7 and vib.y == 8 and vib.z == 9)

        -- fixed matrix as nested rows
        local m = AnyObject.new_Matrix2d({{2, 0}, {0, 3}})
        local mb = m:cast_Matrix2d()
        _G.ok_mat = (mb[1][1] == 2 and mb[1][2] == 0 and mb[2][1] == 0 and mb[2][2] == 3)

        -- dynamic matrix (vertices-like)
        local mx = AnyObject.new_MatrixXf({{1.5, 2.5, 3.5}, {4.5, 5.5, 6.5}})
        local mxb = mx:cast_MatrixXf()
        _G.ok_matx = (#mxb == 2 and #mxb[1] == 3 and mxb[2][3] == 6.5)

        -- quaternion
        local q = AnyObject.new_Quaternionf({x = 0, y = 0, z = 0, w = 1})
        local qb = q:cast_Quaternionf()
        _G.ok_quat = (qb.x == 0 and qb.y == 0 and qb.z == 0 and qb.w == 1)

        -- Clipper2 float polygon
        local poly = AnyObject.new_PolygonD({{x = 0, y = 0}, {x = 1, y = 0}, {x = 0, y = 1}})
        local polyb = poly:cast_PolygonD()
        _G.ok_poly = (#polyb == 3 and polyb[3].y == 1 and polyb[2].x == 1)

        -- Clipper2 integer polygons (nested)
        local polys = AnyObject.new_Polygons({{{x = 0, y = 0}, {x = 2, y = 0}, {x = 0, y = 2}}, {{x = 5, y = 5}}})
        local polysb = polys:cast_Polygons()
        _G.ok_polys = (#polysb == 2 and #polysb[1] == 3 and polysb[1][2].x == 2 and polysb[2][1].y == 5)

        -- scalar adapters remain available
        local s = AnyObject.new_string("Hello")
        _G.ok_str = (AnyObject.invoke(s, "size"):cast_size_t() == 5)

        -- long long adapter is exposed through the common registration path
        -- (regression guard for the LuaLongLong CRTP base)
        local ll = AnyObject.new_longlong(1234567890123)
        _G.ok_llong = (ll:cast_longlong() == 1234567890123)
    )";

    if (luaL_dostring(L, script) != LUA_OK)
    {
        const char* err = lua_tostring(L, -1);
        BOOST_FAIL("Lua script error: " << err);
    }

    BOOST_CHECK(GetBoolGlobal(L, "ok_vec"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_field"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_seq"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_veci"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_mat"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_matx"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_quat"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_poly"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_polys"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_str"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_llong"));

    lua_close(L);
}

BOOST_AUTO_TEST_CASE(typeinfo_reflection_and_cast_methods)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    HsBa::Slicer::RegisterCommonAnyObjectTypes(L);

    const char* script = R"(
        -- integer vectors now reflect x/y/z as int fields
        local vi = AnyObject.new_Vector3i({x = 7, y = 8, z = 9})
        local sumi, ni = 0, 0
        vi:foreach_field(function(name, value) sumi = sumi + value:cast_int(); ni = ni + 1 end)
        _G.ok_veci_field = (sumi == 24 and ni == 3)

        -- quaternion reflects x/y/z/w as float fields (internal order is x, y, z, w)
        local q = AnyObject.new_Quaternionf({x = 1, y = 2, z = 3, w = 4})
        local sumq, nq = 0, 0
        q:foreach_field(function(name, value) sumq = sumq + value:cast_float(); nq = nq + 1 end)
        _G.ok_quat_field = (sumq == 10 and nq == 4)

        -- Clipper2 point reflects x/y as double fields
        local p = AnyObject.new_Point2D({x = 2.5, y = 3.5})
        local sump, npt = 0, 0
        p:foreach_field(function(name, value) sump = sump + value:cast_double(); npt = npt + 1 end)
        _G.ok_point_field = (sump == 6.0 and npt == 2)

        -- cast_<Name> is registered as an AnyObject method (TypeInfo::methods), reached via invoke
        local v = AnyObject.new_Vector3d({x = 1, y = 2, z = 3})
        local view = v:invoke("cast_Vector3d")
        local t = view:cast_Vector3d()
        _G.ok_invoke_cast = (t.x == 1 and t.y == 2 and t.z == 3)

        -- opaque types (matrices) still expose a working cast method
        local m = AnyObject.new_Matrix2d({{2, 0}, {0, 3}})
        local mview = m:invoke("cast_Matrix2d")
        _G.ok_opaque_cast = (mview ~= nil)

        -- unknown method raises an error through the metatable invoke
        local ok = pcall(function() return v:invoke("no_such_method") end)
        _G.ok_bad_method = (ok == false)
    )";

    if (luaL_dostring(L, script) != LUA_OK)
    {
        BOOST_FAIL("Lua script error: " << lua_tostring(L, -1));
    }

    BOOST_CHECK(GetBoolGlobal(L, "ok_veci_field"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_quat_field"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_point_field"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_invoke_cast"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_opaque_cast"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_bad_method"));

    lua_close(L);
}

BOOST_AUTO_TEST_CASE(pipeline_pool_installation)
{
    // Installing twice must stay idempotent (once_flag): exactly one entry per pool.
    HsBa::Slicer::InstallCommonAnyObjectTypes();
    HsBa::Slicer::InstallCommonAnyObjectTypes();

    auto& pool2d = HsBa::Slicer::Get2DFunctions();
    auto& pool3d = HsBa::Slicer::Get3DFunctions();
    auto& pool_file = HsBa::Slicer::GetFileFunctions();
    BOOST_CHECK_EQUAL(pool2d.size(), 1u);
    BOOST_CHECK_EQUAL(pool3d.size(), 1u);
    BOOST_CHECK_EQUAL(pool_file.size(), 1u);

    // Simulate the Support stage which runs the 2D *and* 3D pools on one state;
    // the per-state guard makes the second execution a no-op instead of a re-registration.
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    for (auto& reg : pool2d)
        reg(L);
    for (auto& reg : pool3d)
        reg(L);

    const char* script = R"(
        local v = AnyObject.new_Vector3d({x = 1, y = 2, z = 3})
        local t = v:cast_Vector3d()
        _G.ok_pool = (t.x == 1 and t.y == 2 and t.z == 3)
        -- direct re-registration attempts are also no-ops, the table stays functional
        local s = AnyObject.new_string("Hello")
        _G.ok_scalar = (AnyObject.invoke(s, "size"):cast_size_t() == 5)
    )";

    if (luaL_dostring(L, script) != LUA_OK)
    {
        BOOST_FAIL("Lua script error: " << lua_tostring(L, -1));
    }

    BOOST_CHECK(GetBoolGlobal(L, "ok_pool"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_scalar"));

    // Calling RegisterCommonAnyObjectTypes directly on an already-registered state must not
    // destroy the existing AnyObject globals (guard returns early).
    HsBa::Slicer::RegisterCommonAnyObjectTypes(L);
    BOOST_CHECK_EQUAL(LUA_OK, luaL_dostring(L, "_G.ok_guard = (type(AnyObject.new_Vector3d) == 'function')"));
    BOOST_CHECK(GetBoolGlobal(L, "ok_guard"));

    lua_close(L);
}

BOOST_AUTO_TEST_CASE(new_non_table_argument_errors)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    HsBa::Slicer::RegisterCommonAnyObjectTypes(L);

    // new_Vector3d requires a table; passing a number must raise a Lua error.
    int status = luaL_dostring(L, "AnyObject.new_Vector3d(42)");
    BOOST_CHECK_NE(status, LUA_OK);
    const char* err = lua_tostring(L, -1);
    BOOST_CHECK(err != nullptr);

    lua_close(L);
}

// Round-trips every remaining registered custom type so each TableAdapter instantiation
// (new_/cast_ plus the push/read helpers) is executed. Lua assert() aborts the script on any
// mismatch, so a clean dostring plus the all_ok flag means every type converted correctly.
BOOST_AUTO_TEST_CASE(all_registered_types_round_trip)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    HsBa::Slicer::RegisterCommonAnyObjectTypes(L);

    const char* script = R"lua(
        -- Eigen named vectors not exercised by the other cases
        local v2f = AnyObject.new_Vector2f({x=1.5, y=2.5}):cast_Vector2f()
        assert(v2f.x == 1.5 and v2f.y == 2.5)
        local v3f = AnyObject.new_Vector3f({1, 2, 3}):cast_Vector3f()  -- plain sequence input
        assert(v3f.x == 1 and v3f.y == 2 and v3f.z == 3)
        local v4f = AnyObject.new_Vector4f({x=1, y=2, z=3, w=4}):cast_Vector4f()
        assert(v4f.x == 1 and v4f.w == 4)
        local v2d = AnyObject.new_Vector2d({x=3.25, y=4.5}):cast_Vector2d()
        assert(v2d.x == 3.25 and v2d.y == 4.5)
        local v4d = AnyObject.new_Vector4d({x=1, y=2, z=3, w=4}):cast_Vector4d()
        assert(v4d.z == 3 and v4d.w == 4)
        local v2i = AnyObject.new_Vector2i({x=7, y=8}):cast_Vector2i()
        assert(v2i.x == 7 and v2i.y == 8)
        local v4i = AnyObject.new_Vector4i({x=1, y=2, z=3, w=4}):cast_Vector4i()
        assert(v4i.x == 1 and v4i.w == 4)

        -- double quaternion (x, y, z, w named)
        local qd = AnyObject.new_Quaterniond({x=0.1, y=0.2, z=0.3, w=1.0}):cast_Quaterniond()
        assert(qd.w == 1.0 and qd.x == 0.1)

        -- Clipper2 integer point rounds to int64
        local pt2 = AnyObject.new_Point2({x=11, y=-13}):cast_Point2()
        assert(pt2.x == 11 and pt2.y == -13)

        -- fixed matrices as nested row sequences
        local m3 = AnyObject.new_Matrix3d({{1,0,0},{0,1,0},{0,0,1}}):cast_Matrix3d()
        assert(m3[1][1] == 1 and m3[2][2] == 1 and m3[3][3] == 1 and m3[1][2] == 0)
        local m4 = AnyObject.new_Matrix4d({{2,0,0,0},{0,2,0,0},{0,0,2,0},{0,0,0,2}}):cast_Matrix4d()
        assert(m4[1][1] == 2 and m4[4][4] == 2)

        -- dynamic integer matrix keeps its 3x2 shape
        local mxi = AnyObject.new_MatrixXi({{1,2},{3,4},{5,6}}):cast_MatrixXi()
        assert(#mxi == 3 and #mxi[1] == 2 and mxi[3][2] == 6)

        -- double polygon set and single integer polygon
        local pd = AnyObject.new_PolygonsD({ { {x=0,y=0},{x=1,y=0},{x=0,y=1} } }):cast_PolygonsD()
        assert(#pd == 1 and #pd[1] == 3 and pd[1][3].y == 1)
        local pg = AnyObject.new_Polygon({ {x=2,y=2},{x=4,y=2},{x=2,y=4} }):cast_Polygon()
        assert(#pg == 3 and pg[1].x == 2 and pg[3].y == 4)

        -- field reflection over a double vector visits x/y as doubles
        local s, n = 0, 0
        AnyObject.new_Vector2d({x=1.5, y=2.5}):foreach_field(function(name, value) s = s + value:cast_double(); n = n + 1 end)
        assert(s == 4.0 and n == 2)

        _G.all_ok = true
    )lua";

    if (luaL_dostring(L, script) != LUA_OK)
    {
        BOOST_FAIL("Lua script error: " << lua_tostring(L, -1));
    }
    BOOST_CHECK(GetBoolGlobal(L, "all_ok"));
    lua_close(L);
}

BOOST_AUTO_TEST_SUITE_END()
