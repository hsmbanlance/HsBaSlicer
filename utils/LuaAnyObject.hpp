#pragma once
#ifndef HSBA_SLICER_LUA_ANY_OBJECT_HPP
#define HSBA_SLICER_LUA_ANY_OBJECT_HPP

#include <functional>
#include <vector>

#include "LuaNewObject.hpp"
#include "base/any_object.hpp"

/**
 * @file LuaAnyObject.hpp
 * @brief Helpers to expose `AnyObject` values into Lua and convert between AnyObject and Lua types.
 *
 * This header provides adapter types and registration utilities to create and cast
 * `Utils::AnyObject` values from Lua and to push native values back to Lua.
 */

namespace HsBa::Slicer
{
/**
 * @brief Metatable/type name used for wrapped `AnyObject` instances in Lua.
 */
constexpr Utils::TemplateString AnyObjectTypeName = "AnyObject";

/**
 * @brief Base interface providing new/cast function pairs for exposing types to Lua.
 *
 * Implementations provide two Lua-callable functions: one to create a new Lua value
 * from a native object and one to cast an `AnyObject` back to the native type.
 */
class LuaAnyObjectNewCastBase
{
public:
    typedef int (*LuaFunc)(lua_State* L);
    struct LuaFuncPair
    {
        std::string name;
        LuaFunc func;
    };
    virtual LuaFuncPair GetNewFuncPair() const = 0;
    virtual LuaFuncPair GetCastFuncPair() const = 0;
    virtual ~LuaAnyObjectNewCastBase() = default;
};

/**
 * @brief CRTP helper that implements common plumbing for new/cast function retrieval.
 *
 * @tparam Derived Concrete implementation providing `type_name_impl`, `new_func_impl` and `cast_func_impl`.
 */
template <typename Derived>
class LuaAnyObjectNewCastAbstract : public LuaAnyObjectNewCastBase
{
protected:
    const std::string type_name() const { return static_cast<const Derived*>(this)->type_name_impl(); }

public:
    using LuaFunc = LuaAnyObjectNewCastBase::LuaFunc;
    using LuaFuncPair = LuaAnyObjectNewCastBase::LuaFuncPair;
    LuaFunc new_func() const { return reinterpret_cast<LuaFunc>(&Derived::new_func_impl); }
    LuaFunc cast_func() const { return reinterpret_cast<LuaFunc>(&Derived::cast_func_impl); }
    LuaFuncPair GetNewFuncPair() const override { return {"new_" + std::string(type_name()), new_func()}; }
    LuaFuncPair GetCastFuncPair() const override { return {"cast_" + std::string(type_name()), cast_func()}; }
};

/**
 * @brief Default implementation adapter that bridges a native type `T` and a Lua representation.
 *
 * @tparam T Native type.
 * @tparam TypeName Compile-time string used as the Lua-visible type name.
 */
template <typename T, Utils::TemplateString TypeName>
class LuaAnyObjectNewCastImpl : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<T, TypeName>>
{
public:
    const std::string type_name_impl() const { return TypeName.ToString(); }
    static int new_func_impl(lua_State* L)
    {
        auto* obj = (T*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid {} object", TypeName).c_str());
            lua_error(L);
            return 0;
        }
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, *obj);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            T value = obj->cast_new<T>();
            NewLuaObject<T, TypeName>(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

// Specializations for basic Lua types
/**
 * @brief Convenience aliases for common Lua/native type adapters.
 */
using LuaInt = LuaAnyObjectNewCastImpl<int, "int">;
using LuaLongLong = LuaAnyObjectNewCastImpl<long long, "longlong">;
using LuaLong = LuaAnyObjectNewCastImpl<long, "long">;
using LuaSize_t = LuaAnyObjectNewCastImpl<size_t, "size_t">;
using LuaDouble = LuaAnyObjectNewCastImpl<double, "double">;
using LuaFloat = LuaAnyObjectNewCastImpl<float, "float">;
using LuaBool = LuaAnyObjectNewCastImpl<bool, "bool">;
using LuaString = LuaAnyObjectNewCastImpl<std::string, "string">;
using LuaCString = LuaAnyObjectNewCastImpl<const char*, "cstring">;

template <>
class LuaAnyObjectNewCastImpl<int, "int"> : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<int, "int">>
{
public:
    const std::string type_name_impl() const { return "int"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isinteger(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "int").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<int>(lua_tointeger(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<int>();
            lua_pushinteger(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<long, "long"> : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<long, "long">>
{
public:
    const std::string type_name_impl() const { return "long"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isinteger(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "long").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<long>(lua_tointeger(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<long>();
            lua_pushinteger(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<long long, "longlong">
    : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<long, "long">>
{
public:
    const std::string type_name_impl() const { return "longlong"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isinteger(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "longlong").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<long long>(lua_tointeger(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<long long>();
            lua_pushinteger(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<size_t, "size_t">
    : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<size_t, "size_t">>
{
public:
    const std::string type_name_impl() const { return "size_t"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isinteger(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "size_t").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<size_t>(lua_tointeger(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<size_t>();
            lua_pushinteger(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<float, "float">
    : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<float, "float">>
{
public:
    const std::string type_name_impl() const { return "float"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isnumber(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "float").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<float>(lua_tonumber(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<float>();
            lua_pushnumber(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<double, "double">
    : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<double, "double">>
{
public:
    const std::string type_name_impl() const { return "double"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isnumber(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "double").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<double>(lua_tonumber(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<double>();
            lua_pushnumber(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<bool, "bool"> : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<bool, "bool">>
{
public:
    const std::string type_name_impl() const { return "bool"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isboolean(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "bool").c_str());
            lua_error(L);
            return 0;
        }
        auto v = static_cast<bool>(lua_toboolean(L, 1));
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<bool>();
            lua_pushboolean(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<std::string, "string">
    : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<std::string, "string">>
{
public:
    const std::string type_name_impl() const { return "string"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isstring(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "string").c_str());
            lua_error(L);
            return 0;
        }
        std::string v = lua_tostring(L, 1);
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<std::string>();
            lua_pushstring(L, value.c_str());
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

template <>
class LuaAnyObjectNewCastImpl<const char*, "cstring">
    : public LuaAnyObjectNewCastAbstract<LuaAnyObjectNewCastImpl<const char*, "cstring">>
{
public:
    const std::string type_name_impl() const { return "cstring"; }
    static int new_func_impl(lua_State* L)
    {
        if (!lua_isstring(L, 1))
        {
            lua_pushstring(L, std::format("Invalid {} object", "cstring").c_str());
            lua_error(L);
            return 0;
        }
        const char* v = lua_tostring(L, 1);
        NewLuaObject<Utils::AnyObject, AnyObjectTypeName>(L, v);
        return 1;
    }
    static int cast_func_impl(lua_State* L)
    {
        auto* obj = (Utils::AnyObject*)lua_topointer(L, 1);
        if (!obj)
        {
            lua_pushstring(L, std::format("Invalid AnyObject object").c_str());
            lua_error(L);
            return 0;
        }
        try
        {
            // use cast_new to avoid double deconstruction in _gc metamethod
            auto value = obj->cast_new<const char*>();
            lua_pushstring(L, value);
            return 1;
        }
        catch (const std::exception& e)
        {
            lua_pushstring(L, e.what());
            lua_error(L);
            return 0;
        }
    }
};

/**
 * @brief Register provided AnyObject new/cast adapters in the given Lua state.
 *
 * This registers the adapter functions so Lua code can create native values from
 * `AnyObject` instances and vice versa.
 *
 * @param L Lua state to register into.
 * @param added_types List of type adapters to register.
 */
void RegisterAnyObject(lua_State* L, const std::vector<LuaAnyObjectNewCastBase*>& added_types);
}  // namespace HsBa::Slicer
#endif  // HSBA_SLICER_LUA_ANY_OBJECT_HPP