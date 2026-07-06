#pragma once
#ifndef FILEOPERATOR_LUAADAPTER_HPP
#define FILEOPERATOR_LUAADAPTER_HPP

#include <lua.hpp>

#include "base/template_helper.hpp"
#include "bit7z_unzipper.hpp"
#include "bit7z_zipper.hpp"
#include "sql_adapter.hpp"
#include "unzipper.hpp"
#include "utils/LuaNewObject.hpp"
#include "zipper.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Push std::any value to Lua stack with type conversion.
 * @param L Lua state pointer.
 * @param value The any value to push.
 */
void PushAnyToLua(lua_State* L, const std::any& value);

/**
 * @brief Register zipper classes and functions in Lua.
 * @param L Lua state pointer.
 */
void RegisterLuaZipper(lua_State* L);

/**
 * @brief Register SQLite adapter in Lua.
 * @param L Lua state pointer.
 */
void RegisterLuaSQLiteAdapter(lua_State* L);

#ifdef HSBA_USE_MYSQL
/**
 * @brief Register MySQL adapter in Lua (if MySQL support is enabled).
 * @param L Lua state pointer.
 */
void RegisterLuaMySQLAdapter(lua_State* L);
#endif  // HSBA_USE_MYSQL

#ifdef HSBA_USE_PGSQL
/**
 * @brief Register PostgreSQL adapter in Lua (if PostgreSQL support is enabled).
 * @param L Lua state pointer.
 */
void RegisterLuaPostgreSQLAdapter(lua_State* L);
#endif  // HSBA_USE_PGSQL

#ifdef HSBA_USE_BIT7Z
/**
 * @brief Register Bit7z-based zipper/unzipper in Lua (if Bit7z support is enabled).
 * @param L Lua state pointer.
 */
void RegisterLuaBit7zZipper(lua_State* L);
#endif  // HSBA_USE_BIT7Z
}  // namespace HsBa::Slicer
#endif  // FILEOPERATOR_LUAADAPTER_HPP