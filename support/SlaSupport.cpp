#include "SlaSupport.hpp"

#include <algorithm>
#include <cmath>

#include "2D/IntPolygon.hpp"
#include "OverhangDetector.hpp"

namespace HsBa::Slicer::Support
{
namespace
{
bool PointInsidePolygonsD(double x, double y, const PolygonsD& polys)
{
    Clipper2Lib::Point64 p64{static_cast<int64_t>(std::llround(x * integerization)),
                             static_cast<int64_t>(std::llround(y * integerization))};
    const Polygons polys_int = Integerization(polys);
    auto r = PointInPolygons(p64, polys_int);
    return r != Clipper2Lib::PointInPolygonResult::IsOutside;
}

// Local wrappers to resolve ADL ambiguity between HsBa::Slicer:: and Clipper2Lib:: overloads
PolygonsD UnionD(const PolygonsD& left, const PolygonsD& right,
                 Clipper2Lib::FillRule fr = Clipper2Lib::FillRule::EvenOdd)
{
    return Clipper2Lib::Union(left, right, fr);
}

PolygonsD UnionD(const PolygonsD& subjects, Clipper2Lib::FillRule fr)
{
    return Clipper2Lib::Union(subjects, fr);
}
}  // namespace
PolygonsD SlaSacrificialSupport::SampleSupportPoints(const PolygonsD& overhang, double tip_radius, double spacing)
{
    if (overhang.empty() || spacing <= 0.0)
        return {};

    // Get bounding box
    double min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    for (const auto& poly : overhang)
    {
        for (const auto& pt : poly)
        {
            min_x = std::min(min_x, pt.x);
            min_y = std::min(min_y, pt.y);
            max_x = std::max(max_x, pt.x);
            max_y = std::max(max_y, pt.y);
        }
    }

    if (min_x > max_x || min_y > max_y)
        return {};

    PolygonsD support_points;
    for (double y = min_y; y <= max_y; y += spacing)
    {
        for (double x = min_x; x <= max_x; x += spacing)
        {
            if (PointInsidePolygonsD(x, y, overhang))
            {
                support_points.push_back(MakeCircle(x, y, tip_radius, 16));
            }
        }
    }

    return support_points;
}

PolygonsD SlaSacrificialSupport::Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                                          float layer_height, const SlaSupportConfig& config)
{
    PolygonsD overhang =
        OverhangDetector::Detect(current_layer, prev_layer, layer_height, config.overhang_angle_threshold);
    if (overhang.empty())
        return {};

    // Apply gap
    const double gap = static_cast<double>(config.support_gap);
    const Polygons overhang_int = Integerization(overhang);
    const Polygons gapped =
        Offset(overhang_int, -gap * integerization, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    if (gapped.empty())
        return {};

    const PolygonsD gapped_d = UnIntegerization(gapped);

    // SLA sacrificial supports use small tip diameter
    const double tip_radius = static_cast<double>(config.tip_diameter) * 0.5;
    // Spacing between support points: use support_diameter as spacing
    const double spacing = static_cast<double>(config.support_diameter) * 2.0;

    PolygonsD points = SampleSupportPoints(gapped_d, tip_radius, spacing);
    if (points.empty())
        return {};

    // Union all support point circles
    PolygonsD result;
    result.reserve(points.size());
    for (auto& p : points)
        result.push_back(std::move(p));

    return UnionD(result, Clipper2Lib::FillRule::NonZero);
}

PolygonsD SlaSacrificialSupport::Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer,
                                          float layer_height, const SupportConfig& config)
{
    // Convert base config to SLA config (use defaults for SLA-specific fields)
    SlaSupportConfig sla_config;
    static_cast<SupportConfig&>(sla_config) = config;
    return Generate(current_layer, prev_layer, layer_height, sla_config);
}
}  // namespace HsBa::Slicer::Support
