/** @file LuaNewObject.hpp
 * @brief Helpers to construct and garbage-collect C++ objects stored in Lua userdata and manage lua_State lifetime.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_LUANEWOBJECT_HPP
#define HSBA_LUANEWOBJECT_HPP

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>

#include <lua.hpp>

#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Keeps a function out of its caller's stack frame.
 *
 * Required by the Lua error paths: lua_error's longjmp unwinds every frame between the raise and
 * Lua's protected call, and MSVC x64 runs the destructors those frames registered. A helper whose
 * locals were already destroyed by a thrown exception would therefore free them a second time when
 * the raise passes over it, so the helper has to leave the stack before the raise happens.
 */
#if defined(_MSC_VER)
#define HSBA_NOINLINE __declspec(noinline)
#else
#define HSBA_NOINLINE __attribute__((noinline))
#endif

/**
 * @brief Signature of a Lua binding body wrapped by guarded().
 *
 * The worker pushes either the results (and returns their count) or the error text (and returns -1);
 * it must not call lua_error itself, because its frame owns the C++ resources of the operation.
 */
using LuaErrorWorker = int (*)(lua_State*);

/**
 * @brief Register-safe wrapper that raises Lua errors from a frame which owns nothing.
 *
 * lua_error's longjmp is unwound frame by frame, and MSVC x64 runs the destructors each unwound frame
 * registered. A binding frame that has just caught an exception has already destroyed the locals its try
 * block created, so raising from that frame frees them a second time (heap corruption or terminate under
 * /O2). guarded() therefore keeps the raise in its own frame - no try/catch, no owning locals - while the
 * worker, invoked through a function pointer so it can never be inlined, has fully returned by then. The
 * worker's POSIX-side benefit is the same: nothing leaks, because nothing is left alive at the raise.
 *
 * @tparam Worker Binding body to run; register the wrapper as Worker's name with guarded<Worker>.
 * @param L Lua state.
 * @return int Result count to hand back to Lua.
 */
template <LuaErrorWorker Worker>
HSBA_NOINLINE int guarded(lua_State* L)
{
    const int results = Worker(L);
    if (results < 0)
    {
        return lua_error(L);  // the worker already pushed the message and left the stack
    }
    return results;
}

/**
 * @brief Raise a Lua error without leaking C++ memory and without double freeing it.
 *
 * lua_error longjmps, and the two toolchains disagree about what that does to C++ objects:
 * MSVC x64 routes longjmp through RtlUnwind, which runs the destructors registered by every
 * skipped frame, while POSIX longjmp jumps straight out and runs none. An owning object that
 * was already destroyed before the raise is therefore freed twice on Windows, and an owning
 * object still alive at the raise leaks on Linux. To be correct on both, the message is
 * formatted into a trivially destructible buffer and copied into Lua's own memory, so the
 * raising frame holds nothing that any unwinder could clean up.
 *
 * @tparam Args Format argument types; they must be non-owning (literals, views, TemplateString).
 * @param L Lua state.
 * @param fmt Compile-time format string, e.g. "Invalid {} object".
 * @param args Format arguments.
 * @return int Never returns; declared so callers can write `return raise_lua_error(...);`.
 */
template <typename... Args>
inline int raise_lua_error(lua_State* L, std::format_string<Args...> fmt, Args&&... args)
{
    std::array<char, 512> buffer{};
    const auto formatted = std::format_to_n(buffer.data(), buffer.size() - 1, fmt, std::forward<Args>(args)...);
    const size_t length = std::min(static_cast<size_t>(formatted.size), buffer.size() - 1);
    lua_pushlstring(L, buffer.data(), length);
    return lua_error(L);
}

/**
 * @brief Raise a Lua error prefixed with the caller position, without leaking or double freeing.
 *
 * Same message layout as luaL_error ("file:line: message"), produced by luaL_where + push +
 * concat, and the same trivially destructible buffer as raise_lua_error.
 *
 * @tparam Args Format argument types; they must be non-owning, see raise_lua_error.
 * @param L Lua state.
 * @param fmt Compile-time format string, e.g. "Failed to load model '{}'".
 * @param args Format arguments; only non-owning values are allowed, see raise_lua_error.
 * @return int Never returns; declared so callers can write `return raise_lua_error_with_where(...);`.
 */
template <typename... Args>
inline int raise_lua_error_with_where(lua_State* L, std::format_string<Args...> fmt, Args&&... args)
{
    std::array<char, 512> buffer{};
    const auto formatted = std::format_to_n(buffer.data(), buffer.size() - 1, fmt, std::forward<Args>(args)...);
    const size_t length = std::min(static_cast<size_t>(formatted.size), buffer.size() - 1);
    luaL_where(L, 1);
    lua_pushlstring(L, buffer.data(), length);
    lua_concat(L, 2);
    return lua_error(L);
}

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