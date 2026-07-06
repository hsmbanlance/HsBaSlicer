#include "FdmSupport.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

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

PolygonsD IntersectD(const PolygonsD& left, const PolygonsD& right,
                     Clipper2Lib::FillRule fr = Clipper2Lib::FillRule::EvenOdd)
{
    return Clipper2Lib::Intersect(left, right, fr);
}
}  // namespace
// ---------------------------------------------------------------------------
// FdmPlaneSupport
// ---------------------------------------------------------------------------
PolygonsD FdmPlaneSupport::Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                                    const SupportConfig& config)
{
    // Detect overhang regions
    PolygonsD overhang =
        OverhangDetector::Detect(current_layer, prev_layer, layer_height, config.overhang_angle_threshold);
    if (overhang.empty())
        return {};

    // Apply gap: shrink overhang by gap distance to separate support from model
    const double gap = static_cast<double>(config.support_gap);
    const Polygons overhang_int = Integerization(overhang);
    const Polygons gapped =
        Offset(overhang_int, -gap * integerization, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    if (gapped.empty())
        return {};

    // Inflate by support_diameter/2 to create column cross-sections
    const double radius = static_cast<double>(config.support_diameter) * 0.5;
    const Polygons columns =
        Offset(gapped, radius * integerization, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

    return UnIntegerization(columns);
}

// ---------------------------------------------------------------------------
// FdmTreeSupport
// ---------------------------------------------------------------------------
PolygonsD FdmTreeSupport::GenerateBranches(const PolygonsD& overhang, float layer_height, const SupportConfig& config)
{
    // For tree support, we place circular branch tips at regular sample points
    // within the overhang region, with radius determined by branch angle.
    // The branch radius at the tip = support_diameter / 2
    // Branches expand as they go down (not modeled in 2D cross-section;
    // the expansion is handled layer-by-layer in GenerateAll).

    const double tip_radius = static_cast<double>(config.support_diameter) * 0.5;
    const double spacing = static_cast<double>(config.support_diameter) * 1.5;  // spacing between branch points

    // Get bounding box of overhang
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

    // Generate grid of candidate branch points
    PolygonsD branch_circles;
    for (double y = min_y; y <= max_y; y += spacing)
    {
        for (double x = min_x; x <= max_x; x += spacing)
        {
            // Check if point is inside overhang region
            if (PointInsidePolygonsD(x, y, overhang))
            {
                // Create a circle at this point
                branch_circles.push_back(MakeCircle(x, y, tip_radius, 24));
            }
        }
    }

    // Union all branch circles
    if (branch_circles.empty())
        return {};

    PolygonsD result;
    result.reserve(branch_circles.size());
    for (auto& c : branch_circles)
        result.push_back(std::move(c));
    // Collapse overlapping circles via Clipper2 union
    return UnionD(result, Clipper2Lib::FillRule::NonZero);
}

PolygonsD FdmTreeSupport::Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                                   const SupportConfig& config)
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
    return GenerateBranches(gapped_d, layer_height, config);
}

// ---------------------------------------------------------------------------
// FdmHoneycombSupport
// ---------------------------------------------------------------------------
PolygonsD FdmHoneycombSupport::GenerateHoneycomb(const PolygonsD& overhang, const SupportConfig& config)
{
    const double cell_size = static_cast<double>(config.honeycomb_cell_size);
    if (cell_size <= 0.0)
        return {};

    // Honeycomb geometry: flat-top hexagons
    // Width (x-direction) = cell_size, Height (y-direction) = cell_size * sqrt(3)/2
    const double hex_w = cell_size;
    const double hex_h = cell_size * std::sqrt(3.0) * 0.5;

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

    // Generate hexagonal cells
    PolygonsD hex_cells;
    int row = 0;
    for (double y = min_y; y <= max_y + hex_h; y += hex_h, ++row)
    {
        const double x_offset = (row % 2 == 0) ? 0.0 : hex_w * 0.75;
        for (double x = min_x + x_offset; x <= max_x + hex_w; x += hex_w * 1.5)
        {
            // Create flat-top hexagon centered at (x, y)
            PolygonD hex;
            hex.reserve(6);
            for (int i = 0; i < 6; ++i)
            {
                const double angle = std::numbers::pi / 3.0 * static_cast<double>(i);
                hex.emplace_back(x + cell_size * 0.5 * std::cos(angle), y + cell_size * 0.5 * std::sin(angle));
            }
            hex_cells.push_back(std::move(hex));
        }
    }

    if (hex_cells.empty())
        return {};

    // Intersect hex cells with overhang region
    // First union all hex cells into a single polygon set, then intersect with overhang
    PolygonsD all_hex;
    all_hex.reserve(hex_cells.size());
    for (auto& h : hex_cells)
        all_hex.push_back(std::move(h));

    return IntersectD(UnionD(all_hex, Clipper2Lib::FillRule::NonZero), overhang, Clipper2Lib::FillRule::NonZero);
}

PolygonsD FdmHoneycombSupport::Generate(const PolygonsD& current_layer, const PolygonsD& prev_layer, float layer_height,
                                        const SupportConfig& config)
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
    return GenerateHoneycomb(gapped_d, config);
}
}  // namespace HsBa::Slicer::Support
