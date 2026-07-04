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
 * @brief 生成单层FDM支撑截面。
 * @param current_layer 当前层轮廓。
 * @param prev_layer 前一层轮廓（首层为空）。
 * @param layer_height 层高（mm）。
 * @param config FDM支撑配置。
 * @return 支撑截面多边形。
 */
HSBA_SLICER_LIB_API PolygonsD GenerateFdmSupport(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                                                  float layer_height, const Support::FdmSupportConfig& config);

/**
 * @brief 生成所有层的FDM支撑。
 * @param layers 所有层轮廓（从底到顶）。
 * @param config FDM支撑配置。
 * @return 每层的支撑截面多边形。
 */
HSBA_SLICER_LIB_API std::vector<PolygonsD> GenerateAllFdmSupport(const std::vector<PolygonsD>& layers,
                                                                  const Support::FdmSupportConfig& config);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_FDM_SUPPORT_HPP
