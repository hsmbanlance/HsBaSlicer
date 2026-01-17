#pragma once
#ifndef HSBA_SLICER_IMAGETOPOLYGONS_HPP
#define HSBA_SLICER_IMAGETOPOLYGONS_HPP

#include "FloatPolygons.hpp"
#include <string>
#include <vector>

inline constexpr uint8_t MAX_GRAY_VALUE = 255;
inline constexpr uint8_t MIN_GRAY_VALUE = 0;

namespace HsBa::Slicer
{
    PolygonsD FromImage(const std::string& path, int threshold = 128, double pixelSize = 1.0);

    std::vector<PolygonsD> FromImageMulti(const std::string& path, const std::vector<int>& thresholds, double pixelSize = 1.0);

    bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
        uint8_t foreground = MAX_GRAY_VALUE, uint8_t background = MIN_GRAY_VALUE);

    bool LuaToImage(const PolygonsD& poly, const std::string& scriptPath,
     const std::string& outPath = "output.png", const std::string& functionName = "generate_image");

    bool LuaToImageString(const PolygonsD& poly, const std::string& script,
        const std::string& outPath = "output.png", const std::string& functionName = "generate_image");
}

#endif // HSBA_SLICER_IMAGETOPOLYGONS_HPP
