#pragma once
#ifndef HSBA_SLICER_GCODE_PATH_HPP
#define HSBA_SLICER_GCODE_PATH_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "layerspath.hpp"

// forward-declare lua state
struct lua_State;

namespace HsBa::Slicer
{

/// @brief Target firmware type for GCode output.
enum class GCodeFirmware
{
    Marlin,  ///< Marlin firmware (most common FDM)
    RepRap,  ///< RepRap / RRF firmware
    Klipper  ///< Klipper firmware
};

/// @brief Printer configuration for GCode generation.
struct GCodePrinterConfig
{
    float nozzle_diameter = 0.4f;       ///< Nozzle diameter (mm)
    float filament_diameter = 1.75f;    ///< Filament diameter (mm)
    float nozzle_temp = 200.0f;         ///< Nozzle temperature (°C)
    float bed_temp = 60.0f;             ///< Bed temperature (°C)
    float retract_length = 1.0f;        ///< Retraction length (mm)
    float retract_speed = 40.0f;        ///< Retraction speed (mm/s)
    float print_speed = 50.0f;          ///< Print speed (mm/s)
    float travel_speed = 100.0f;        ///< Travel speed (mm/s)
    float first_layer_speed = 20.0f;    ///< First layer speed (mm/s)
    float layer_height = 0.2f;          ///< Layer height (mm)
    float line_width = 0.4f;            ///< Extrusion line width (mm)
    float extrusion_multiplier = 1.0f;  ///< Extrusion multiplier
    bool relative_extrusion = false;    ///< Use relative extrusion (M83) vs absolute (M82)
    bool enable_retraction = true;      ///< Enable retraction on travel moves
};

/// @brief GCode path output supporting multiple firmware formats.
///
/// Inherits LayersPath for layer data storage and Lua extensibility.
/// Adds ToGCode(firmware) for standard 3D printer GCode output.
class GCodePath : public LayersPath
{
public:
    explicit GCodePath(
        const GCodePrinterConfig& config, const std::function<void(std::string_view, std::string_view)>& callback =
                                              [](std::string_view, std::string_view) {});
    virtual ~GCodePath() = default;

    /// @brief Generate standard GCode string for the specified firmware.
    std::string ToGCode(GCodeFirmware firmware) const;

    /// @brief Save standard GCode to file for the specified firmware.
    void SaveGCode(const std::filesystem::path& path, GCodeFirmware firmware) const;

    /// @brief Generate GCode with Lua post-processing script.
    std::string ToGCode(GCodeFirmware firmware, std::string_view script,
                        const std::function<void(lua_State*)>& lua_reg = {}) const;

    /// @brief Get printer configuration.
    const GCodePrinterConfig& printerConfig() const { return printer_config_; }

private:
    GCodePrinterConfig printer_config_;

    std::string GenerateHeader(GCodeFirmware fw) const;
    std::string GenerateFooter(GCodeFirmware fw) const;
    std::string GenerateLayerGCode(int layer_idx, GCodeFirmware fw) const;
    double CalcExtrusion(double segment_length) const;
};

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_GCODE_PATH_HPP
