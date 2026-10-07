#define BOOST_TEST_MODULE dll_loader_test
#include <boost/test/included/unit_test.hpp>

#include "utils/LuaDllLoader.hpp"

#ifndef HSBA_NO_DLL_LOADER

#include <lua.hpp>

#include <memory>
#include <string>
#include <vector>

namespace
{
// Absolute path to the mock library, injected by CMake. Injected into Lua as a global
// (rather than pasted into the script) so path separators never need escaping.
const std::string& MockDllPath()
{
    static const std::string path = []
    {
#ifdef MOCK_DL_LOADER_DLL_DIR
        std::string dir(MOCK_DL_LOADER_DLL_DIR);
        dir += '/';
#ifdef _WIN32
        dir += "mock_dll_loader_dll.dll";
#elif defined(__APPLE__)
        dir += "libmock_dll_loader_dll.dylib";
#else
        dir += "libmock_dll_loader_dll.so";
#endif
        return dir;
#else
        return std::string("mock_dll_loader_dll");
#endif
    }();
    return path;
}

// Bootstraps a Lua state with the DllLoader binding and a MOCK_PATH global.
struct Fixture
{
    lua_State* L = nullptr;
    std::string error_;

    Fixture()
    {
        L = luaL_newstate();
        luaL_openlibs(L);
        std::vector<std::unique_ptr<HsBa::Slicer::DllGetFunctionAbstract>> regs;
        regs.push_back(std::make_unique<HsBa::Slicer::detail::LuaDllGetFunction<"intret", int>>());
        regs.push_back(std::make_unique<HsBa::Slicer::detail::LuaDllGetFunction<"doubleret", double>>());
        regs.push_back(std::make_unique<HsBa::Slicer::detail::LuaDllGetFunction<"voidret", void>>());
        regs.push_back(std::make_unique<HsBa::Slicer::detail::LuaDllGetFunction<"i2", int, int, int>>());
        regs.push_back(std::make_unique<HsBa::Slicer::detail::LuaDllGetFunction<"d2", double, double, double>>());
        HsBa::Slicer::RegisterLuaDllLoader(L, std::move(regs));
        lua_pushstring(L, MockDllPath().c_str());
        lua_setglobal(L, "MOCK_PATH");
    }
    ~Fixture()
    {
        if (L)
            lua_close(L);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    bool run(const char* script)
    {
        if (luaL_dostring(L, script) != LUA_OK)
        {
            error_ = lua_tostring(L, -1) ? lua_tostring(L, -1) : "(unknown error)";
            return false;
        }
        return true;
    }
    bool globalTrue(const char* name)
    {
        lua_getglobal(L, name);
        bool v = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return v;
    }
    double globalNum(const char* name)
    {
        lua_getglobal(L, name);
        double v = lua_tonumber(L, -1);
        lua_pop(L, 1);
        return v;
    }
};
}  // namespace

BOOST_AUTO_TEST_SUITE(dll_loader)

// DllLoader.new + zero-argument call (int / double / void return branches) + get
// (lightuserdata) + unload + reload. The post-reload call proves reload() works.
BOOST_AUTO_TEST_CASE(new_call_get_unload_reload)
{
    Fixture f;
    const char* script = R"(
        local o = DllLoader.new(MOCK_PATH)
        _G.ans   = o:call_intret("mock_answer")
        _G.pi    = o:call_doubleret("mock_pi")
        _G.nores = o:call_voidret("mock_nop")            -- void -> nil result
        _G.got   = (o:get_intret("mock_answer") ~= nil)  -- lightuserdata address
        o:unload()
        o:reload(MOCK_PATH)
        _G.ans2  = o:call_intret("mock_answer")
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK_EQUAL(f.globalNum("ans"), 42.0);
    BOOST_CHECK_EQUAL(f.globalNum("pi"), 3.5);
    lua_getglobal(f.L, "nores");
    BOOST_CHECK(lua_isnil(f.L, -1));
    lua_pop(f.L, 1);
    BOOST_CHECK(f.globalTrue("got"));
    BOOST_CHECK_EQUAL(f.globalNum("ans2"), 42.0);
}

// Regression for the "arguments never read from the stack" defect: mock_add(3,4)
// and mock_scale(2.5,4.0) must observe the real Lua-supplied values, not zeros.
BOOST_AUTO_TEST_CASE(call_with_real_arguments)
{
    Fixture f;
    const char* script = R"(
        local o = DllLoader.new(MOCK_PATH)
        _G.sum  = o:call_i2("mock_add", 3, 4)
        _G.prod = o:call_d2("mock_scale", 2.5, 4.0)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK_EQUAL(f.globalNum("sum"), 7.0);
    BOOST_CHECK_EQUAL(f.globalNum("prod"), 10.0);
}

// Argument-count mismatch must raise through the error branch (pcall returns false).
BOOST_AUTO_TEST_CASE(call_arg_count_mismatch_errors)
{
    Fixture f;
    const char* script = R"(
        local o = DllLoader.new(MOCK_PATH)
        _G.err = not pcall(function() return o:call_i2("mock_add", 3) end)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);
    BOOST_CHECK(f.globalTrue("err"));
}

// Resolving an unknown symbol must raise through the get catch branch.
BOOST_AUTO_TEST_CASE(get_unknown_symbol_errors)
{
    Fixture f;
    const char* script = R"(
        local o = DllLoader.new(MOCK_PATH)
        _G.err = not pcall(function() return o:get_intret("no_such_symbol") end)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);
    BOOST_CHECK(f.globalTrue("err"));
}

// The `if (!dll)` null-object guard in unload/reload/get/call: calling the class-table
// form with a non-pointer first argument (a number) makes lua_topointer return NULL,
// so every branch raises instead of dereferencing garbage.
BOOST_AUTO_TEST_CASE(null_object_error_branches)
{
    Fixture f;
    const char* script = R"(
        _G.e1 = not pcall(function() return DllLoader.unload(5) end)
        _G.e2 = not pcall(function() return DllLoader.reload(5, MOCK_PATH) end)
        _G.e3 = not pcall(function() return DllLoader.get_intret(5, "mock_answer") end)
        _G.e4 = not pcall(function() return DllLoader.call_intret(5, "mock_answer") end)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK(f.globalTrue("e1"));
    BOOST_CHECK(f.globalTrue("e2"));
    BOOST_CHECK(f.globalTrue("e3"));
    BOOST_CHECK(f.globalTrue("e4"));
}

BOOST_AUTO_TEST_SUITE_END()

#endif  // !HSBA_NO_DLL_LOADER
