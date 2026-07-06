#pragma once
#ifndef HSBA_SLICER_LUA_SUPPORT_HPP
#define HSBA_SLICER_LUA_SUPPORT_HPP

#include <filesystem>
#include <string>
#include <string_view>

#include "ISupport.hpp"

namespace HsBa::Slicer::Support
{
/**
 * @brief Lua script-based custom support generator.
 *
 * Allows users to provide a Lua script that completely replaces the built-in
 * support generation algorithm. The script receives layer data and configuration,
 * and returns support polygons.
 *
 * Lua globals available to the script:
 * - current_layer: table of polygons (1-based array of { {x=..,y=..}, ... })
 * - prev_layer: table of polygons
 * - layer_height: number (mm)
 * - config: table with fields: overhang_angle_threshold, support_diameter, support_gap, etc.
 *
 * The script should either:
 * - Return a table of polygons from the main chunk, or
 * - Define a function (default: "generate_support") that returns polygons
 *
 * Helper functions from 2D/LuaAdapter are registered for polygon manipulation.
 */
class LuaSupport : public ISupport
{
public:
    /**
     * @brief Construct with inline Lua script.
     * @param script Inline Lua script code.
     */
    explicit LuaSupport(std::string_view script);

    /**
     * @brief Construct with inline script and custom function name.
     * @param script Inline Lua script code.
     * @param funcName Name of the Lua function to call for support generation.
     */
    LuaSupport(std::string_view script, std::string_view funcName);

    /**
     * @brief Construct with script file and function name.
     * @param script_file Path to the Lua script file.
     * @param funcName Name of the Lua function to call.
     */
    explicit LuaSupport(const std::filesystem::path& script_file, std::string_view funcName);

    PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                       const SupportConfig& config) override;

private:
    enum class SourceType
    {
        Inline,
        File
    };

    SourceType source_type_;
    std::string script_;
    std::filesystem::path script_file_;
    std::string func_name_;
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_LUA_SUPPORT_HPP
