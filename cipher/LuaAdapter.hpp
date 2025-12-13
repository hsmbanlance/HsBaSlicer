#pragma once
#ifndef CIPHER_LUAADAPTER_HPP
#define CIPHER_LUAADAPTER_HPP

#include <lua.hpp>

namespace HsBa::Slicer::Cipher {
    void RegisterLuaCipher(lua_State* L);
}

#endif // CIPHER_LUAADAPTER_HPP
