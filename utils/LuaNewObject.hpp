#pragma once
#ifndef HSBA_LUANEWOBJECT_HPP
#define HSBA_LUANEWOBJECT_HPP

#include <lua.hpp>

#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Allocate a new object in Lua userdata and assign it a metatable.
 *
 * @tparam T Type of object to construct.
 * @tparam Args Constructor argument types.
 * @param L Lua state.
 * @param mt Metatable name.
 * @param args Constructor arguments.
 * @return T* Pointer to the newly constructed object stored in Lua userdata.
 */
template <typename T, typename... Args>
inline T* NewLuaObject(lua_State* L, const char* mt, Args&&... args)
{
    void* ud = lua_newuserdata(L, sizeof(T));
    T* obj = new (ud) T(std::forward<Args>(args)...);
    luaL_getmetatable(L, mt);
    lua_setmetatable(L, -2);
    return obj;
}

/**
 * @brief Allocate a new object in Lua userdata using a compile-time metatable name.
 *
 * @tparam T Type of object to construct.
 * @tparam TName Compile-time metatable identifier.
 * @tparam Args Constructor argument types.
 * @param L Lua state.
 * @param args Constructor arguments.
 * @return T* Pointer to the newly constructed object stored in Lua userdata.
 */
template <typename T, Utils::TemplateString TName, typename... Args>
inline T* NewLuaObject(lua_State* L, Args&&... args)
{
    void* ud = lua_newuserdata(L, sizeof(T));
    T* obj = new (ud) T(std::forward<Args>(args)...);
    luaL_getmetatable(L, static_cast<const char*>(TName));
    lua_setmetatable(L, -2);
    return obj;
}

/**
 * @brief Garbage-collect a Lua userdata object by calling its destructor.
 *
 * @tparam T Type of object stored in userdata.
 * @param L Lua state.
 * @param mt Metatable name associated with the userdata.
 * @return int Always returns 0 for Lua finalizers.
 */
template <typename T>
int LuaGC(lua_State* L, const char* mt) noexcept
{
    void* ud = luaL_checkudata(L, 1, mt);
    if (ud)
        static_cast<T*>(ud)->~T();
    return 0;
}

/**
 * @brief Garbage-collect a Lua userdata object using a compile-time metatable name.
 *
 * @tparam T Type of object stored in userdata.
 * @tparam TName Compile-time metatable identifier.
 * @param L Lua state.
 * @return int Always returns 0 for Lua finalizers.
 */
template <typename T, Utils::TemplateString TName>
int LuaGC(lua_State* L) noexcept
{
    void* ud = luaL_checkudata(L, 1, static_cast<const char*>(TName));
    if (ud)
        static_cast<T*>(ud)->~T();
    return 0;
}

/**
 * @brief Custom deleter for Lua states stored in unique_ptr.
 */
struct LuaStateDeleter
{
    void operator()(lua_State* L) const noexcept
    {
        if (L)
            lua_close(L);
    }
};

/**
 * @brief Unique pointer alias for managing Lua state lifetime.
 */
using UniqueLua = std::unique_ptr<lua_State, LuaStateDeleter>;

/**
 * @brief Create a unique Lua state wrapped in a smart pointer.
 *
 * @return UniqueLua Managed Lua state.
 */
inline UniqueLua MakeUniqueLuaState()
{
    lua_State* L = luaL_newstate();
    return UniqueLua{L};
}
}  // namespace HsBa::Slicer

#endif  // !HSBA_LUANEWOBJECT_HPP