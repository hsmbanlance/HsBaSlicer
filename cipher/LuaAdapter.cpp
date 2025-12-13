#include "LuaAdapter.hpp"
#include "encoder.hpp"
#include <format>

namespace HsBa::Slicer::Cipher
{
    namespace
    {
        int l_base64_encode(lua_State* L)
        {
            size_t len;
            const char* data = luaL_checklstring(L, 1, &len);
            try
            {
                std::string_view sv(data, len);
                std::string out = Encoder::base64_encode(sv);
                lua_pushlstring(L, out.c_str(), out.size());
                return 1;
            }
            catch (const std::exception& e)
            {
                lua_pushstring(L, e.what());
                return lua_error(L);
            }
        }

        int l_base64_decode(lua_State* L)
        {
            size_t len;
            const char* data = luaL_checklstring(L, 1, &len);
            try
            {
                std::string_view sv(data, len);
                auto vec = Encoder::base64_decode(sv);
                lua_pushlstring(L, reinterpret_cast<const char*>(vec.data()), vec.size());
                return 1;
            }
            catch (const std::exception& e)
            {
                lua_pushstring(L, e.what());
                return lua_error(L);
            }
        }

        int l_hex_encode(lua_State* L)
        {
            size_t len;
            const char* data = luaL_checklstring(L, 1, &len);
            try
            {
                std::string_view sv(data, len);
                std::string out = Encoder::hex_encode(sv);
                lua_pushlstring(L, out.c_str(), out.size());
                return 1;
            }
            catch (const std::exception& e)
            {
                lua_pushstring(L, e.what());
                return lua_error(L);
            }
        }

        int l_hex_decode(lua_State* L)
        {
            size_t len;
            const char* data = luaL_checklstring(L, 1, &len);
            try
            {
                std::string_view sv(data, len);
                auto vec = Encoder::hex_decode(sv);
                lua_pushlstring(L, reinterpret_cast<const char*>(vec.data()), vec.size());
                return 1;
            }
            catch (const std::exception& e)
            {
                lua_pushstring(L, e.what());
                return lua_error(L);
            }
        }

        static const luaL_Reg cipherLib[] = {
            {"base64_encode", l_base64_encode},
            {"base64_decode", l_base64_decode},
            {"hex_encode", l_hex_encode},
            {"hex_decode", l_hex_decode},
            {nullptr, nullptr}
        };
    }

    void RegisterLuaCipher(lua_State* L)
    {
        luaL_newlib(L, cipherLib);
        lua_setglobal(L, "Cipher");
    }
} // namespace HsBa::Slicer::Cipher
