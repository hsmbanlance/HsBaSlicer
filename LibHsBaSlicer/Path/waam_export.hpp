#pragma once
#ifndef HSBA_SLICER_LIB_WAAM_EXPORT_HPP
#define HSBA_SLICER_LIB_WAAM_EXPORT_HPP

#include <string>
#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Welding process parameters handed to the robot path generator.
 *
 * Mirrors the subset of WeldParam that the WAAM pipeline exposes through the
 * C ABI. Values are carried straight into each weld robot point.
 */
struct WaamWeldParams
{
    float current = 180.0f;        ///< Welding current [A]
    float voltage = 22.0f;         ///< Arc voltage [V]
    float wire_feed_speed = 5.0f;  ///< Wire feed speed [m/min]
    float gas_flow_rate = 15.0f;   ///< Shielding gas flow rate [L/min]
    float travel_speed = 8.0f;     ///< Torch travel speed [mm/s]
    int process = 0;               ///< 0 = arc (MIG/MAG), 1 = laser
};

/**
 * @brief Data package for WAAM (Wire Arc Additive Manufacturing) export.
 *
 * Unlike the bed-based processes, WAAM output is a robot language program
 * (ABB / KUKA / FANUC). This package carries the per-layer deposition contours
 * and the welding/robot settings used to build the robot path.
 */
struct WaamRobotPackage
{
    std::vector<PolygonsD> layer_outlines;  ///< Per-layer deposition contours
    std::vector<float> layer_z_heights;     ///< Z height per layer (mm)
    WaamWeldParams weld;                    ///< Welding parameters
    int robot_type = 0;                     ///< 0=ABB, 1=KUKA, 2=FANUC, 3=Unknown
    float bead_width = 1.2f;                ///< Deposited bead width (mm), used for intra-layer spacing
    std::string config_json;                ///< Optional configuration sidecar content
    bool spiral_mode = false;               ///< When true, the outer walls of all layers are merged into
                                            ///< ONE continuous, Z-rising weld bead (no per-layer travel/arc
                                            ///< restarts). Requires >=1 closed contour per layer; otherwise
                                            ///< falls back to the standard per-layer deposition.
};

/**
 * @brief Generate a WAAM robot program from a deposition package.
 *
 * Builds a WeldRobotPath from the layer contours and writes brand-specific
 * robot code to `output_path`. When `lua_script` is non-empty, the robot code
 * is post-processed by that Lua script (executed inline, same contract as the
 * SLS export: return a table/nil, not a string, to avoid clobbering output).
 *
 * @param pkg WAAM robot package data.
 * @param output_path Output robot program path.
 * @param lua_script Optional Lua post-processing script path (empty = built-in).
 * @param lua_func Lua function name (reserved; script executed inline).
 * @param error_out Optional out-parameter receiving the failure detail message.
 * @return true if export succeeded, false otherwise.
 */
HSBA_SLICER_LIB_API bool SaveWaamRobotPath(const WaamRobotPackage& pkg, const std::string& output_path,
                                           const std::string& lua_script = "",
                                           const std::string& lua_func = "export_waam",
                                           std::string* error_out = nullptr);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_WAAM_EXPORT_HPP
