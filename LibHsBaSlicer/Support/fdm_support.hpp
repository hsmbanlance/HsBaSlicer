#pragma once
#ifndef HSBA_SLICER_LIB_FDM_SUPPORT_HPP
#define HSBA_SLICER_LIB_FDM_SUPPORT_HPP

#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"
#include "support/SupportConfig.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Generate FDM support cross-section for a single layer.
 * @param current_layer Current layer outline.
 * @param prev_layer Previous layer outline (empty for first layer).
 * @param layer_height Layer height (mm).
 * @param config FDM support configuration.
 * @return Support cross-section polygons.
 */
HSBA_SLICER_LIB_API PolygonsD GenerateFdmSupport(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                                                 float layer_height, const Support::FdmSupportConfig& config);

/**
 * @brief Generate FDM support for all layers.
 * @param layers All layer outlines (bottom to top).
 * @param config FDM support configuration.
 * @return Support cross-section polygons per layer.
 */
HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllFdmSupport(const std::vector<PolygonsD>& layers,
                                                                 const Support::FdmSupportConfig& config);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_FDM_SUPPORT_HPP
