#pragma once
#ifndef HSBA_SLICER_SLA_SUPPORT_HPP
#define HSBA_SLICER_SLA_SUPPORT_HPP

#include "ISupport.hpp"

namespace HsBa::Slicer::Support
{
/**
 * @brief SLA Sacrificial support: generates thin column supports for resin printing.
 *
 * Sacrificial supports use small tip diameters for easy removal and larger
 * base diameters for build plate adhesion. The cross-section at each layer
 * is a circle whose diameter tapers from base to tip.
 */
class SlaSacrificialSupport : public ISupport
{
public:
    PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                       const SupportConfig& config) override;

    /**
     * @brief Generate with SLA-specific configuration.
     * @param current_layer Current layer contours.
     * @param prev_layer Previous layer contours.
     * @param layer_height Layer height in mm.
     * @param config SLA-specific support configuration.
     * @return Support cross-section polygons.
     */
    static PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                              const SlaSupportConfig& config);

private:
    /**
     * @brief Sample support points from overhang region.
     */
    static PolygonsD SampleSupportPoints(const PolygonsD& overhang, double tip_radius, double spacing);
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_SLA_SUPPORT_HPP
