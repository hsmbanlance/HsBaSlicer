/** @file LuaAddFunction.hpp
 * @brief Lib-side registration of external Lua functions and event callbacks by pipeline dimension.
 * @author HsBa
 */
#pragma once

#ifndef HSBA_SLICER_LUAADDFUNCTION_HPP
#define HSBA_SLICER_LUAADDFUNCTION_HPP

#include <functional>
#include <string>
#include <vector>

#include "LibHsBaSlicer/export.h"

struct lua_State;


namespace HsBa::Slicer
{
/// Lua registration function type: receives a lua_State and installs library functions into it.
using LuaRegFunc = std::function<void(lua_State*)>;

/**
 * @brief Register an external 2D Lua function for pipeline stages (Support, Fill, SLA Output).
 * @param func Registration callback invoked with the target Lua state.
 */
HSBA_SLICER_LIB_API void Add2DFunctions(LuaRegFunc func);

/**
 * @brief Register an external 3D Lua function for pipeline stages (Slice, Support).
 * @param func Registration callback invoked with the target Lua state.
 */
HSBA_SLICER_LIB_API void Add3DFunctions(LuaRegFunc func);

/**
 * @brief Register an external File Lua function for pipeline stages (SLS Output, SLA Output).
 * @param func Registration callback invoked with the target Lua state.
 */
HSBA_SLICER_LIB_API void AddFileFunctions(LuaRegFunc func);

/**
 * @brief Get the list of registered 2D Lua registration functions.
 * @return Reference to the vector of 2D registration callbacks.
 */
HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& Get2DFunctions();

/**
 * @brief Get the list of registered 3D Lua registration functions.
 * @return Reference to the vector of 3D registration callbacks.
 */
HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& Get3DFunctions();

/**
 * @brief Get the list of registered File Lua registration functions.
 * @return Reference to the vector of File registration callbacks.
 */
HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& GetFileFunctions();

// ===== Event callback registration (Zipper, DB, etc.) =====

/**
 * @brief Register a Lua event callback under a named event (e.g. "zipper.on_add", "db.on_query").
 * @param event_name Event name to bind the callback to.
 * @param func Registration callback invoked with the target Lua state.
 */
HSBA_SLICER_LIB_API void AddEventCallback(const std::string& event_name, LuaRegFunc func);

/**
 * @brief Get the Lua registration callbacks bound to an event name.
 * @param event_name Event name to look up.
 * @return Reference to the vector of callbacks registered for the event.
 */
HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& GetEventCallbacks(const std::string& event_name);
}  // namespace HsBa::Slicer


#endif  //! HSBA_SLICER_LUAADDFUNCTION_HPP