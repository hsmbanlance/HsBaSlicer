#pragma once
#ifndef HSBA_SLICER_LUAADAPTERS_HPP
#define HSBA_SLICER_LUAADAPTERS_HPP

#include <lua.hpp>

#include "IntPolygon.hpp"
#include "FloatPolygons.hpp"

namespace HsBa::Slicer
{
	void PushPolygonDToLua(lua_State* L, const PolygonD& poly);
	void PushPolygonsDToLua(lua_State* L, const PolygonsD& poly);
	void PushPolygonToLua(lua_State* L, const Polygon& poly);
	void PushPolygonsToLua(lua_State* L, const Polygons& poly);
	PolygonD LuaTableToPolygonD(lua_State* L, int index);
	PolygonsD LuaTableToPolygonsD(lua_State* L, int index);
	Polygon LuaTableToPolygon(lua_State* L, int index);
	Polygons LuaTableToPolygons(lua_State* L, int index);

	// Helper: register Lua functions for polygon operations
	void RegisterLuaPolygonOperations(lua_State* L);
} // namespace HsBa::Slicer

#endif // HSBA_SLICER_LUAADAPTERS_HPP