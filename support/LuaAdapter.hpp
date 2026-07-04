#pragma once
#ifndef HSBA_SLICER_SUPPORT_LUAADAPTER_HPP
#define HSBA_SLICER_SUPPORT_LUAADAPTER_HPP

#include <lua.hpp>

namespace HsBa::Slicer::Support
{
/**
 * @brief Register support module functions in Lua.
 *
 * Creates a global "Support" table with:
 * - Support.new_plane() -> FdmPlaneSupport userdata
 * - Support.new_tree() -> FdmTreeSupport userdata
 * - Support.new_honeycomb() -> FdmHoneycombSupport userdata
 * - Support.new_sla() -> SlaSacrificialSupport userdata
 * - Support.new_lua(script) -> LuaSupport userdata
 * - Support.new_lua_file(path, funcName) -> LuaSupport userdata
 * - Support.generate(support_obj, current_layer, prev_layer, layer_height, config_table) -> polygons table
 * - Support.detect_overhang(current_layer, prev_layer, layer_height, angle) -> polygons table
 *
 * Also registers SupportConfig metatable for config table creation.
 *
 * @param L Lua state pointer.
 */
void RegisterLuaSupport(lua_State* L);
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_SUPPORT_LUAADAPTER_HPP
