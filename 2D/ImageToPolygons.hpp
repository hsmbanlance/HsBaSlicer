#pragma once
#ifndef HSBA_SLICER_IMAGETOPOLYGONS_HPP
#define HSBA_SLICER_IMAGETOPOLYGONS_HPP

#include "FloatPolygons.hpp"
#include <functional>
#include <string>
#include <vector>

// forward-declare lua state to avoid including lua.hpp in this header
struct lua_State;

inline constexpr uint8_t MAX_GRAY_VALUE = 255;  ///< Maximum grayscale value
inline constexpr uint8_t MIN_GRAY_VALUE = 0;    ///< Minimum grayscale value

namespace HsBa::Slicer
{
/**
 * @brief Convert an image to polygons by thresholding.
 * @param path Path to the input image file.
 * @param threshold Grayscale threshold for binarization (default: 128).
 * @param pixelSize Size of each pixel in output coordinates (default: 1.0).
 * @return Polygons extracted from the image.
 */
PolygonsD FromImage(const std::string& path, int threshold = 128, double pixelSize = 1.0);

/**
 * @brief Convert an image to multiple polygon sets using different thresholds.
 * @param path Path to the input image file.
 * @param thresholds Vector of grayscale thresholds for binarization.
 * @param pixelSize Size of each pixel in output coordinates (default: 1.0).
 * @return Vector of polygon sets, one for each threshold.
 */
std::vector<PolygonsD> FromImageMulti(const std::string& path, const std::vector<int>& thresholds,
                                      double pixelSize = 1.0);

/**
 * @brief Render polygons to an image file.
 * @param polys Input polygons to render.
 * @param width Width of the output image in pixels.
 * @param height Height of the output image in pixels.
 * @param pixelSize Size mapping from polygon coordinates to pixels.
 * @param outPath Output image file path.
 * @param foreground Foreground color value (default: 255 - white).
 * @param background Background color value (default: 0 - black).
 * @return true if rendering succeeded, false otherwise.
 */
bool ToImage(const PolygonsD& polys, int width, int height, double pixelSize, const std::string& outPath,
             uint8_t foreground = MAX_GRAY_VALUE, uint8_t background = MIN_GRAY_VALUE);

/**
 * @brief Render polygons to an image using a Lua script for customization.
 * @param poly Input polygons to render.
 * @param scriptPath Path to the Lua script file.
 * @param outPath Output image file path (default: "output.png").
 * @param functionName Lua function name to call (default: "generate_image").
 * @param lua_reg Optional Lua registration callback.
 * @return true if rendering succeeded, false otherwise.
 */
bool LuaToImage(const PolygonsD& poly, const std::string& scriptPath, const std::string& outPath = "output.png",
                const std::string& functionName = "generate_image",
                const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief Render polygons to an image using inline Lua script code.
 * @param poly Input polygons to render.
 * @param script Inline Lua script code string.
 * @param outPath Output image file path (default: "output.png").
 * @param functionName Lua function name to call (default: "generate_image").
 * @param lua_reg Optional Lua registration callback.
 * @return true if rendering succeeded, false otherwise.
 */
bool LuaToImageString(const PolygonsD& poly, const std::string& script, const std::string& outPath = "output.png",
                      const std::string& functionName = "generate_image",
                      const std::function<void(lua_State*)>& lua_reg = {});
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_IMAGETOPOLYGONS_HPP
