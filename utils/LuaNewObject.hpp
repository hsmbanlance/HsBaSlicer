#pragma once
#ifndef HSBA_LUANEWOBJECT_HPP
#define HSBA_LUANEWOBJECT_HPP

#include <lua.hpp>

#include "base/template_helper.hpp"

namespace HsBa::Slicer
{
	template<typename T, typename... Args>
	inline T* NewLuaObject(lua_State* L, const char* mt, Args&&... args)
	{
		void* ud = lua_newuserdata(L, sizeof(T));
		T* obj = new (ud) T(std::forward<Args>(args)...);
		luaL_getmetatable(L, mt);
		lua_setmetatable(L, -2);
		return obj;
	}

	template<typename T, Utils::TemplateString TName, typename ...Args>
	inline T* NewLuaObject(lua_State* L, Args&& ...args)
	{
		void* ud = lua_newuserdata(L, sizeof(T));
		T* obj = new (ud) T(std::forward<Args>(args)...);
		luaL_getmetatable(L, static_cast<const char*>(TName));
		lua_setmetatable(L, -2);
		return obj;
	}

	template<typename T>
	int LuaGC(lua_State* L, const char* mt) noexcept
	{
		void* ud = luaL_checkudata(L, 1, mt);
		if (ud) static_cast<T*>(ud)->~T();
		return 0;
	}

	template<typename T, Utils::TemplateString TName>
	int LuaGC(lua_State* L) noexcept
	{
		void* ud = luaL_checkudata(L, 1, static_cast<const char*>(TName));
		if (ud) static_cast<T*>(ud)->~T();
		return 0;
	}

	struct LuaStateDeleter
	{
		void operator()(lua_State* L) const noexcept
		{
			if (L) lua_close(L);
		}
	};

	using UniqueLua = std::unique_ptr<lua_State, LuaStateDeleter>;

	inline UniqueLua MakeUniqueLuaState()
	{
		lua_State* L = luaL_newstate();
		return UniqueLua{ L };
	}
} // namespace HsBa::Slicer

#endif // !HSBA_LUANEWOBJECT_HPP