#pragma once
#ifndef HSBA_SLICER_LIB_PATH_GENERATOR_HPP
#define HSBA_SLICER_LIB_PATH_GENERATOR_HPP

#include <memory>
#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"
#include "paths/pointspath.hpp"

namespace HsBa::Slicer
{
/**
 * @brief FDM路径生成配置
 */
struct FdmPathConfig
{
    float layer_height = 0.2f;          ///< 层高（mm）
    float line_width = 0.4f;            ///< 线宽（mm）
    float print_speed = 50.0f;          ///< 打印速度（mm/s）
    float travel_speed = 100.0f;        ///< 空走速度（mm/s）
    float extrusion_multiplier = 1.0f;  ///< 挤出量倍率
    GCodeUnits units = GCodeUnits::mm;  ///< 单位
};

/**
 * @brief 单层路径数据
 */
struct LayerPathData
{
    PolygonsD outlines;     ///< 轮廓路径
    PolygonsD fills;        ///< 填充路径
    PolygonsD supports;     ///< 支撑路径
    float z_height = 0.0f;  ///< 层高度
};

/**
 * @brief 生成G-code路径。
 * @param layer_data 各层路径数据（轮廓、填充、支撑）。
 * @param config FDM路径配置。
 * @return G-code路径对象。
 */
HSBA_SLICER_LIB_API std::unique_ptr<PointsPath> GenerateGCodePath(const std::vector<LayerPathData>& layer_data,
                                                                  const FdmPathConfig& config);

/**
 * @brief 将PolygonsD转换为G-point序列（辅助函数）。
 * @param polys 输入多边形。
 * @param z Z高度。
 * @param config 路径配置。
 * @param is_extrude 是否挤出（true=打印，false=空走）。
 * @return GPoint序列。
 */
HSBA_SLICER_LIB_API std::vector<GPoint> PolygonsToGPoints(const PolygonsD& polys, float z, const FdmPathConfig& config,
                                                          bool is_extrude);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_PATH_GENERATOR_HPP
