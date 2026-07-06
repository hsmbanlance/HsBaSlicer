#pragma once
#ifndef HSBA_LUADLLLOADER_HPP
#define HSBA_LUADLLLOADER_HPP

#ifndef HSBA_NO_DLL_LOADER

#include <boost/dll.hpp>

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
template <typename Ret, typename... Args>
int lua_dll_get_function(lua_State* L)
{
    using FuncType = Ret (*)(Args...);
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
        auto func = dll->GetFunction<FuncType>(function_name);
        // Push the function pointer as lightuserdata
        lua_pushlightuserdata(L, (void*)func.template target<void*>());
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
    // table of arguments starts from index 3

    if (!dll)
    {
        lua_pushstring(L, std::format("Invalid DllLoader object").c_str());
        lua_error(L);
        return 0;
    }

    // Collect arguments
    std::tuple<std::decay_t<Args>...> args;
    int top = lua_gettop(L);
    if (sizeof...(Args) != top - 2)
    {
        lua_pushstring(L, std::format("Expected {} arguments but got {}", sizeof...(Args), top - 2).c_str());
        lua_error(L);
        return 0;
    }
    try
    {
        auto func = dll->GetFunction<std::function<Ret(Args...)>>(function_name);
        // Call the function with collected arguments
        auto result = std::apply(func, args);
        // Push result back to Lua
        if constexpr (!std::is_same_v<Ret, void>)
        {
            // For simplicity, assume result is a string or number
            if constexpr (std::is_arithmetic_v<Ret>)
            {
                lua_pushnumber(L, result);
            }
            else if constexpr (std::is_convertible_v<Ret, std::string>)
            {
                lua_pushstring(L, result.c_str());
            }
            else
            {
                // Push as AnyObject
                NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, result);
            }
            return 1;
        }
        else
        {
            return 0;
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
    virtual std::string_view Name() const override { return TName; }
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