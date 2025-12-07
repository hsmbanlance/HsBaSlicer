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
	template<typename T, typename... Args>
	inline T* NewLuaObject(lua_State* L, const char* mt, Args&&... args)
	{
		void* ud = lua_newuserdata(L, sizeof(T));          // 1. 分配
		T* obj = new (ud) T(std::forward<Args>(args)...);// 2. 构造
		luaL_getmetatable(L, mt);                          // 3. 绑定元表
		lua_setmetatable(L, -2);
		return obj;
	}

	template<typename T>
	int LuaGC(lua_State* L) noexcept
	{
		void* ud = luaL_checkudata(L, 1, typeid(T).name());
		if (ud) static_cast<T*>(ud)->~T();   // 显式析构
		return 0;
	}
	void PushAnyToLua(lua_State* L, const std::any& value);
	void RegisterLuaZipper(lua_State* L);
	void RegisterLuaSQLiteAdapter(lua_State* L);
#ifdef USE_MYSQL
	void RegisterLuaMySQLAdapter(lua_State* L);
#endif // USE_MYSQL
#ifdef USE_PGSQL
	void RegisterLuaPostgreSQLAdapter(lua_State* L);
#endif // USE_PGSQL
#ifdef USE_BIT7Z
	void RegisterLuaBit7zZipper(lua_State* L);
#endif // USE_BIT7Z
}// namespace HsBa::Slicer
#endif // FILEOPERATOR_LUAADAPTER_HPP