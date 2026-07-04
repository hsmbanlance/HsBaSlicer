#pragma once
#ifndef HSBA_SLICER_OVERHANG_DETECTOR_HPP
#define HSBA_SLICER_OVERHANG_DETECTOR_HPP

#include "2D/FloatPolygons.hpp"
#include "SupportConfig.hpp"

namespace HsBa::Slicer::Support
{
/**
 * @brief Detect overhang regions between two adjacent layers.
 *
 * Overhang is defined as areas present in the current layer but not supported
 * by the previous layer, filtered by the overhang angle threshold.
 */
class OverhangDetector
{
public:
    /**
     * @brief Detect overhang regions for a single layer pair.
     * @param current_layer Current layer contours.
     * @param prev_layer Previous layer contours.
     * @param layer_height Layer height in mm.
     * @param angle_threshold_deg Overhang angle threshold in degrees.
     * @return Polygons representing overhang regions that need support.
     */
    static PolygonsD Detect(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                            float angle_threshold_deg);

    /**
     * @brief Detect overhang with full config.
     * @param current_layer Current layer contours.
     * @param prev_layer Previous layer contours.
     * @param config Support configuration (uses layer_height and overhang_angle_threshold).
     * @return Polygons representing overhang regions.
     */
    static PolygonsD Detect(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                            const SupportConfig& config);

    /**
     * @brief Compute the maximum horizontal bridge distance for a given angle and layer height.
     * @param layer_height Layer height in mm.
     * @param angle_deg Overhang angle threshold in degrees.
     * @return Maximum bridge distance in mm.
     */
    static double MaxBridgeDistance(float layer_height, float angle_deg);
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_OVERHANG_DETECTOR_HPP
