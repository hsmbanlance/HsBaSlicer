#define BOOST_TEST_MODULE cipher_lua_adapter_test
#include <boost/test/included/unit_test.hpp>

#include "cipher/LuaAdapter.hpp"

#include <lua.hpp>

#include <string>

// Exercises the Lua bindings registered by RegisterLuaCipher (cipher/LuaAdapter.cpp):
// the global `Cipher` table's base64/hex encode/decode wrappers over Cipher::Encoder.
// Results are pushed back into Lua globals and read from C++, mirroring how scripts
// consume them. The invalid-hex case drives the catch->lua_error branch through pcall.
BOOST_AUTO_TEST_SUITE(cipher_lua_adapter)

namespace
{
// Owns a lua_State pre-loaded with the Cipher module.
struct LuaCipherFixture
{
    lua_State* L = nullptr;
    std::string error_;

    LuaCipherFixture()
    {
        L = luaL_newstate();
        luaL_openlibs(L);
        HsBa::Slicer::Cipher::RegisterLuaCipher(L);
    }
    ~LuaCipherFixture()
    {
        if (L)
            lua_close(L);
    }
    LuaCipherFixture(const LuaCipherFixture&) = delete;
    LuaCipherFixture& operator=(const LuaCipherFixture&) = delete;

    bool run(const char* script)
    {
        if (luaL_dostring(L, script) != LUA_OK)
        {
            error_ = lua_tostring(L, -1) ? lua_tostring(L, -1) : "(unknown error)";
            return false;
        }
        return true;
    }
    std::string globalStr(const char* name)
    {
        lua_getglobal(L, name);
        size_t len = 0;
        const char* s = lua_tolstring(L, -1, &len);
        std::string out(s ? s : "", len);
        lua_pop(L, 1);
        return out;
    }
    bool globalTrue(const char* name)
    {
        lua_getglobal(L, name);
        bool v = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return v;
    }
};
}  // namespace

// Cipher.base64_encode / base64_decode round-trip against the standard golden vector.
BOOST_AUTO_TEST_CASE(base64_round_trip)
{
    LuaCipherFixture f;
    const char* script = R"(
        _G.enc  = Cipher.base64_encode("hello")
        _G.dec  = Cipher.base64_decode(_G.enc)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK_EQUAL(f.globalStr("enc"), "aGVsbG8=");
    BOOST_CHECK_EQUAL(f.globalStr("dec"), "hello");
}

// Cipher.hex_encode / hex_decode round-trip (ASCII "ABC" -> "414243").
BOOST_AUTO_TEST_CASE(hex_round_trip)
{
    LuaCipherFixture f;
    const char* script = R"(
        _G.enc  = Cipher.hex_encode("ABC")
        _G.dec  = Cipher.hex_decode(_G.enc)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK_EQUAL(f.globalStr("enc"), "414243");
    BOOST_CHECK_EQUAL(f.globalStr("dec"), "ABC");
}

// hex_decode of a non-hex character makes Encoder::hex_decode throw; the binding must
// surface it as a Lua error (pcall returns false), covering the catch->lua_error branch.
BOOST_AUTO_TEST_CASE(invalid_hex_raises_lua_error)
{
    LuaCipherFixture f;
    const char* script = R"(
        _G.errored = not pcall(function() return Cipher.hex_decode("zz") end)
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK(f.globalTrue("errored"));
}

// Empty input is a valid edge case for all four encoders (no exception, empty result).
BOOST_AUTO_TEST_CASE(empty_input_edge_cases)
{
    LuaCipherFixture f;
    const char* script = R"(
        _G.b64enc  = Cipher.base64_encode("")
        _G.b64dec  = Cipher.base64_decode("")
        _G.hexenc  = Cipher.hex_encode("")
        _G.hexdec  = Cipher.hex_decode("")
    )";
    if (!f.run(script))
        BOOST_FAIL("Lua script error: " << f.error_);

    BOOST_CHECK_EQUAL(f.globalStr("b64enc"), "");
    BOOST_CHECK_EQUAL(f.globalStr("b64dec"), "");
    BOOST_CHECK_EQUAL(f.globalStr("hexenc"), "");
    BOOST_CHECK_EQUAL(f.globalStr("hexdec"), "");
}

BOOST_AUTO_TEST_SUITE_END()
