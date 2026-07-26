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
using LuaRegFunc = std::function<void(lua_State*)>;

HSBA_SLICER_LIB_API void Add2DFunctions(LuaRegFunc func);

HSBA_SLICER_LIB_API void Add3DFunctions(LuaRegFunc func);

HSBA_SLICER_LIB_API void AddFileFunctions(LuaRegFunc func);

HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& Get2DFunctions();

HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& Get3DFunctions();

HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& GetFileFunctions();

// ===== Event callback registration (Zipper, DB, etc.) =====

HSBA_SLICER_LIB_API void AddEventCallback(const std::string& event_name, LuaRegFunc func);

HSBA_SLICER_LIB_API std::vector<LuaRegFunc>& GetEventCallbacks(const std::string& event_name);
}  // namespace HsBa::Slicer


#endif  //! HSBA_SLICER_LUAADDFUNCTION_HPP