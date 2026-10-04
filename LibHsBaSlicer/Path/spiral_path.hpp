/** @file spiral_path.hpp
 * @brief Continuous helical (spiralized) outer-wall path generation API for extrusion-style deposition.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_SPIRAL_PATH_HPP
#define HSBA_SLICER_SPIRAL_PATH_HPP

#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"

namespace HsBa::Slicer
{
/**
 * @brief A single point on a continuous 3D deposition path.
 *
 * X/Y are planar coordinates (same units as the sliced contours); Z is the
 * absolute build height for that point. Unlike LayerPathData (which keeps a
 * constant Z per layer), a spiralized wall carries a distinct Z for every point
 * so the extrusion rises continuously between layers.
 */
struct SpiralPoint
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/**
 * @brief Build a continuous helical ("spiralized") outer-wall path from per-layer closed contours.
 *
 * This implements the classic spiralize / helical outer contour strategy for
 * extrusion-style deposition processes (FDM, WAAM, binder/wire 3DP, ...): the
 * outer wall is emitted as ONE uninterrupted, extrusion-continuous polyline
 * whose Z rises by exactly one layer per revolution. There are no retractions
 * or per-layer travel jumps, which improves surface quality and wall strength.
 *
 * For each layer the outermost closed contour (largest absolute signed area) is
 * selected, its seam vertex is rotated to sit closest to the previous layer's
 * seam in XY (to keep a single fixed seam line), and the loop is traversed while
 * linearly ramping Z from this layer's height to the next layer's height. The
 * last layer (nothing above to connect to) is traversed flat at its own height.
 *
 * The result is geometrically exact for uniform-cross-section prismatic walls
 * (e.g. cylinders/prisms). For varying cross-sections the seam alignment still
 * yields a connected path; layers without a valid closed contour are skipped.
 *
 * @param layer_sections Per-layer contours (each layer = a set of closed polygons).
 *        Only the outermost closed contour of each layer is used.
 * @param layer_zs Build height (Z) of each layer; must have the same size as
 *        layer_sections. layer_zs[i] is the height at the start of layer i's loop.
 * @return The ordered continuous 3D polyline. Empty when fewer than one valid
 *         contour is available.
 */
HSBA_SLICER_LIB_API std::vector<SpiralPoint> SpiralizeOuterWall(const std::vector<PolygonsD>& layer_sections,
                                                                const std::vector<double>& layer_zs);

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_SPIRAL_PATH_HPP
