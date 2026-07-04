#pragma once
#ifndef HSBA_SLICER_LIB_POLYGON_FILL_HPP
#define HSBA_SLICER_LIB_POLYGON_FILL_HPP

#include "../export.h"
#include "2D/IntPolygon.hpp"
#include "2D/PolygonFill.hpp"

namespace HsBa::Slicer
{
/**
 * @brief 对多边形进行填充。
 * @param poly 输入多边形。
 * @param spacing 填充线间距。
 * @param mode 填充模式（Line/SimpleZigzag/Zigzag）。
 * @param angle_deg 填充角度（度）。
 * @return 填充结果多边形。
 */
HSBA_SLICER_LIB_API Polygons FillPolygon(const Polygons& poly, double spacing, FillMode mode, double angle_deg);

/**
 * @brief 带边框的填充。
 * @param poly 输入多边形。
 * @param spacing 内部填充间距。
 * @param border_count 边框偏移圈数。
 * @param fill_mode 填充模式。
 * @param angle_deg 填充角度（度）。
 * @return 带边框的填充结果。
 */
HSBA_SLICER_LIB_API Polygons FillWithBorder(const Polygons& poly, double spacing, int border_count,
                                             FillMode fill_mode, double angle_deg);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_POLYGON_FILL_HPP
