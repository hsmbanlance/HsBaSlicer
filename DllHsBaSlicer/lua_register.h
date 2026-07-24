#pragma once
#ifndef HSBA_SLICER_LUA_REGISTER_H
#define HSBA_SLICER_LUA_REGISTER_H

#include "dllexport.h"

struct lua_State;

/// C-compatible Lua registration function pointer type.
typedef void (*HsBaLuaRegFn)(lua_State*);

#if __cplusplus
extern "C"
{
#endif  // __cplusplus

    /// Register an external 2D function for Lua pipeline stages (Support, Fill, SLA Output).
    HSBA_SLICER_API void HsBaAdd2DFunction(HsBaLuaRegFn func);

    /// Register an external 3D function for Lua pipeline stages (Slice, Support).
    HSBA_SLICER_API void HsBaAdd3DFunction(HsBaLuaRegFn func);

    /// Register an external File function for Lua pipeline stages (SLS Output, SLA Output).
    HSBA_SLICER_API void HsBaAddFileFunction(HsBaLuaRegFn func);

    /// Register an event callback by name (e.g. "zipper.on_add", "db.on_query").
    HSBA_SLICER_API void HsBaAddEventCallback(const char* event_name, HsBaLuaRegFn func);

#if __cplusplus
}
#endif  // __cplusplus

#endif  // !HSBA_SLICER_LUA_REGISTER_H
