#pragma once
#ifndef HSBA_SLICER_LIB_PATH_GENERATOR_HPP
#define HSBA_SLICER_LIB_PATH_GENERATOR_HPP

#include <memory>
#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"
#include "paths/gcodepath.hpp"
#include "paths/pointspath.hpp"

namespace HsBa::Slicer
{
/**
 * @brief FDM path generation configuration.
 */
struct FdmPathConfig
{
    float layer_height = 0.2f;          ///< Layer height (mm)
    float line_width = 0.4f;            ///< Line width (mm)
    float print_speed = 50.0f;          ///< Print speed (mm/s)
    float travel_speed = 100.0f;        ///< Travel speed (mm/s)
    float extrusion_multiplier = 1.0f;  ///< Extrusion multiplier
    GCodeUnits units = GCodeUnits::mm;  ///< Units
};

/**
 * @brief Single layer path data.
 */
struct LayerPathData
{
    PolygonsD outlines;     ///< Outline paths
    PolygonsD fills;        ///< Fill paths
    PolygonsD supports;     ///< Support paths
    float z_height = 0.0f;  ///< Layer Z height
};

/**
 * @brief Generate G-code paths.
 * @param layer_data Per-layer path data (outlines, fills, supports).
 * @param config FDM path configuration.
 * @return G-code path object.
 */
HSBA_SLICER_LIB_API std::unique_ptr<PointsPath> GenerateGCodePath(const std::vector<LayerPathData>& layer_data,
                                                                  const FdmPathConfig& config);

/**
 * @brief Generate G-code paths with firmware-specific output (V2).
 * @param layer_data Per-layer path data (outlines, fills, supports).
 * @param config FDM path configuration.
 * @param printer_config Printer configuration for GCode generation.
 * @return GCodePath object supporting multi-firmware output.
 */
HSBA_SLICER_LIB_API std::unique_ptr<GCodePath> GenerateGCodePathV2(const std::vector<LayerPathData>& layer_data,
                                                                   const FdmPathConfig& config,
                                                                   const GCodePrinterConfig& printer_config);

/**
 * @brief Generate a spiralized (vase-mode) G-code path: the outer walls of all
 *        layers are merged into a single continuous, Z-rising extrusion line.
 *
 * Uses SpiralizeOuterWall over the per-layer outer contours and emits it as a
 * continuous 3D wall (no infill/support, no per-layer travel). Best suited for
 * uniform-cross-section (prismatic) models. Returns a GCodePath carrying only a
 * continuous wall (no layers); ToGCode emits header + helix + footer.
 *
 * @param layer_outlines Per-layer contours (outer loop chosen per layer).
 * @param layer_zs Build height (Z) of each layer; same size as layer_outlines.
 * @param printer_config Printer configuration for G-code generation.
 * @return G-code path object with a continuous helical wall.
 */
HSBA_SLICER_LIB_API std::unique_ptr<GCodePath> GenerateGCodePathSpiral(const std::vector<PolygonsD>& layer_outlines,
                                                                       const std::vector<double>& layer_zs,
                                                                       const GCodePrinterConfig& printer_config);

/**
 * @brief Convert PolygonsD to G-point sequence (helper function).
 * @param polys Input polygons.
 * @param z Z height.
 * @param config Path configuration.
 * @param is_extrude Whether extruding (true=printing, false=traveling).
 * @return GPoint sequence.
 */
HSBA_SLICER_LIB_API std::vector<GPoint> PolygonsToGPoints(const PolygonsD& polys, float z, const FdmPathConfig& config,
                                                          bool is_extrude);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_PATH_GENERATOR_HPP
