/** @file lua_pipeline.cpp
 * @brief Implementation of the fully Lua-driven custom pipeline environment and run entry points.
 * @author HsBa
 */
#include "lua_pipeline.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <lua.hpp>

#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"
#include "2D/LuaAdapter.hpp"
#include "2D/PolygonFill.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/path_optimizer.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"
#include "LibHsBaSlicer/Path/waam_export.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "base/error.hpp"
#include "cipher/LuaAdapter.hpp"
#include "fileoperator/LuaAdapter.hpp"
#include "paths/gcodepath.hpp"
#include "support/LuaAdapter.hpp"
#include "support/SupportConfig.hpp"
#include "utils/LuaNewObject.hpp"

namespace HsBa::Slicer
{
namespace
{

// ============================================================================
// Internal run-state shared between the C++ driver and the `HsBa` Lua table
// ============================================================================

struct PipelineRunState
{
    LuaPipelineContext* ctx = nullptr;
    LuaPipelineOutput* out = nullptr;
};

// Absolute registry keys (Lua 5.3+ allows lua_geti/lua_seti on the
// LUA_REGISTRYINDEX pseudo-index) used to bind the `HsBa` table to the current
// run. They must always be used as the *key* argument, never as the table
// index: the table index is LUA_REGISTRYINDEX itself.
enum : int
{
    REG_CTX = 1000001,  // lightuserdata: PipelineRunState*
    REG_LAYERS,         // integer: total layer count reported by the script
    REG_OUTPATH,        // string: output path reported by the script
};

PipelineRunState* GetState(lua_State* L)
{
    lua_geti(L, LUA_REGISTRYINDEX, REG_CTX);
    auto* state = static_cast<PipelineRunState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return state;
}

// ============================================================================
// Small Lua-table reading helpers
// ============================================================================

double GetNumberField(lua_State* L, int index, const char* name, double def)
{
    double v = def;
    if (lua_getfield(L, index, name) == LUA_TNUMBER)
        v = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

int GetIntField(lua_State* L, int index, const char* name, int def)
{
    int v = def;
    if (lua_getfield(L, index, name) == LUA_TNUMBER)
        v = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return v;
}

bool GetBoolField(lua_State* L, int index, const char* name, bool def)
{
    bool v = def;
    int t = lua_getfield(L, index, name);
    if (t == LUA_TBOOLEAN)
        v = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return v;
}

std::string GetStringField(lua_State* L, int index, const char* name, std::string_view def = {})
{
    std::string v(def);
    if (lua_getfield(L, index, name) == LUA_TSTRING)
        v = lua_tostring(L, -1);
    lua_pop(L, 1);
    return v;
}

Eigen::Vector3f GetVector3Field(lua_State* L, int index, const char* name, const Eigen::Vector3f& def)
{
    Eigen::Vector3f v = def;
    if (lua_getfield(L, index, name) == LUA_TTABLE)
    {
        v.x() = static_cast<float>(GetNumberField(L, -1, "x", v.x()));
        v.y() = static_cast<float>(GetNumberField(L, -1, "y", v.y()));
        v.z() = static_cast<float>(GetNumberField(L, -1, "z", v.z()));
    }
    lua_pop(L, 1);
    return v;
}

FillMode ParseFillMode(std::string_view mode)
{
    if (mode == "line" || mode == "Line")
        return FillMode::Line;
    if (mode == "simpleZigzag" || mode == "SimpleZigzag")
        return FillMode::SimpleZigzag;
    return FillMode::Zigzag;
}

GCodeFirmware ParseFirmware(std::string_view fw)
{
    if (fw == "reprap" || fw == "RRF")
        return GCodeFirmware::RepRap;
    if (fw == "klipper" || fw == "Klipper")
        return GCodeFirmware::Klipper;
    return GCodeFirmware::Marlin;
}

// Read a support-config table into the given config struct base fields.
void ReadSupportConfig(lua_State* L, int index, Support::SupportConfig& config)
{
    config.overhang_angle_threshold = static_cast<float>(GetNumberField(L, index, "overhang_angle", 45.0));
    config.layer_height = static_cast<float>(GetNumberField(L, index, "layer_height", 0.2));
    config.support_gap = static_cast<float>(GetNumberField(L, index, "support_gap", 0.5));
    config.support_diameter = static_cast<float>(GetNumberField(L, index, "support_diameter", 2.0));
    config.support_density = static_cast<float>(GetNumberField(L, index, "support_density", 0.3));
    config.support_pattern = GetIntField(L, index, "support_pattern", config.support_pattern);
    config.tree_branch_angle = static_cast<float>(GetNumberField(L, index, "tree_branch_angle", 30.0));
    config.tree_max_branch_radius = static_cast<float>(GetNumberField(L, index, "tree_max_branch_radius", 5.0));
    config.honeycomb_cell_size = static_cast<float>(GetNumberField(L, index, "honeycomb_cell_size", 5.0));
}

void ReadSlaFloorConfig(lua_State* L, int index, SlaFloorConfig& config)
{
    config.raft_offset = GetNumberField(L, index, "raft_offset", config.raft_offset);
    config.border_width = GetNumberField(L, index, "border_width", config.border_width);
    config.fill_spacing = GetNumberField(L, index, "fill_spacing", config.fill_spacing);
    config.fill_angle_deg = GetNumberField(L, index, "fill_angle_deg", config.fill_angle_deg);
    config.border_count = GetIntField(L, index, "border_count", config.border_count);
    config.use_convex_hull = GetBoolField(L, index, "use_convex_hull", config.use_convex_hull);
    config.concave_hull_points = GetIntField(L, index, "concave_hull_points", config.concave_hull_points);
}

// Read a polygons array (table of {x,y} point arrays) from stack index.
PolygonsD ReadPolygonsD(lua_State* L, int index)
{
    return LuaTableToPolygonsD(L, index);
}

std::vector<PolygonsD> ReadLayerList(lua_State* L, int index)
{
    std::vector<PolygonsD> layers;
    if (!lua_istable(L, index))
        return layers;
    const size_t len = lua_rawlen(L, index);
    layers.reserve(len);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, index, static_cast<int>(i));
        layers.push_back(ReadPolygonsD(L, -1));
        lua_pop(L, 1);
    }
    return layers;
}

void PushPolygonsD(lua_State* L, const PolygonsD& polys)
{
    PushPolygonsDToLua(L, polys);
}

void PushLayerList(lua_State* L, const std::vector<PolygonsD>& layers)
{
    lua_newtable(L);
    for (size_t i = 0; i != layers.size(); ++i)
    {
        PushPolygonsD(L, layers[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
}

// ============================================================================
// `HsBa` table operations
// ============================================================================

// HsBa.progress(percent[, stage])
int Lp_progress(lua_State* L)
{
    auto* state = GetState(L);
    const int percent = static_cast<int>(luaL_checkinteger(L, 1));
    std::string_view stage = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
    if (state && state->ctx && state->ctx->progress_cb)
        state->ctx->progress_cb(percent, stage);
    return 0;
}

// HsBa.setLayers(n) / HsBa.setOutputPath(path)
int Lp_setLayers(lua_State* L)
{
    luaL_checkinteger(L, 1);
    lua_pushinteger(L, static_cast<int>(lua_tointeger(L, 1)));
    lua_seti(L, LUA_REGISTRYINDEX, REG_LAYERS);
    return 0;
}

int Lp_setOutputPath(lua_State* L)
{
    luaL_checkstring(L, 1);
    lua_pushvalue(L, 1);
    lua_seti(L, LUA_REGISTRYINDEX, REG_OUTPATH);
    return 0;
}

// HsBa.readFile(path) -> string|nil
int Lp_readFile(lua_State* L)
{
    std::string path = luaL_checkstring(L, 1);
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        lua_pushnil(L);
        return 1;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    const std::string content = oss.str();
    lua_pushlstring(L, content.data(), content.size());
    return 1;
}

// HsBa.writeFile(path, content) -> true
int Lp_writeFile(lua_State* L)
{
    std::string path = luaL_checkstring(L, 1);
    std::string content = luaL_checkstring(L, 2);
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs)
        return luaL_error(L, "Failed to open file for writing: %s", path.c_str());
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    lua_pushboolean(L, 1);
    return 1;
}

// --- Model management -------------------------------------------------------

// HsBa.loadModel(name, path) -> true
int Lp_loadModel(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    std::string_view path = luaL_checkstring(L, 2);
    auto model = GetModel(name);
    if (!model)
        model = LoadModel(name, path);
    if (!model)
        return luaL_error(L, "Failed to load model '%s' from '%s'", name.c_str(), std::string(path).c_str());
    lua_pushboolean(L, 1);
    return 1;
}

// HsBa.modelInfo(name) -> {bbox_min={x,y,z}, bbox_max={x,y,z}, volume, layerHeight, firstLayerHeight}
int Lp_modelInfo(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    if (!ContainsModel(name))
        return luaL_error(L, "Model '%s' not found in pool", name.c_str());
    const ModelInfo info = GetModelInfo(name);

    lua_newtable(L);  // result
    lua_newtable(L);  // bbox_min
    lua_pushnumber(L, info.bbox_min.x());
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, info.bbox_min.y());
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, info.bbox_min.z());
    lua_setfield(L, -2, "z");
    lua_setfield(L, -2, "bbox_min");
    lua_newtable(L);  // bbox_max
    lua_pushnumber(L, info.bbox_max.x());
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, info.bbox_max.y());
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, info.bbox_max.z());
    lua_setfield(L, -2, "z");
    lua_setfield(L, -2, "bbox_max");
    lua_pushnumber(L, info.volume);
    lua_setfield(L, -2, "volume");
    return 1;
}

// HsBa.translateModel(name, {x,y,z}) / HsBa.rotateModel(name, {x,y,z,w}) / HsBa.scaleModel(name, s | {x,y,z})
int Lp_translateModel(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const Eigen::Vector3f t = GetVector3Field(L, 2, "", Eigen::Vector3f::Zero());
    TranslateModel(name, t);
    return 0;
}

int Lp_rotateModel(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    Eigen::Quaternionf q(1.f, 0.f, 0.f, 0.f);
    q.x() = static_cast<float>(GetNumberField(L, 2, "x", 0.0));
    q.y() = static_cast<float>(GetNumberField(L, 2, "y", 0.0));
    q.z() = static_cast<float>(GetNumberField(L, 2, "z", 0.0));
    q.w() = static_cast<float>(GetNumberField(L, 2, "w", 1.0));
    RotateModel(name, q);
    return 0;
}

int Lp_scaleModel(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    if (lua_istable(L, 2))
    {
        const Eigen::Vector3f s = GetVector3Field(L, 2, "", Eigen::Vector3f::Ones());
        ScaleModel(name, s);
    }
    else
    {
        ScaleModel(name, static_cast<float>(luaL_checknumber(L, 2)));
    }
    return 0;
}

// HsBa.removeModel(name) / HsBa.modelNames() -> {name,...}
int Lp_removeModel(lua_State* L)
{
    RemoveModel(luaL_checkstring(L, 1));
    return 0;
}

int Lp_modelNames(lua_State* L)
{
    const auto names = GetModelNames();
    lua_newtable(L);
    for (size_t i = 0; i != names.size(); ++i)
    {
        lua_pushstring(L, names[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

// --- Slicing ----------------------------------------------------------------

// HsBa.layerCount(name, layerHeight, firstLayerHeight) -> int
int Lp_layerCount(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    const double layer_height = luaL_checknumber(L, 2);
    const double first_layer_height = luaL_checknumber(L, 3);
    if (!ContainsModel(name) || layer_height <= 0.0)
        return luaL_error(L, "layerCount: invalid model or layer height");
    const ModelInfo info = GetModelInfo(name);
    const double model_height = info.bbox_max.z() - info.bbox_min.z();
    if (model_height <= 0.0)
    {
        lua_pushinteger(L, 0);
        return 1;
    }
    const double remaining = model_height - first_layer_height;
    const int layers = (remaining <= 0.0) ? 1 : 1 + static_cast<int>(std::ceil(remaining / layer_height));
    lua_pushinteger(L, layers);
    return 1;
}

// HsBa.layerZ(index0based, layerHeight, firstLayerHeight) -> number
int Lp_layerZ(lua_State* L)
{
    const int index = static_cast<int>(luaL_checkinteger(L, 1));
    const double layer_height = luaL_checknumber(L, 2);
    const double first_layer_height = luaL_checknumber(L, 3);
    const double z = (index == 0) ? first_layer_height : first_layer_height + index * layer_height;
    lua_pushnumber(L, z);
    return 1;
}

// HsBa.slice(name, z) -> polygons (safe, closed contours only)
int Lp_slice(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    const double z = luaL_checknumber(L, 2);
    auto model = GetModel(name);
    if (!model)
        return luaL_error(L, "Model '%s' not loaded", name.c_str());
    PushPolygonsD(L, UnIntegerization(Slice(*model, static_cast<float>(z))));
    return 1;
}

// HsBa.sliceUnsafe(name, z) -> polygons (normalized, may drop open contours)
int Lp_sliceUnsafe(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    const double z = luaL_checknumber(L, 2);
    auto model = GetModel(name);
    if (!model)
        return luaL_error(L, "Model '%s' not loaded", name.c_str());
    PushPolygonsD(L, NormalizeUnSafePolygons(UnSafeSlice(*model, static_cast<float>(z))));
    return 1;
}

// --- Coordinate conversion ----------------------------------------------------

// HsBa.toInt(polygons) / HsBa.toDouble(intPolygons)
int Lp_toInt(lua_State* L)
{
    const PolygonsD polys = ReadPolygonsD(L, 1);
    PushPolygonsToLua(L, Integerization(polys));
    return 1;
}

int Lp_toDouble(lua_State* L)
{
    const Polygons polys = LuaTableToPolygons(L, 1);
    PushPolygonsD(L, UnIntegerization(polys));
    return 1;
}

// --- Fill ---------------------------------------------------------------------

// HsBa.fill(polygons[, {spacing, mode, angle, borderCount}]) -> polygons (int or double tables)
int Lp_fill(lua_State* L)
{
    const Polygons polys = LuaTableToPolygons(L, 1);
    double spacing = 0.4;
    FillMode mode = FillMode::Zigzag;
    double angle = 45.0;
    int border_count = 0;
    if (lua_istable(L, 2))
    {
        spacing = GetNumberField(L, 2, "spacing", spacing);
        mode = ParseFillMode(GetStringField(L, 2, "mode"));
        angle = GetNumberField(L, 2, "angle", angle);
        border_count = GetIntField(L, 2, "borderCount", 0);
    }
    Polygons result;
    if (border_count > 0)
        result = FillWithBorder(polys, spacing, border_count, mode, angle);
    else
        result = FillPolygon(polys, spacing, mode, angle);
    PushPolygonsToLua(L, result);
    return 1;
}

// --- Support ------------------------------------------------------------------

// HsBa.fdmSupport(layers, cfg) -> layers
int Lp_fdmSupport(lua_State* L)
{
    const auto layers = ReadLayerList(L, 1);
    Support::FdmSupportConfig config;
    if (lua_istable(L, 2))
    {
        ReadSupportConfig(L, 2, config);
        config.interface_layers = GetIntField(L, 2, "interface_layers", config.interface_layers);
        config.interface_density =
            static_cast<float>(GetNumberField(L, 2, "interface_density", config.interface_density));
    }
    PushLayerList(L, GenerateAllFdmSupport(layers, config));
    return 1;
}

// HsBa.slaSupport(layers, cfg) -> layers
int Lp_slaSupport(lua_State* L)
{
    const auto layers = ReadLayerList(L, 1);
    Support::SlaSupportConfig config;
    if (lua_istable(L, 2))
    {
        ReadSupportConfig(L, 2, config);
        config.tip_diameter = static_cast<float>(GetNumberField(L, 2, "tip_diameter", config.tip_diameter));
        config.raft_thickness = static_cast<float>(GetNumberField(L, 2, "raft_thickness", config.raft_thickness));
    }
    PushLayerList(L, GenerateAllSlaSupport(layers, config));
    return 1;
}

// --- SLA floor ----------------------------------------------------------------

// HsBa.floor(bottomPolygons, cfg) -> polygons (int table)
int Lp_floor(lua_State* L)
{
    const Polygons polys = LuaTableToPolygons(L, 1);
    SlaFloorConfig config;
    if (lua_istable(L, 2))
        ReadSlaFloorConfig(L, 2, config);
    PushPolygonsToLua(L, GenerateFloorRaft(polys, config));
    return 1;
}

// --- Path / G-code output -------------------------------------------------------

// Push a continuous 3D spiral path as an array of {x=, y=, z=} point tables.
void PushSpiralPath(lua_State* L, const std::vector<SpiralPoint>& path)
{
    lua_createtable(L, static_cast<int>(path.size()), 0);
    for (size_t i = 0; i < path.size(); ++i)
    {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, path[i].x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, path[i].y);
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, path[i].z);
        lua_setfield(L, -2, "z");
        lua_rawseti(L, -2, static_cast<int>(i) + 1);
    }
}

// HsBa.spiralize(sections[, {layerHeight, startZ, zHeights}]) -> path
// sections: per-layer closed contours (array of layers, each = array of polygons
//           made of {x, y} point tables), e.g. the outlines produced by HsBa.slice.
// Returns a single continuous 3D polyline (array of {x, y, z}) whose Z rises one
// layer per revolution: the classic spiralized / helical outer wall with no
// retractions or per-layer travel. Intended for extrusion-style deposition
// processes (FDM, WAAM, 3DP). Provide per-layer heights via {layerHeight, startZ}
// or an explicit zHeights array.
int Lp_spiralize(lua_State* L)
{
    const auto sections = ReadLayerList(L, 1);
    std::vector<double> zs(sections.size(), 0.0);
    bool explicit_z = false;
    double layer_height = 0.4;
    double start_z = 0.0;
    if (lua_istable(L, 2))
    {
        layer_height = GetNumberField(L, 2, "layerHeight", layer_height);
        start_z = GetNumberField(L, 2, "startZ", start_z);
        lua_getfield(L, 2, "zHeights");
        if (lua_istable(L, -1))
        {
            explicit_z = true;
            size_t n = lua_rawlen(L, -1);
            if (n > sections.size())
            {
                n = sections.size();
            }
            for (size_t i = 1; i <= n; ++i)
            {
                lua_rawgeti(L, -1, static_cast<int>(i));
                zs[i - 1] = lua_tonumber(L, -1);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);  // pop zHeights field
    }
    if (!explicit_z)
    {
        for (size_t i = 0; i < sections.size(); ++i)
        {
            zs[i] = start_z + static_cast<double>(i) * layer_height;
        }
    }
    PushSpiralPath(L, SpiralizeOuterWall(sections, zs));
    return 1;
}

// HsBa.toGcode(layers, cfg) -> gcode string
// layers: array of {outlines=..., fills=..., supports=..., zHeight=...} (double polygons)
// cfg: {layerHeight, lineWidth, printSpeed, travelSpeed, extrusionMultiplier,
//       firmware="marlin"|"reprap"|"klipper", nozzleDiameter, filamentDiameter, ...}
int Lp_toGcode(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<LayerPathData> layer_data;
    const size_t len = lua_rawlen(L, 1);
    layer_data.reserve(len);
    for (size_t i = 1; i <= len; ++i)
    {
        lua_rawgeti(L, 1, static_cast<int>(i));
        LayerPathData data;
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "outlines");
            data.outlines = ReadPolygonsD(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "fills");
            data.fills = ReadPolygonsD(L, -1);
            lua_pop(L, 1);
            lua_getfield(L, -1, "supports");
            data.supports = ReadPolygonsD(L, -1);
            lua_pop(L, 1);
            data.z_height = static_cast<float>(GetNumberField(L, -1, "zHeight", 0.0));
        }
        lua_pop(L, 1);
        layer_data.push_back(std::move(data));
    }

    FdmPathConfig path_config;
    GCodePrinterConfig printer_config;
    GCodeFirmware firmware = GCodeFirmware::Marlin;
    if (lua_istable(L, 2))
    {
        path_config.layer_height = static_cast<float>(GetNumberField(L, 2, "layerHeight", path_config.layer_height));
        path_config.line_width = static_cast<float>(GetNumberField(L, 2, "lineWidth", path_config.line_width));
        path_config.print_speed = static_cast<float>(GetNumberField(L, 2, "printSpeed", path_config.print_speed));
        path_config.travel_speed = static_cast<float>(GetNumberField(L, 2, "travelSpeed", path_config.travel_speed));
        path_config.extrusion_multiplier =
            static_cast<float>(GetNumberField(L, 2, "extrusionMultiplier", path_config.extrusion_multiplier));
        firmware = ParseFirmware(GetStringField(L, 2, "firmware"));

        printer_config.nozzle_diameter =
            static_cast<float>(GetNumberField(L, 2, "nozzleDiameter", printer_config.nozzle_diameter));
        printer_config.filament_diameter =
            static_cast<float>(GetNumberField(L, 2, "filamentDiameter", printer_config.filament_diameter));
        printer_config.nozzle_temp = static_cast<float>(GetNumberField(L, 2, "nozzleTemp", printer_config.nozzle_temp));
        printer_config.bed_temp = static_cast<float>(GetNumberField(L, 2, "bedTemp", printer_config.bed_temp));
        printer_config.retract_length =
            static_cast<float>(GetNumberField(L, 2, "retractLength", printer_config.retract_length));
        printer_config.retract_speed =
            static_cast<float>(GetNumberField(L, 2, "retractSpeed", printer_config.retract_speed));
        printer_config.print_speed = path_config.print_speed;
        printer_config.travel_speed = path_config.travel_speed;
        printer_config.layer_height = path_config.layer_height;
        printer_config.line_width = path_config.line_width;
        printer_config.extrusion_multiplier = path_config.extrusion_multiplier;
    }

    auto path = GenerateGCodePathV2(layer_data, path_config, printer_config);
    if (!path)
        return luaL_error(L, "toGcode: failed to generate path");
    const std::string gcode = path->ToGCode(firmware);
    lua_pushlstring(L, gcode.data(), gcode.size());
    return 1;
}

// --- Packaging / export -----------------------------------------------------------

// HsBa.saveSlsPackage({outlines=layers, zHeights={...}, config="json", output="x.zip",
//                     script="export.lua"[, func="export_sls"]}) -> bool
int Lp_saveSlsPackage(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    SlsPackage pkg;
    lua_getfield(L, 1, "outlines");
    pkg.layer_outlines = ReadLayerList(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "zHeights");
    if (lua_istable(L, -1))
    {
        const size_t len = lua_rawlen(L, -1);
        pkg.layer_z_heights.reserve(len);
        for (size_t i = 1; i <= len; ++i)
        {
            lua_rawgeti(L, -1, static_cast<int>(i));
            pkg.layer_z_heights.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    pkg.config_json = GetStringField(L, 1, "config");
    const std::string output = GetStringField(L, 1, "output");
    const std::string script = GetStringField(L, 1, "script");
    const std::string func = GetStringField(L, 1, "func", "export_sls");
    if (output.empty() || script.empty())
        return luaL_error(L, "saveSlsPackage: 'output' and 'script' fields are required");
    const bool ok = SaveSlsPackageLua(pkg, output, script, func);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// HsBa.saveWaamPackage({outlines=layers, zHeights={...}, weld={current,voltage,...},
//                       robotType=0, beadWidth=1.2, config="json", output="x.txt"
//                       [, script="path.lua"][, func="export_waam"]}) -> bool|string
// WAAM output is a robot language program; 'script' is an optional custom code
// generator. On failure the error detail is returned as the second value.
int Lp_saveWaamPackage(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    WaamRobotPackage pkg;
    lua_getfield(L, 1, "outlines");
    pkg.layer_outlines = ReadLayerList(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "zHeights");
    if (lua_istable(L, -1))
    {
        const size_t len = lua_rawlen(L, -1);
        pkg.layer_z_heights.reserve(len);
        for (size_t i = 1; i <= len; ++i)
        {
            lua_rawgeti(L, -1, static_cast<int>(i));
            pkg.layer_z_heights.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // Welding sub-table (all fields optional, defaults come from WaamWeldParams).
    if (lua_getfield(L, 1, "weld") == LUA_TTABLE)
    {
        pkg.weld.current = static_cast<float>(GetNumberField(L, -1, "current", pkg.weld.current));
        pkg.weld.voltage = static_cast<float>(GetNumberField(L, -1, "voltage", pkg.weld.voltage));
        pkg.weld.wire_feed_speed = static_cast<float>(GetNumberField(L, -1, "wireFeedSpeed", pkg.weld.wire_feed_speed));
        pkg.weld.gas_flow_rate = static_cast<float>(GetNumberField(L, -1, "gasFlowRate", pkg.weld.gas_flow_rate));
        pkg.weld.travel_speed = static_cast<float>(GetNumberField(L, -1, "travelSpeed", pkg.weld.travel_speed));
        pkg.weld.process = GetIntField(L, -1, "process", pkg.weld.process);
    }
    lua_pop(L, 1);

    pkg.robot_type = GetIntField(L, 1, "robotType", pkg.robot_type);
    pkg.bead_width = static_cast<float>(GetNumberField(L, 1, "beadWidth", pkg.bead_width));
    pkg.config_json = GetStringField(L, 1, "config");
    const std::string output = GetStringField(L, 1, "output");
    const std::string script = GetStringField(L, 1, "script");
    const std::string func = GetStringField(L, 1, "func", "export_waam");
    if (output.empty())
        return luaL_error(L, "saveWaamPackage: 'output' field is required");

    std::string error;
    const bool ok = SaveWaamRobotPath(pkg, output, script, func, &error);
    lua_pushboolean(L, ok ? 1 : 0);
    if (!ok)
        lua_pushstring(L, error.c_str());
    return ok ? 1 : 2;
}

// HsBa.saveSlaPackage({outlines=layers, supports=layers, floor=polygons, config="json",
//                     output="x.zip", imageWidth, imageHeight, imageExtension=".png"}) -> bool
int Lp_saveSlaPackage(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    SlaPackage pkg;
    lua_getfield(L, 1, "outlines");
    pkg.layer_outlines = ReadLayerList(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "supports");
    pkg.layer_supports = ReadLayerList(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "floor");
    pkg.floor_polygons = ReadPolygonsD(L, -1);
    lua_pop(L, 1);
    pkg.config_json = GetStringField(L, 1, "config");
    pkg.image_width = GetIntField(L, 1, "imageWidth", 0);
    pkg.image_height = GetIntField(L, 1, "imageHeight", 0);
    pkg.image_extension = GetStringField(L, 1, "imageExtension", ".png");
    const std::string output = GetStringField(L, 1, "output");
    if (output.empty())
        return luaL_error(L, "saveSlaPackage: 'output' field is required");
    const bool ok = SaveSlaPackage(pkg, output);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// HsBa.renderImage(polygons, width, height, path) -> bool
int Lp_renderImage(lua_State* L)
{
    const PolygonsD polys = ReadPolygonsD(L, 1);
    const int width = static_cast<int>(luaL_checkinteger(L, 2));
    const int height = static_cast<int>(luaL_checkinteger(L, 3));
    std::string path = luaL_checkstring(L, 4);
    const bool ok = RenderPolygonsToImage(polys, width, height, path);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

const luaL_Reg hsba_ops[] = {
    {"progress", Lp_progress},
    {"setLayers", Lp_setLayers},
    {"setOutputPath", Lp_setOutputPath},
    {"readFile", Lp_readFile},
    {"writeFile", Lp_writeFile},
    {"loadModel", Lp_loadModel},
    {"modelInfo", Lp_modelInfo},
    {"translateModel", Lp_translateModel},
    {"rotateModel", Lp_rotateModel},
    {"scaleModel", Lp_scaleModel},
    {"removeModel", Lp_removeModel},
    {"modelNames", Lp_modelNames},
    {"layerCount", Lp_layerCount},
    {"layerZ", Lp_layerZ},
    {"slice", Lp_slice},
    {"sliceUnsafe", Lp_sliceUnsafe},
    {"toInt", Lp_toInt},
    {"toDouble", Lp_toDouble},
    {"fill", Lp_fill},
    {"fdmSupport", Lp_fdmSupport},
    {"slaSupport", Lp_slaSupport},
    {"floor", Lp_floor},
    {"toGcode", Lp_toGcode},
    {"spiralize", Lp_spiralize},
    {"saveSlsPackage", Lp_saveSlsPackage},
    {"saveWaamPackage", Lp_saveWaamPackage},
    {"saveSlaPackage", Lp_saveSlaPackage},
    {"renderImage", Lp_renderImage},
    {nullptr, nullptr},
};

// ============================================================================
// Environment construction
// ============================================================================

void InstallHsBaTable(lua_State* L)
{
    // Each operation resolves the run state through the registry slot bound
    // below, so no per-function upvalue is required.
    luaL_newlib(L, hsba_ops);
    lua_setglobal(L, "HsBa");
}

void InjectContextGlobals(lua_State* L, const LuaPipelineContext& ctx)
{
    lua_pushstring(L, ctx.config_json.c_str());
    lua_setglobal(L, "pipeline_config");
    lua_pushstring(L, ctx.output_path.c_str());
    lua_setglobal(L, "output_path");
    lua_pushstring(L, ctx.model_name.c_str());
    lua_setglobal(L, "model_name");
    lua_pushstring(L, ctx.model_path.c_str());
    lua_setglobal(L, "model_path");
    lua_pushstring(L, ctx.entry_func.c_str());
    lua_setglobal(L, "pipeline_entry");
}

bool RunLuaSource(lua_State* L, std::string_view source, std::string_view chunk_name)
{
    if (luaL_dostring(L, std::string(source).c_str()) != LUA_OK)
    {
        const char* err = lua_tostring(L, -1);
        std::string message = std::string("Lua error in ") + std::string(chunk_name) + ": " + (err ? err : "unknown");
        lua_pop(L, 1);
        throw RuntimeError(message);
    }
    return true;
}

}  // namespace

HSBA_SLICER_LIB_API void SetupLuaPipelineEnvironment(lua_State* L, LuaPipelineContext& ctx, LuaPipelineOutput* out,
                                                     void* run_state)
{
    luaL_openlibs(L);

    // Make sure the common AnyObject registrar is present in the pools before
    // consuming them (same contract as the built-in pipelines).
    InstallCommonAnyObjectTypes();
    for (auto& reg : Get2DFunctions())
        reg(L);
    for (auto& reg : Get3DFunctions())
        reg(L);
    for (auto& reg : GetFileFunctions())
        reg(L);

    // Stage-independent libraries used by the pipeline building blocks.
    RegisterLuaPolygonOperations(L);
    Support::RegisterLuaSupport(L);
    RegisterLuaPolygonFillFunctions(L);
    RegisterLuaPathOptimizeFunctions(L);
    RegisterLuaZipper(L);
    Cipher::RegisterLuaCipher(L);
    RegisterLuaSQLiteAdapter(L);
    RegisterLuaParamStore(L);
#ifdef HSBA_USE_BIT7Z
    RegisterLuaBit7zZipper(L);
#endif
#ifdef HSBA_USE_MYSQL
    RegisterLuaMySQLAdapter(L);
#endif
#ifdef HSBA_USE_PGSQL
    RegisterLuaPostgreSQLAdapter(L);
#endif

    // Bind the run state to the registry (the state must outlive the Lua
    // state; the caller owns its storage).
    static PipelineRunState fallback_state;
    PipelineRunState* state = run_state ? static_cast<PipelineRunState*>(run_state) : &fallback_state;
    state->ctx = &ctx;
    state->out = out;
    lua_pushlightuserdata(L, state);
    lua_seti(L, LUA_REGISTRYINDEX, REG_CTX);
    lua_pushinteger(L, 0);
    lua_seti(L, LUA_REGISTRYINDEX, REG_LAYERS);
    lua_pushnil(L);
    lua_seti(L, LUA_REGISTRYINDEX, REG_OUTPATH);

    InstallHsBaTable(L);
    InjectContextGlobals(L, ctx);
}

HSBA_SLICER_LIB_API LuaPipelineOutput RunLuaPipeline(const LuaPipelineContext& ctx)
{
    LuaPipelineOutput out;
    PipelineRunState run_state;

    auto L = MakeUniqueLuaState();
    if (!L)
    {
        out.error_message = "Failed to create Lua state";
        return out;
    }

    try
    {
        SetupLuaPipelineEnvironment(L.get(), const_cast<LuaPipelineContext&>(ctx), &out, &run_state);

        // Execute the inline source first as a parameter prelude for the script file, then load the script file.
        if (!ctx.script.empty())
            RunLuaSource(L.get(), ctx.script, "=(custom pipeline prelude)");

        if (!ctx.script_file.empty())
        {
            std::ifstream ifs(ctx.script_file, std::ios::binary);
            if (!ifs)
            {
                out.error_message = "Failed to open Lua script file: " + ctx.script_file;
                return out;
            }
            std::ostringstream oss;
            oss << ifs.rdbuf();
            RunLuaSource(L.get(), oss.str(), ctx.script_file);
        }

        // Call the entry function: any truthy return marks success, a string
        // return is additionally reported as the result payload; error/false
        // returns (or an uncaught Lua error) mean failure.
        lua_getglobal(L.get(), ctx.entry_func.c_str());
        if (lua_isnil(L.get(), -1))
        {
            lua_pop(L.get(), 1);
            out.error_message = "Lua entry function '" + ctx.entry_func + "' not found in script";
            return out;
        }
        if (lua_pcall(L.get(), 0, 1, 0) != LUA_OK)
        {
            const char* err = lua_tostring(L.get(), -1);
            out.error_message = std::string("Lua error: ") + (err ? err : "unknown");
            lua_pop(L.get(), 1);
            return out;
        }
        const bool truthy = !(lua_isnil(L.get(), -1) || (lua_isboolean(L.get(), -1) && !lua_toboolean(L.get(), -1)));
        if (lua_isstring(L.get(), -1))
            out.result_string = lua_tostring(L.get(), -1);
        lua_pop(L.get(), 1);

        lua_geti(L.get(), LUA_REGISTRYINDEX, REG_LAYERS);
        out.total_layers = static_cast<int>(lua_tointeger(L.get(), -1));
        lua_pop(L.get(), 1);
        lua_geti(L.get(), LUA_REGISTRYINDEX, REG_OUTPATH);
        if (lua_isstring(L.get(), -1))
            out.output_path = lua_tostring(L.get(), -1);
        lua_pop(L.get(), 1);

        out.success = truthy;
        if (!truthy && out.result_string.empty())
            out.error_message = "Lua pipeline reported failure (entry function returned false/nil)";
    }
    catch (const RuntimeError& e)
    {
        out.success = false;
        out.error_message = std::string("Pipeline error: ") + e.what();
    }

    return out;
}

}  // namespace HsBa::Slicer
