/** @file path_generator.cpp
 * @brief Implementation of FDM G-code path generation (per-layer paths, multi-firmware and spiral/vase mode).
 * @author HsBa
 */
#include "path_generator.hpp"

#include <cmath>
#include <format>

#include "spiral_path.hpp"

namespace HsBa::Slicer
{
HSBA_SLICER_LIB_API std::vector<GPoint> PolygonsToGPoints(const PolygonsD& polys, float z, const FdmPathConfig& config,
                                                          bool is_extrude)
{
    std::vector<GPoint> points;
    points.reserve(polys.size() * 4);  // Rough estimate

    for (const auto& poly : polys)
    {
        if (poly.empty())
            continue;

        // First point: travel to the start
        GPoint travel;
        travel.type = GcodeType::G0;
        travel.p1 = {static_cast<float>(poly.front().x), static_cast<float>(poly.front().y), z};
        travel.velocity = config.travel_speed;
        travel.extrusion = 0.0;
        points.push_back(travel);

        // Subsequent points: print to each vertex
        for (size_t i = 1; i <= poly.size(); ++i)
        {
            const auto& pt = poly[i % poly.size()];
            GPoint gpt;
            gpt.type = GcodeType::G1;
            gpt.p1 = {static_cast<float>(pt.x), static_cast<float>(pt.y), z};
            gpt.velocity = is_extrude ? config.print_speed : config.travel_speed;

            if (is_extrude)
            {
                // Compute extrusion: line_width * layer_height * segment_length * multiplier
                const auto& prev = poly[(i - 1) % poly.size()];
                double dx = pt.x - prev.x;
                double dy = pt.y - prev.y;
                double seg_len = std::sqrt(dx * dx + dy * dy);
                gpt.extrusion = config.line_width * config.layer_height * seg_len * config.extrusion_multiplier;
            }
            else
            {
                gpt.extrusion = 0.0;
            }
            points.push_back(gpt);
        }
    }
    return points;
}

HSBA_SLICER_LIB_API std::unique_ptr<PointsPath> GenerateGCodePath(const std::vector<LayerPathData>& layer_data,
                                                                  const FdmPathConfig& config)
{
    auto path = std::make_unique<PointsPath>(config.units);

    for (const auto& layer : layer_data)
    {
        float z = layer.z_height;

        // 1. Print outlines (outer wall)
        auto outline_pts = PolygonsToGPoints(layer.outlines, z, config, true);
        for (auto& pt : outline_pts)
        {
            path->push_back(pt);
        }

        // 2. Print infill (interior)
        auto fill_pts = PolygonsToGPoints(layer.fills, z, config, true);
        for (auto& pt : fill_pts)
        {
            path->push_back(pt);
        }

        // 3. Print support
        auto support_pts = PolygonsToGPoints(layer.supports, z, config, true);
        for (auto& pt : support_pts)
        {
            path->push_back(pt);
        }
    }

    return path;
}

HSBA_SLICER_LIB_API std::unique_ptr<GCodePath> GenerateGCodePathV2(const std::vector<LayerPathData>& layer_data,
                                                                   const FdmPathConfig& config,
                                                                   const GCodePrinterConfig& printer_config)
{
    auto path = std::make_unique<GCodePath>(printer_config);

    for (const auto& layer : layer_data)
    {
        // Encode Z height in layer config string
        std::string layer_config = std::format("Z:{:.6f}", layer.z_height);

        // Combine all polygons for this layer: outlines + fills + supports
        PolygonsD combined;
        combined.insert(combined.end(), layer.outlines.begin(), layer.outlines.end());
        combined.insert(combined.end(), layer.fills.begin(), layer.fills.end());
        combined.insert(combined.end(), layer.supports.begin(), layer.supports.end());

        path->push_back(layer_config, combined);
    }

    return path;
}

HSBA_SLICER_LIB_API std::unique_ptr<GCodePath> GenerateGCodePathSpiral(const std::vector<PolygonsD>& layer_outlines,
                                                                       const std::vector<double>& layer_zs,
                                                                       const GCodePrinterConfig& printer_config)
{
    auto path = std::make_unique<GCodePath>(printer_config);

    // Merge all per-layer outer contours into one continuous, Z-rising helix.
    const std::vector<SpiralPoint> helix = SpiralizeOuterWall(layer_outlines, layer_zs);

    std::vector<PathPoint3D> wall;
    wall.reserve(helix.size());
    for (const auto& pt : helix)
        wall.push_back(PathPoint3D{pt.x, pt.y, pt.z});

    // Vase mode emits only the continuous wall (no per-layer polygons); ToGCode
    // writes header + the single unbroken helix + footer.
    path->setContinuousWall(std::move(wall));
    return path;
}

}  // namespace HsBa::Slicer
