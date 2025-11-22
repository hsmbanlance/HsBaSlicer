#pragma once
#ifndef HSBA_SLICER_IMAGETOPOLYGONS_HPP
#define HSBA_SLICER_IMAGETOPOLYGONS_HPP

#include "FloatPolygons.hpp"
#include <string>

namespace HsBa::Slicer
{
    // 从图片加载并提取多边形（支持 BMP/PNG/JPEG），返回浮点坐标的多边形集合
    PolygonsD FromImage(const std::string& path, int threshold = 128, double pixelSize = 1.0);

    // 将多边形栅格化为灰度 PNG 文件
    bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
        uint8_t foreground = 255, uint8_t background = 0);
}

#endif // HSBA_SLICER_IMAGETOPOLYGONS_HPP
