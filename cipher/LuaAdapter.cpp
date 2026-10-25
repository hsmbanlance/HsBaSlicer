/** @file LuaAdapter.cpp
 * @brief Implementation of the Lua bindings exposing the Cipher module to scripts.
 * @author HsBa
 */
#include "LuaAdapter.hpp"
#include "encoder.hpp"
#include <format>

namespace HsBa::Slicer::Cipher
{
namespace
{
int l_base64_encode(lua_State* L)
{
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    try
    {
        std::string_view const sv(data, len);
        std::string const out = Encoder::base64_encode(sv);
        lua_pushlstring(L, out.c_str(), out.size());
        return 1;
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int l_base64_decode(lua_State* L)
{
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    try
    {
        std::string_view const sv(data, len);
        auto vec = Encoder::base64_decode(sv);
        lua_pushlstring(L, reinterpret_cast<const char*>(vec.data()), vec.size());
        return 1;
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int l_hex_encode(lua_State* L)
{
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    try
    {
        std::string_view const sv(data, len);
        std::string const out = Encoder::hex_encode(sv);
        lua_pushlstring(L, out.c_str(), out.size());
        return 1;
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

int l_hex_decode(lua_State* L)
{
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);
    try
    {
        std::string_view const sv(data, len);
        auto vec = Encoder::hex_decode(sv);
        lua_pushlstring(L, reinterpret_cast<const char*>(vec.data()), vec.size());
        return 1;
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
    }
    return lua_error(L);
}

const luaL_Reg cipherLib[] = {{.name = "base64_encode", .func = l_base64_encode},
                              {.name = "base64_decode", .func = l_base64_decode},
                              {.name = "hex_encode", .func = l_hex_encode},
                              {.name = "hex_decode", .func = l_hex_decode},
                              {.name = nullptr, .func = nullptr}};
}  // namespace

void RegisterLuaCipher(lua_State* L)
{
    luaL_newlib(L, cipherLib);
    lua_setglobal(L, "Cipher");
}
}  // namespace HsBa::Slicer::Cipher
