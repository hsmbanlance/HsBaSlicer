#include "sla_floor.hpp"

#include <lua.hpp>

#include "2D/2Dhull.hpp"
#include "2D/LuaAdapter.hpp"
#include "2D/PolygonFill.hpp"
#include "base/error.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
static Polygons ComputeFootprint(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    if (config.use_convex_hull)
    {
        Polygon hull = ConvexHull(bottom_layer);
        return Polygons{hull};
    }
    if (config.concave_hull_points > 0)
    {
        Polygon hull = ConcaveHullSimulation(bottom_layer, config.concave_hull_points);
        return Polygons{hull};
    }
    return bottom_layer;
}

HSBA_SLICER_LIB_API Polygons GenerateFloorContact(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    return ComputeFootprint(bottom_layer, config);
}

HSBA_SLICER_LIB_API Polygons GenerateFloorBorder(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    Polygons footprint = ComputeFootprint(bottom_layer, config);

    // Outward offset to expand the footprint into raft area
    Polygons raft_outer = Offset(footprint, config.raft_offset + config.border_width,
                                 Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Inward offset to carve out the interior fill region
    Polygons raft_inner = Offset(raft_outer, config.border_width * config.border_count,
                                 Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Border = outer - inner
    return Difference(raft_outer, raft_inner);
}

HSBA_SLICER_LIB_API Polygons GenerateFloorFill(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    Polygons footprint = ComputeFootprint(bottom_layer, config);

    // Expand footprint to raft boundary
    Polygons raft_outer = Offset(footprint, config.raft_offset + config.border_width,
                                 Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Carve out border region so fill stays inside the border
    Polygons fill_region = Offset(raft_outer, -config.border_width * config.border_count,
                                  Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    if (fill_region.empty())
    {
        return {};
    }

    return ZigzagFill(fill_region, config.fill_spacing, config.fill_angle_deg);
}

HSBA_SLICER_LIB_API Polygons GenerateFloorRaft(const Polygons& bottom_layer, const SlaFloorConfig& config)
{
    Polygons footprint = ComputeFootprint(bottom_layer, config);

    // Step 1: Outward offset to create full raft boundary
    Polygons raft_outer = Offset(footprint, config.raft_offset + config.border_width,
                                 Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Step 2: Generate border loops using composite offset fill
    //   - border_count inward loops at border_width spacing
    //   - then zigzag fill for the interior
    Polygons border_loops = Offset(raft_outer, -config.border_width / 2.0,
                                   Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    // Step 3: Interior fill region (inside the last border loop)
    Polygons fill_region = Offset(raft_outer, -config.border_width * config.border_count,
                                  Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    Polygons result = std::move(border_loops);

    if (!fill_region.empty())
    {
        Polygons fill = ZigzagFill(fill_region, config.fill_spacing, config.fill_angle_deg);
        result.insert(result.end(), fill.begin(), fill.end());
    }

    return result;
}

namespace
{
void PushFloorConfigToLua(lua_State* L, const SlaFloorConfig& config)
{
    lua_newtable(L);

    lua_pushnumber(L, config.raft_offset);
    lua_setfield(L, -2, "raft_offset");

    lua_pushnumber(L, config.border_width);
    lua_setfield(L, -2, "border_width");

    lua_pushnumber(L, config.fill_spacing);
    lua_setfield(L, -2, "fill_spacing");

    lua_pushnumber(L, config.fill_angle_deg);
    lua_setfield(L, -2, "fill_angle_deg");

    lua_pushinteger(L, config.border_count);
    lua_setfield(L, -2, "border_count");

    lua_pushboolean(L, config.use_convex_hull ? 1 : 0);
    lua_setfield(L, -2, "use_convex_hull");

    lua_pushinteger(L, config.concave_hull_points);
    lua_setfield(L, -2, "concave_hull_points");
}
}  // anonymous namespace

HSBA_SLICER_LIB_API Polygons LuaCustomFloorByFile(
    const Polygons& bottom_layer, const std::string& script_path,
    const std::string& function_name, const SlaFloorConfig& config,
    const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Failed to create Lua state");

    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    RegisterLuaPolygonFillFunctions(L.get());
    if (lua_reg)
        lua_reg(L.get());

    // Load script file
    if (luaL_loadfile(L.get(), script_path.c_str()) || lua_pcall(L.get(), 0, 0, 0))
    {
        throw RuntimeError("Failed to load Lua floor script: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Get function
    lua_getglobal(L.get(), function_name.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua floor function not found: " + function_name);
    }

    // Push bottom layer polygons as first argument
    PolygonsD bottomD = UnIntegerization(bottom_layer);
    PushPolygonsDToLua(L.get(), bottomD);

    // Push config table as second argument
    PushFloorConfigToLua(L.get(), config);

    // Call function with 2 args, 1 result
    if (lua_pcall(L.get(), 2, 1, 0) != LUA_OK)
    {
        throw RuntimeError("Error calling Lua floor function: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Parse result table
    if (!lua_istable(L.get(), -1))
    {
        throw RuntimeError("Lua floor function did not return a table");
    }

    Polygons result;
    lua_pushnil(L.get());
    while (lua_next(L.get(), -2))
    {
        if (lua_istable(L.get(), -1))
        {
            PolygonD outpoly;
            int n = static_cast<int>(lua_rawlen(L.get(), -1));
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L.get(), -1, i);
                if (lua_istable(L.get(), -1))
                {
                    lua_getfield(L.get(), -1, "x");
                    lua_getfield(L.get(), -2, "y");
                    double x = lua_isnumber(L.get(), -2) ? lua_tonumber(L.get(), -2) : 0.0;
                    double y = lua_isnumber(L.get(), -1) ? lua_tonumber(L.get(), -1) : 0.0;
                    outpoly.emplace_back(Point2D{x, y});
                    lua_pop(L.get(), 2);
                }
                lua_pop(L.get(), 1);
            }
            if (!outpoly.empty())
            {
                result.push_back(Integerization(outpoly));
            }
        }
        lua_pop(L.get(), 1);
    }
    return result;
}

HSBA_SLICER_LIB_API Polygons LuaCustomFloorByString(
    const Polygons& bottom_layer, const std::string& lua_script,
    const std::string& function_name, const SlaFloorConfig& config,
    const std::function<void(lua_State*)>& lua_reg)
{
    auto L = MakeUniqueLuaState();
    if (!L)
        throw RuntimeError("Failed to create Lua state");

    luaL_openlibs(L.get());
    RegisterLuaPolygonOperations(L.get());
    RegisterLuaPolygonFillFunctions(L.get());
    if (lua_reg)
        lua_reg(L.get());

    // Load inline script
    if (luaL_loadstring(L.get(), lua_script.c_str()) != LUA_OK)
    {
        throw RuntimeError("Failed to load Lua floor script: " + std::string(lua_tostring(L.get(), -1)));
    }
    if (lua_pcall(L.get(), 0, 0, 0) != LUA_OK)
    {
        throw RuntimeError("Exec Lua floor script failed: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Get function
    lua_getglobal(L.get(), function_name.c_str());
    if (!lua_isfunction(L.get(), -1))
    {
        throw RuntimeError("Lua floor function not found: " + function_name);
    }

    // Push bottom layer polygons as first argument
    PolygonsD bottomD = UnIntegerization(bottom_layer);
    PushPolygonsDToLua(L.get(), bottomD);

    // Push config table as second argument
    PushFloorConfigToLua(L.get(), config);

    // Call function with 2 args, 1 result
    if (lua_pcall(L.get(), 2, 1, 0) != LUA_OK)
    {
        throw RuntimeError("Error calling Lua floor function: " + std::string(lua_tostring(L.get(), -1)));
    }

    // Parse result table
    if (!lua_istable(L.get(), -1))
    {
        throw RuntimeError("Lua floor function did not return a table");
    }

    Polygons result;
    lua_pushnil(L.get());
    while (lua_next(L.get(), -2))
    {
        if (lua_istable(L.get(), -1))
        {
            PolygonD outpoly;
            int n = static_cast<int>(lua_rawlen(L.get(), -1));
            for (int i = 1; i <= n; ++i)
            {
                lua_rawgeti(L.get(), -1, i);
                if (lua_istable(L.get(), -1))
                {
                    lua_getfield(L.get(), -1, "x");
                    lua_getfield(L.get(), -2, "y");
                    double x = lua_isnumber(L.get(), -2) ? lua_tonumber(L.get(), -2) : 0.0;
                    double y = lua_isnumber(L.get(), -1) ? lua_tonumber(L.get(), -1) : 0.0;
                    outpoly.emplace_back(Point2D{x, y});
                    lua_pop(L.get(), 2);
                }
                lua_pop(L.get(), 1);
            }
            if (!outpoly.empty())
            {
                result.push_back(Integerization(outpoly));
            }
        }
        lua_pop(L.get(), 1);
    }
    return result;
}

}  // namespace HsBa::Slicer
