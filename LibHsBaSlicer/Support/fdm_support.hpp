#pragma once
#ifndef HSBA_SLICER_LIB_FDM_SUPPORT_HPP
#define HSBA_SLICER_LIB_FDM_SUPPORT_HPP

#include <string>
#include <string_view>
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

/**
 * @brief Generate SLA sacrificial support for all layers.
 *
 * Uses the SLA-specific support configuration to generate thin-column
 * sacrificial supports suitable for resin printing.
 *
 * @param layers All layer outlines (bottom to top).
 * @param config SLA support configuration.
 * @return Support cross-section polygons per layer.
 */
HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllSlaSupport(const std::vector<PolygonsD>& layers,
                                                                 const Support::SlaSupportConfig& config);

/**
 * @brief Generate custom support for all layers using a Lua script.
 *
 * The Lua script receives layer data and configuration, and returns
 * support polygons. This replaces the built-in support algorithm.
 *
 * @param layers All layer outlines (bottom to top).
 * @param config General support configuration.
 * @param script Inline Lua script source code.
 * @param functionName Lua function name to call (default: "generate_support").
 * @return Support cross-section polygons per layer.
 */
HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllLuaSupport(const std::vector<PolygonsD>& layers,
                                                                 const Support::SupportConfig& config,
                                                                 std::string_view script,
                                                                 std::string_view functionName = "generate_support");

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_FDM_SUPPORT_HPP
