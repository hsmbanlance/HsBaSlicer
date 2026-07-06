#pragma once
#ifndef HSBA_SLICER_ISUPPORT_HPP
#define HSBA_SLICER_ISUPPORT_HPP

#include <vector>

#include "2D/FloatPolygons.hpp"
#include "SupportConfig.hpp"

namespace HsBa::Slicer::Support
{
/**
 * @brief Abstract interface for support generators (FDM, SLA, Lua custom, etc.).
 *
 * Each implementation receives the current layer and previous layer contours,
 * detects overhang regions, and produces support cross-sections for the current layer.
 */
class ISupport
{
public:
    virtual ~ISupport() = default;

    /**
     * @brief Generate support cross-sections for a single layer.
     * @param current_layer Current layer contours.
     * @param prev_layer Previous layer contours (empty for first layer).
     * @param layer_height Layer height in mm.
     * @param config Support configuration.
     * @return Support cross-section polygons for this layer.
     */
    virtual PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                               const SupportConfig& config) = 0;

    /**
     * @brief Generate supports for all layers (default implementation iterates).
     * @param layers All layer contours from bottom to top.
     * @param config Support configuration.
     * @return Vector of support polygons per layer (same size as input).
     */
    virtual std::vector<PolygonsD> GenerateAll(const std::vector<PolygonsD>& layers, const SupportConfig& config);
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_ISUPPORT_HPP
