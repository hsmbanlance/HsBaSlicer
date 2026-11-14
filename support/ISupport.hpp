/** @file ISupport.hpp
 * @brief Abstract interface for per-layer support generators.
 * @author HsBa
 */
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
     * @brief Support cross-sections for one layer from an already-detected overhang region.
     *
     * GenerateAll needs the pattern separated from the detection because a support column
     * has to be re-deposited on every layer below its overhang until it reaches either the
     * build plate or solid model; the region therefore travels downward and is handed back
     * here layer by layer. The default routes the region through Generate as an entirely
     * unsupported layer, which is exactly what Detect(current, {}) computes (an identity),
     * so generators that only implement Generate keep their current pattern semantics.
     *
     * @param overhang_region Region of this layer that material below must hold up.
     * @param layer_height Layer height in mm.
     * @param config Support configuration.
     * @return Support cross-section polygons for this layer.
     */
    virtual PolygonsD GenerateFromOverhangs(const PolygonsD& overhang_region, float layer_height,
                                            const SupportConfig& config)
    {
        return Generate(overhang_region, PolygonsD{}, layer_height, config);
    }

    /**
     * @brief Generate supports for all layers (default implementation iterates).
     *
     * Each overhang is supported by a column that continues downward until the build plate
     * or solid model takes over, so a layer's support is never an isolated floating slice.
     * Generators whose script decides the whole structure themselves (LuaSupport) override
     * this and keep a plain per-layer loop.
     *
     * @param layers All layer contours from bottom to top.
     * @param config Support configuration.
     * @return Vector of support polygons per layer (same size as input).
     */
    virtual std::vector<PolygonsD> GenerateAll(const std::vector<PolygonsD>& layers, const SupportConfig& config);

    /**
     * @brief Whether the per-layer overhang detection can be run in parallel.
     *
     * Detecting one layer only reads that layer and its predecessor, so when the concrete
     * generator declares itself independent the detection phase of GenerateAll is dispatched
     * across the shared pipeline ThreadPool. Subclasses whose Generate relies on state
     * accumulated from previous layers (e.g. tree branch growth) must override this to false
     * so GenerateAll falls back to a serial loop. The downward column growth that follows is
     * inherently serial in every case.
     */
    virtual bool IsLayerIndependent() const { return true; }
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_ISUPPORT_HPP
