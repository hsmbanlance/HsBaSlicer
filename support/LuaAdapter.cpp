#include "LuaAdapter.hpp"

#include <memory>
#include <string>

#include "2D/LuaAdapter.hpp"
#include "FdmSupport.hpp"
#include "LuaSupport.hpp"
#include "OverhangDetector.hpp"
#include "SlaSupport.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer::Support
{
namespace
{
// Type tag for support userdata
enum class SupportType : int
{
    Plane = 0,
    Tree = 1,
    Honeycomb = 2,
    SLA = 3,
    LuaInline = 4,
    LuaFile = 5
};

struct LuaSupportWrapper
{
    SupportType type;
    std::unique_ptr<ISupport> impl;

    ~LuaSupportWrapper() = default;
};

constexpr const char* SUPPORT_MT = "HsBaSupport";

SupportConfig ReadConfigFromTable(lua_State* L, int idx)
{
    SupportConfig cfg;
    if (!lua_istable(L, idx))
        return cfg;

    lua_getfield(L, idx, "overhang_angle_threshold");
    if (lua_isnumber(L, -1))
        cfg.overhang_angle_threshold = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "layer_height");
    if (lua_isnumber(L, -1))
        cfg.layer_height = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "support_gap");
    if (lua_isnumber(L, -1))
        cfg.support_gap = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "support_diameter");
    if (lua_isnumber(L, -1))
        cfg.support_diameter = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "support_density");
    if (lua_isnumber(L, -1))
        cfg.support_density = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "tree_branch_angle");
    if (lua_isnumber(L, -1))
        cfg.tree_branch_angle = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "tree_max_branch_radius");
    if (lua_isnumber(L, -1))
        cfg.tree_max_branch_radius = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "honeycomb_cell_size");
    if (lua_isnumber(L, -1))
        cfg.honeycomb_cell_size = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    return cfg;
}

// Lua: Support.new_plane()
int l_new_plane(lua_State* L)
{
    auto* wrapper = NewLuaObject<LuaSupportWrapper>(L, SUPPORT_MT);
    wrapper->type = SupportType::Plane;
    wrapper->impl = std::make_unique<FdmPlaneSupport>();
    return 1;
}

// Lua: Support.new_tree()
int l_new_tree(lua_State* L)
{
    auto* wrapper = NewLuaObject<LuaSupportWrapper>(L, SUPPORT_MT);
    wrapper->type = SupportType::Tree;
    wrapper->impl = std::make_unique<FdmTreeSupport>();
    return 1;
}

// Lua: Support.new_honeycomb()
int l_new_honeycomb(lua_State* L)
{
    auto* wrapper = NewLuaObject<LuaSupportWrapper>(L, SUPPORT_MT);
    wrapper->type = SupportType::Honeycomb;
    wrapper->impl = std::make_unique<FdmHoneycombSupport>();
    return 1;
}

// Lua: Support.new_sla()
int l_new_sla(lua_State* L)
{
    auto* wrapper = NewLuaObject<LuaSupportWrapper>(L, SUPPORT_MT);
    wrapper->type = SupportType::SLA;
    wrapper->impl = std::make_unique<SlaSacrificialSupport>();
    return 1;
}

// Lua: Support.new_lua(script [, funcName])
int l_new_lua(lua_State* L)
{
    const char* script = luaL_checkstring(L, 1);
    std::string_view funcName = "generate_support";
    if (lua_gettop(L) >= 2 && lua_isstring(L, 2))
        funcName = lua_tostring(L, 2);

    auto* wrapper = NewLuaObject<LuaSupportWrapper>(L, SUPPORT_MT);
    wrapper->type = SupportType::LuaInline;
    wrapper->impl = std::make_unique<LuaSupport>(std::string_view(script), funcName);
    return 1;
}

// Lua: Support.new_lua_file(path, funcName)
int l_new_lua_file(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char* funcName = luaL_checkstring(L, 2);

    auto* wrapper = NewLuaObject<LuaSupportWrapper>(L, SUPPORT_MT);
    wrapper->type = SupportType::LuaFile;
    wrapper->impl = std::make_unique<LuaSupport>(std::filesystem::path(path), std::string_view(funcName));
    return 1;
}

// Lua: Support.generate(support_obj, current_layer, prev_layer, layer_height, config_table)
int l_generate(lua_State* L)
{
    auto* wrapper = static_cast<LuaSupportWrapper*>(luaL_checkudata(L, 1, SUPPORT_MT));
    if (!wrapper || !wrapper->impl)
    {
        lua_pushstring(L, "Invalid support object");
        return lua_error(L);
    }

    PolygonsD current_layer = LuaTableToPolygonsD(L, 2);
    PolygonsD prev_layer = LuaTableToPolygonsD(L, 3);
    float layer_height = static_cast<float>(luaL_checknumber(L, 4));
    SupportConfig config = ReadConfigFromTable(L, 5);

    try
    {
        PolygonsD result = wrapper->impl->Generate(current_layer, prev_layer, layer_height, config);
        PushPolygonsDToLua(L, result);
        return 1;
    }
    catch (const std::exception& e)
    {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

// Lua: Support.detect_overhang(current_layer, prev_layer, layer_height, angle_deg)
int l_detect_overhang(lua_State* L)
{
    PolygonsD current_layer = LuaTableToPolygonsD(L, 1);
    PolygonsD prev_layer = LuaTableToPolygonsD(L, 2);
    float layer_height = static_cast<float>(luaL_checknumber(L, 3));
    float angle_deg = static_cast<float>(luaL_checknumber(L, 4));

    PolygonsD result = OverhangDetector::Detect(current_layer, prev_layer, layer_height, angle_deg);
    PushPolygonsDToLua(L, result);
    return 1;
}

// Lua: Support.default_config() -> config table
int l_default_config(lua_State* L)
{
    SupportConfig cfg;
    lua_newtable(L);

    lua_pushnumber(L, cfg.overhang_angle_threshold);
    lua_setfield(L, -2, "overhang_angle_threshold");
    lua_pushnumber(L, cfg.layer_height);
    lua_setfield(L, -2, "layer_height");
    lua_pushnumber(L, cfg.support_gap);
    lua_setfield(L, -2, "support_gap");
    lua_pushnumber(L, cfg.support_diameter);
    lua_setfield(L, -2, "support_diameter");
    lua_pushnumber(L, cfg.support_density);
    lua_setfield(L, -2, "support_density");
    lua_pushinteger(L, cfg.support_pattern);
    lua_setfield(L, -2, "support_pattern");
    lua_pushnumber(L, cfg.tree_branch_angle);
    lua_setfield(L, -2, "tree_branch_angle");
    lua_pushnumber(L, cfg.tree_max_branch_radius);
    lua_setfield(L, -2, "tree_max_branch_radius");
    lua_pushnumber(L, cfg.honeycomb_cell_size);
    lua_setfield(L, -2, "honeycomb_cell_size");

    return 1;
}

int l_support_gc(lua_State* L)
{
    auto* wrapper = static_cast<LuaSupportWrapper*>(luaL_checkudata(L, 1, SUPPORT_MT));
    if (wrapper)
        wrapper->~LuaSupportWrapper();
    return 0;
}

static const luaL_Reg supportLib[] = {{"new_plane", l_new_plane},
                                      {"new_tree", l_new_tree},
                                      {"new_honeycomb", l_new_honeycomb},
                                      {"new_sla", l_new_sla},
                                      {"new_lua", l_new_lua},
                                      {"new_lua_file", l_new_lua_file},
                                      {"generate", l_generate},
                                      {"detect_overhang", l_detect_overhang},
                                      {"default_config", l_default_config},
                                      {nullptr, nullptr}};
}  // namespace

void RegisterLuaSupport(lua_State* L)
{
    // Create metatable for support objects
    luaL_newmetatable(L, SUPPORT_MT);
    lua_pushcfunction(L, l_support_gc);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // Register Support library
    luaL_newlib(L, supportLib);
    lua_setglobal(L, "Support");
}
}  // namespace HsBa::Slicer::Support
