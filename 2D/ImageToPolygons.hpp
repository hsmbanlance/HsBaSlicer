#pragma once
#ifndef HSBA_SLICER_IMAGETOPOLYGONS_HPP
#define HSBA_SLICER_IMAGETOPOLYGONS_HPP

#include "FloatPolygons.hpp"
#include <string>
#include <vector>

namespace HsBa::Slicer
{
    PolygonsD FromImage(const std::string& path, int threshold = 128, double pixelSize = 1.0);

    std::vector<PolygonsD> FromImageMulti(const std::string& path, const std::vector<int>& thresholds, double pixelSize = 1.0);

    bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
        uint8_t foreground = 255, uint8_t background = 0);

    bool LuaToImage(const PolygonsD& poly, const std::string& scriptPath,
     const std::string& outPath = "output.png", const std::string& functionName = "generate_image");
}

#endif // HSBA_SLICER_IMAGETOPOLYGONS_HPP
