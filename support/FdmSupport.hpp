#pragma once
#ifndef HSBA_SLICER_FDM_SUPPORT_HPP
#define HSBA_SLICER_FDM_SUPPORT_HPP

#include "ISupport.hpp"

namespace HsBa::Slicer::Support
{
/**
 * @brief FDM Plane support: generates simple column supports from overhang regions.
 *
 * Overhang areas are inflated by support_diameter/2 to form cylindrical support columns.
 * A gap is applied between the model and support.
 */
class FdmPlaneSupport : public ISupport
{
public:
    PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                       const SupportConfig& config) override;
};

/**
 * @brief FDM Tree support: generates tree-like branching support structures.
 *
 * Branches grow from the build plate upward, merging when they meet.
 * In 2D cross-section this produces circular support points at branch tips
 * that connect downward with increasing radius.
 */
class FdmTreeSupport : public ISupport
{
public:
    PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                       const SupportConfig& config) override;

private:
    /**
     * @brief Generate tree branch cross-sections from overhang points.
     */
    static PolygonsD GenerateBranches(const PolygonsD& overhang, float layer_height, const SupportConfig& config);
};

/**
 * @brief FDM Honeycomb support: fills overhang regions with a honeycomb pattern.
 *
 * The overhang area is filled with hexagonal cells of configurable size,
 * providing a lightweight but rigid support structure.
 */
class FdmHoneycombSupport : public ISupport
{
public:
    PolygonsD Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                       const SupportConfig& config) override;

private:
    /**
     * @brief Generate hexagonal honeycomb pattern within a bounding region.
     */
    static PolygonsD GenerateHoneycomb(const PolygonsD& overhang, const SupportConfig& config);
};
}  // namespace HsBa::Slicer::Support

#endif  // HSBA_SLICER_FDM_SUPPORT_HPP
