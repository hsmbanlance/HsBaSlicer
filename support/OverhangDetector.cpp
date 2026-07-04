#include "OverhangDetector.hpp"

#include <cmath>
#include <numbers>

#include "2D/IntPolygon.hpp"

namespace HsBa::Slicer::Support
{
namespace
{
// Local wrapper to resolve ADL ambiguity between HsBa::Slicer:: and Clipper2Lib:: overloads
PolygonsD DifferenceD(const PolygonsD& left, const PolygonsD& right,
                      Clipper2Lib::FillRule fr = Clipper2Lib::FillRule::EvenOdd)
{
    return Clipper2Lib::Difference(left, right, fr);
}
}  // namespace
double OverhangDetector::MaxBridgeDistance(float layer_height, float angle_deg)
{
    // max horizontal distance = layer_height / tan(angle)
    // For angle >= 90, no overhang (vertical wall is fine)
    if (angle_deg >= 90.0f)
        return 0.0;
    // For angle <= 0, everything is overhang
    if (angle_deg <= 0.0f)
        return 1e9;

    const double angle_rad = static_cast<double>(angle_deg) * std::numbers::pi / 180.0;
    return static_cast<double>(layer_height) / std::tan(angle_rad);
}

PolygonsD OverhangDetector::Detect(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                                   float angle_threshold_deg)
{
    // No previous layer: entire current layer is overhang
    if (prev_layer.empty())
        return current_layer;

    // Difference: regions in current_layer not covered by prev_layer
    PolygonsD diff = DifferenceD(current_layer, prev_layer, Clipper2Lib::FillRule::NonZero);

    if (diff.empty())
        return {};

    // Compute max bridge distance based on angle threshold
    const double max_bridge = MaxBridgeDistance(layer_height, angle_threshold_deg);

    if (max_bridge <= 0.0)
        return diff;

    // Erode the difference by max_bridge distance to filter small overhangs
    // that can be bridged. Erosion = negative offset.
    // Convert to integer, offset, convert back
    const Polygons diff_int = Integerization(diff);
    const double bridge_int = max_bridge * integerization;

    // Negative offset (erosion) with round join to smooth corners
    const Polygons eroded = Offset(diff_int, -bridge_int, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    if (eroded.empty())
        return {};

    return UnIntegerization(eroded);
}

PolygonsD OverhangDetector::Detect(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                                   const SupportConfig& config)
{
    return Detect(current_layer, prev_layer, config.layer_height, config.overhang_angle_threshold);
}
}  // namespace HsBa::Slicer::Support
