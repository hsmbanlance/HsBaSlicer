#include "sls_export.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>


#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "fileoperator/LuaAdapter.hpp"
#include "paths/imagespath.hpp"

namespace HsBa::Slicer
{

namespace
{

/// @brief Serialize a single polygon to JSON array of {x,y} objects.
std::string PolygonToJson(const PolygonD& poly)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < poly.size(); ++i)
    {
        if (i > 0)
            oss << ",";
        oss << "{\"x\":" << poly[i].x << ",\"y\":" << poly[i].y << "}";
    }
    oss << "]";
    return oss.str();
}

/// @brief Serialize multiple polygons to a JSON object with "polygons" array.
std::string PolygonsToJson(const PolygonsD& polys)
{
    std::ostringstream oss;
    oss << "{\"polygons\":[";
    for (size_t i = 0; i < polys.size(); ++i)
    {
        if (i > 0)
            oss << ",";
        oss << PolygonToJson(polys[i]);
    }
    oss << "]}";
    return oss.str();
}

}  // anonymous namespace

HSBA_SLICER_LIB_API bool SaveSlsPackageLua(const SlsPackage& pkg, const std::string& output_zip,
                                           const std::string& lua_script, const std::string& lua_func)
{
    try
    {
        ImagesPath images_path("config.json", pkg.config_json);

        // Serialize each layer's polygon outlines as JSON and add as "images"
        const int total_layers = static_cast<int>(pkg.layer_outlines.size());
        for (int i = 0; i < total_layers; ++i)
        {
            std::string layer_json = PolygonsToJson(pkg.layer_outlines[i]);
            if (!layer_json.empty())
            {
                float z = (i < static_cast<int>(pkg.layer_z_heights.size())) ? pkg.layer_z_heights[i] : 0.0f;
                // Embed z_height in a wrapper JSON
                std::ostringstream wrapper;
                wrapper << "{\"layer\":" << i << ",\"z_height\":" << z << ",\"outlines\":" << layer_json << "}";
                images_path.AddImage("layers/" + std::to_string(i) + ".json", wrapper.str());
            }
        }

        // Register SQL adapters so Lua script can perform database registration
        std::function<void(lua_State*)> sql_reg = [](lua_State* L)
        {
            RegisterLuaSQLiteAdapter(L);
#ifdef HSBA_USE_MYSQL
            RegisterLuaMySQLAdapter(L);
#endif
#ifdef HSBA_USE_PGSQL
            RegisterLuaPostgreSQLAdapter(L);
#endif
            // Register external File functions for SLS output stage
            for (auto& reg : GetFileFunctions())
                reg(L);
        };

        images_path.Save(std::filesystem::path(output_zip), std::filesystem::path(lua_script),
                         std::string_view(lua_func), sql_reg);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

}  // namespace HsBa::Slicer
