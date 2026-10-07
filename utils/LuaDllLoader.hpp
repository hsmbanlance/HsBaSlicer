#pragma once
#ifndef HSBA_LUADLLLOADER_HPP
#define HSBA_LUADLLLOADER_HPP

#ifndef HSBA_NO_DLL_LOADER

#include <boost/dll.hpp>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include "LuaAnyObject.hpp"
#include "LuaNewObject.hpp"

/**
 * @file LuaDllLoader.hpp
 * @brief Lua DLL loader helpers for runtime function resolution and invocation.
 */

namespace HsBa::Slicer
{
/**
 * @brief Runtime DLL loader wrapper for native function lookup.
 */
class DllLoader
{
public:
    /**
     * @brief Construct the loader for a DLL file path.
     *
     * @param dllPath Path to the DLL to load.
     */
    DllLoader(std::string_view dllPath) : m_dll(dllPath.data()) {}

    /**
     * @brief Retrieve a function symbol from the loaded DLL.
     *
     * @tparam T Function pointer or callable type.
     * @param functionName Name of the symbol to resolve.
     * @return auto Resolved function object.
     */
    template <typename T>
    auto GetFunction(const std::string& functionName) const
    {
        return m_dll.get<T>(functionName);
    }

    /**
     * @brief Call a void-returning function from the loaded DLL.
     *
     * @tparam Args Argument types for the target function.
     * @param functionName Name of the symbol to resolve.
     * @param args Arguments to pass to the function.
     * @return auto Result of the function call.
     */
    template <typename... Args>
    auto CallFunction(const std::string& functionName, Args&&... args) const
    {
        auto func = GetFunction<std::function<void(Args...)>>(functionName);
        return func(std::forward<Args>(args)...);
    }

    /**
     * @brief Reload a new DLL into the loader.
     *
     * @param dllPath New DLL file path.
     */
    void Reload(std::string_view dllPath) { m_dll.load(dllPath.data()); }

    /**
     * @brief Unload the currently loaded DLL.
     */
    void Unload() { m_dll.unload(); }

private:
    boost::dll::shared_library m_dll;
};

class DllGetFunctionAbstract
{
public:
    using LuaResisterFunction = int (*)(lua_State* L);

    /**
     * @brief Virtual destructor.
     */
    virtual ~DllGetFunctionAbstract() = default;

    /**
     * @brief Get the Lua registration function for resolving DLL functions.
     *
     * @return LuaResisterFunction Lua C function pointer.
     */
    virtual LuaResisterFunction GetLuaDllGetFuction() const = 0;

    /**
     * @brief Get the Lua registration function for calling DLL functions.
     *
     * @return LuaResisterFunction Lua C function pointer.
     */
    virtual LuaResisterFunction GetLuaDllCallFuction() const = 0;

    /**
     * @brief Get the name identifier for this function registration.
     *
     * @return std::string_view Registration name.
     */
    virtual std::string_view Name() const = 0;
};

namespace detail
{
/**
 * @brief Convert the Lua stack value at @p index to the native parameter type @p T.
 *
 * Used to unpack the real argument values of `call_<name>` from the Lua stack
 * (positions 3..). The previous implementation default-constructed the argument
 * tuple and never read the stack, so every call passed zeros to the target.
 */
template <typename T>
T lua_arg_to(lua_State* L, int index)
{
    using D = std::decay_t<T>;
    if constexpr (std::is_same_v<D, bool>)
    {
        return static_cast<bool>(lua_toboolean(L, index));
    }
    else if constexpr (std::is_integral_v<D>)
    {
        return static_cast<D>(lua_tointeger(L, index));
    }
    else if constexpr (std::is_floating_point_v<D>)
    {
        return static_cast<D>(lua_tonumber(L, index));
    }
    else if constexpr (std::is_same_v<D, std::string>)
    {
        return std::string(luaL_checkstring(L, index));
    }
    else if constexpr (std::is_same_v<D, const char*>)
    {
        return lua_tostring(L, index);
    }
    else
    {
        return luaL_error(L, "DllLoader: unsupported native argument type at stack index %d", index), D{};
    }
}

// Unpack Lua stack arguments (positions 3..) and invoke the resolved native function.
template <typename Ret, typename... Args, std::size_t... I>
Ret call_dll(Ret(*func)(Args...), lua_State* L, std::index_sequence<I...>)
{
    return func(lua_arg_to<Args>(L, static_cast<int>(3 + I))...);
}

template <typename Ret, typename... Args>
int lua_dll_get_function(lua_State* L)
{
    auto* dll = (DllLoader*)lua_topointer(L, 1);
    std::string function_name = luaL_checkstring(L, 2);
    if (!dll)
    {
        lua_pushstring(L, std::format("Invalid DllLoader object").c_str());
        lua_error(L);
        return 0;
    }
    try
    {
        // Resolve the exported function via its function-type signature.
        // `get<Ret(Args...)>` returns a reference to the function, which `auto`
        // decays to a real function pointer; `get<Ret(*)(Args...)>` (the previous
        // code) would instead read the symbol as a data object holding a pointer
        // (see the note in pointcloud/UserCustomPointCloudModel.cpp).
        auto func = dll->GetFunction<Ret(Args...)>(function_name);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(func));
        return 1;
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
        lua_error(L);
        return 0;
    }
}
template <typename Ret, typename... Args>
int lua_dll_call_function(lua_State* L)
{
    auto* dll = (DllLoader*)lua_topointer(L, 1);
    std::string function_name = luaL_checkstring(L, 2);
    // arguments start from index 3

    if (!dll)
    {
        lua_pushstring(L, std::format("Invalid DllLoader object").c_str());
        lua_error(L);
        return 0;
    }

    int top = lua_gettop(L);
    if (sizeof...(Args) != static_cast<std::size_t>(top - 2))
    {
        lua_pushstring(L, std::format("Expected {} arguments but got {}", sizeof...(Args), top - 2).c_str());
        lua_error(L);
        return 0;
    }
    try
    {
        // Resolve the exported function by its function-type signature; `auto`
        // decays the returned function reference into a real `Ret(*)(Args...)`
        // pointer (see the note in pointcloud/UserCustomPointCloudModel.cpp).
        auto func = dll->GetFunction<Ret(Args...)>(function_name);
        if constexpr (std::is_void_v<Ret>)
        {
            call_dll<Ret, Args...>(func, L, std::index_sequence_for<Args...>{});
            return 0;
        }
        else
        {
            auto result = call_dll<Ret, Args...>(func, L, std::index_sequence_for<Args...>{});
            if constexpr (std::is_arithmetic_v<Ret>)
            {
                lua_pushnumber(L, static_cast<lua_Number>(result));
            }
            else if constexpr (std::is_constructible_v<std::string, Ret>)
            {
                lua_pushstring(L, std::string(result).c_str());
            }
            else
            {
                // Push as AnyObject
                NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, result);
            }
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
        lua_error(L);
        return 0;
    }
}

template <Utils::TemplateString TName, typename Ret, typename... Args>
class LuaDllGetFunction : public DllGetFunctionAbstract
{
public:
    virtual LuaResisterFunction GetLuaDllGetFuction() const override { return lua_dll_get_function<Ret, Args...>; }
    virtual LuaResisterFunction GetLuaDllCallFuction() const override { return lua_dll_call_function<Ret, Args...>; }
    virtual std::string_view Name() const override { return TName.ToStringView(); }
};
}  // namespace detail

/**
 * @brief Register DLL loader helper functions in a Lua state.
 *
 * @param L Lua state where the functions will be registered.
 * @param get_function_registers Collection of DLL function registration helpers.
 */
void RegisterLuaDllLoader(lua_State* L, std::vector<std::unique_ptr<DllGetFunctionAbstract>>&& get_function_registers);
}  // namespace HsBa::Slicer

#endif  // !HSBA_NO_DLL_LOADER

#endif  // !HSBA_LUADLLLOADER_HPP