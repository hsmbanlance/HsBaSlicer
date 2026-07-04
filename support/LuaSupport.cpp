#include "LuaSupport.hpp"

#include <lua.hpp>

#include "2D/LuaAdapter.hpp"
#include "base/error.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer::Support
{
namespace
{
void PushConfigToLua(lua_State* L, const SupportConfig& config)
{
    lua_newtable(L);

    lua_pushnumber(L, config.overhang_angle_threshold);
    lua_setfield(L, -2, "overhang_angle_threshold");

    lua_pushnumber(L, config.layer_height);
    lua_setfield(L, -2, "layer_height");

    lua_pushnumber(L, config.support_gap);
    lua_setfield(L, -2, "support_gap");

    lua_pushnumber(L, config.support_diameter);
    lua_setfield(L, -2, "support_diameter");

    lua_pushnumber(L, config.support_density);
    lua_setfield(L, -2, "support_density");

    lua_pushinteger(L, config.support_pattern);
    lua_setfield(L, -2, "support_pattern");

    lua_pushnumber(L, config.tree_branch_angle);
    lua_setfield(L, -2, "tree_branch_angle");

    lua_pushnumber(L, config.tree_max_branch_radius);
    lua_setfield(L, -2, "tree_max_branch_radius");

    lua_pushnumber(L, config.honeycomb_cell_size);
    lua_setfield(L, -2, "honeycomb_cell_size");
}

PolygonsD GetResultFromLua(lua_State* L)
{
    // Check return value on top of stack, or global 'support_polys'
    int top = lua_gettop(L);
    if (top == 0 || !lua_istable(L, -1))
    {
        lua_getglobal(L, "support_polys");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return {};
        }
    }
    return LuaTableToPolygonsD(L, -1);
}

void SetupLuaEnvironment(lua_State* L, const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                         const SupportConfig& config)
{
    luaL_openlibs(L);

    // Register polygon helper functions
    RegisterLuaPolygonOperations(L);

    // Push layer data as globals
    PushPolygonsDToLua(L, current_layer);
    lua_setglobal(L, "current_layer");

    PushPolygonsDToLua(L, prev_layer);
    lua_setglobal(L, "prev_layer");

    lua_pushnumber(L, layer_height);
    lua_setglobal(L, "layer_height");

    PushConfigToLua(L, config);
    lua_setglobal(L, "config");
}
}  // namespace

LuaSupport::LuaSupport(std::string_view script)
    : source_type_(SourceType::Inline), script_(script), func_name_("generate_support")
{
}

LuaSupport::LuaSupport(std::string_view script, std::string_view funcName)
    : source_type_(SourceType::Inline), script_(script), func_name_(funcName)
{
}

LuaSupport::LuaSupport(const std::filesystem::path& script_file, std::string_view funcName)
    : source_type_(SourceType::File), script_file_(script_file), func_name_(funcName)
{
}

PolygonsD LuaSupport::Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                               const SupportConfig& config)
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("LuaSupport: failed to create Lua state");

    SetupLuaEnvironment(L.get(), current_layer, prev_layer, layer_height, config);

    // Load script
    int loadStatus = LUA_OK;
    if (source_type_ == SourceType::File)
    {
        loadStatus = luaL_loadfile(L.get(), script_file_.string().c_str());
    }
    else
    {
        loadStatus = luaL_loadbuffer(L.get(), script_.data(), script_.size(), "LuaSupportScript");
    }

    if (loadStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("LuaSupport load error: " + err);
    }

    // Execute script chunk
    int callStatus = lua_pcall(L.get(), 0, LUA_MULTRET, 0);
    if (callStatus != LUA_OK)
    {
        const char* es = lua_tostring(L.get(), -1);
        std::string err = es ? es : "<lua error>";
        throw RuntimeError("LuaSupport runtime error: " + err);
    }

    // If function name specified, call it
    if (!func_name_.empty())
    {
        lua_getglobal(L.get(), func_name_.c_str());
        if (lua_isfunction(L.get(), -1))
        {
            int funcCallStatus = lua_pcall(L.get(), 0, 1, 0);
            if (funcCallStatus != LUA_OK)
            {
                const char* es = lua_tostring(L.get(), -1);
                std::string err = es ? es : "<lua error>";
                throw RuntimeError("LuaSupport function call error: " + err);
            }
        }
        else
        {
            lua_pop(L.get(), 1);
        }
    }

    return GetResultFromLua(L.get());
}
}  // namespace HsBa::Slicer::Support
