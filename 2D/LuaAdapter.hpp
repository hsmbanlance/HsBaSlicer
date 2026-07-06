#pragma once
#ifndef HSBA_SLICER_LUAADAPTERS_HPP
#define HSBA_SLICER_LUAADAPTERS_HPP

#include <lua.hpp>

#include "FloatPolygons.hpp"
#include "IntPolygon.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Push a double-precision polygon to Lua stack.
 * @param L Lua state pointer.
 * @param poly Polygon to push.
 */
void PushPolygonDToLua(lua_State* L, const PolygonD& poly);

/**
 * @brief Push multiple double-precision polygons to Lua stack.
 * @param L Lua state pointer.
 * @param poly Polygons to push.
 */
void PushPolygonsDToLua(lua_State* L, const PolygonsD& poly);

/**
 * @brief Push an integer polygon to Lua stack.
 * @param L Lua state pointer.
 * @param poly Polygon to push.
 */
void PushPolygonToLua(lua_State* L, const Polygon& poly);

/**
 * @brief Push multiple integer polygons to Lua stack.
 * @param L Lua state pointer.
 * @param poly Polygons to push.
 */
void PushPolygonsToLua(lua_State* L, const Polygons& poly);

/**
 * @brief Convert a Lua table to a double-precision polygon.
 * @param L Lua state pointer.
 * @param index Stack index of the Lua table.
 * @return Converted polygon.
 */
PolygonD LuaTableToPolygonD(lua_State* L, int index);

/**
 * @brief Convert a Lua table to multiple double-precision polygons.
 * @param L Lua state pointer.
 * @param index Stack index of the Lua table.
 * @return Converted polygons.
 */
PolygonsD LuaTableToPolygonsD(lua_State* L, int index);

/**
 * @brief Convert a Lua table to an integer polygon.
 * @param L Lua state pointer.
 * @param index Stack index of the Lua table.
 * @return Converted polygon.
 */
Polygon LuaTableToPolygon(lua_State* L, int index);

/**
 * @brief Convert a Lua table to multiple integer polygons.
 * @param L Lua state pointer.
 * @param index Stack index of the Lua table.
 * @return Converted polygons.
 */
Polygons LuaTableToPolygons(lua_State* L, int index);

// Helper: register Lua functions for polygon operations
/**
 * @brief Register all polygon operation functions in Lua.
 * @param L Lua state pointer.
 */
void RegisterLuaPolygonOperations(lua_State* L);
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_LUAADAPTERS_HPP