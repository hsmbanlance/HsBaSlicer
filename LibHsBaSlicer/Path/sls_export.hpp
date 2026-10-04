/** @file sls_export.hpp
 * @brief SLS export package type and Lua-driven export save API.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LIB_SLS_EXPORT_HPP
#define HSBA_SLICER_LIB_SLS_EXPORT_HPP

#include <string>
#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Data package for SLS (Selective Laser Sintering) export.
 *
 * SLS has no standard output format; export is entirely driven by Lua scripts.
 * This package carries layer polygon outlines and configuration that the Lua
 * script uses to produce the final output (zip archive, database registration, etc.).
 */
struct SlsPackage
{
    std::vector<PolygonsD> layer_outlines;  ///< Per-layer slice outlines
    std::vector<float> layer_z_heights;     ///< Z height per layer (mm)
    std::string config_json;                ///< Configuration JSON content
    bool spiral_mode = false;               ///< When true, additionally expose a continuous, Z-rising
                                            ///< spiralized outer-wall path (JSON at "spiral/path.json") to
                                            ///< the Lua export. Per-layer images are still emitted so
                                            ///< existing scripts keep working. Default false (unchanged).
};

/**
 * @brief Save SLS package using a Lua script for custom export logic.
 *
 * The Lua script receives:
 * - `config` global table with `path` ("config.json") and `configStr` (JSON content)
 * - `images` global array where each element is `{path="layers/N.json", data="<polygon JSON>"}`
 * - `output_path` global string with the requested output file path
 * - Registered Lua libraries: Zipper, Cipher, and optionally Bit7zZipper, SQLite, MySQL
 *
 * The Lua script is responsible for creating the output archive and
 * performing any database registration.
 *
 * @param pkg SLS package data.
 * @param output_zip Output file path (passed to Lua as `output_path`).
 * @param lua_script Path to the Lua script file.
 * @param lua_func Lua function name (reserved for future use; script is executed inline).
 * @param error_out Optional out-parameter receiving the failure detail message.
 * @return true if export succeeded, false otherwise.
 */
HSBA_SLICER_LIB_API bool SaveSlsPackageLua(const SlsPackage& pkg, const std::string& output_zip,
                                           const std::string& lua_script, const std::string& lua_func = "export_sls",
                                           std::string* error_out = nullptr);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_SLS_EXPORT_HPP
