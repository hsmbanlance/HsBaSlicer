#pragma once
#ifndef FILEOPERATOR_LUAADAPTER_HPP
#define FILEOPERATOR_LUAADAPTER_HPP

#include <lua.hpp>

#include "zipper.hpp"
#include "unzipper.hpp"
#include "bit7z_zipper.hpp"
#include "bit7z_unzipper.hpp"
#include "sql_adapter.hpp"

namespace HsBa::Slicer
{
	void PushAnyToLua(lua_State* L, const std::any& value);
	void RegisterLuaZipper(lua_State* L);
	void RegisterLuaUnzipper(lua_State* L);
	void RegisterLuaSQLiteAdapter(lua_State* L);
#ifdef USE_MYSQL
	void RegisterLuaMySQLAdapter(lua_State* L);
#endif // USE_MYSQL
#ifdef USE_PGSQL
	void RegisterLuaPostgreSQLAdapter(lua_State* L);
#endif // USE_PGSQL
#ifdef USE_BIT7Z
	void RegisterLuaBit7zZipper(lua_State* L);
	void RegisterLuaBit7zUnzipper(lua_State* L);
#endif // USE_BIT7Z
}// namespace HsBa::Slicer
#endif // FILEOPERATOR_LUAADAPTER_HPP