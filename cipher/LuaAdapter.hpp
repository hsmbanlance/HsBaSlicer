/** @file LuaAdapter.hpp
 * @brief Lua bindings that expose the Cipher module (encoder/hasher/encrypt) to Lua scripts.
 * @author HsBa
 */
#pragma once
#ifndef CIPHER_LUAADAPTER_HPP
#define CIPHER_LUAADAPTER_HPP

#include <lua.hpp>

namespace HsBa::Slicer::Cipher
{
/// Register the Cipher module functions (Encoder, Hasher, Encrypt) into the given Lua state.
void RegisterLuaCipher(lua_State* L);
}

#endif  // CIPHER_LUAADAPTER_HPP
