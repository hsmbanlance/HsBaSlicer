/** @file path_generator.cpp
 * @brief Implementation of FDM G-code path generation (per-layer paths, multi-firmware and spiral/vase mode).
 * @author HsBa
 */
#include "path_generator.hpp"

#include <cmath>
#include <format>
#include <string>
#include <utility>

#include "base/parallel.hpp"
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
        {
            continue;
        }

        // First point: travel to the start
        GPoint travel;
        travel.type = GcodeType::G0;
        travel.p1 = {.x = static_cast<float>(poly.front().x), .y = static_cast<float>(poly.front().y), .z = z};
        travel.velocity = config.travel_speed;
        travel.extrusion = 0.0;
        points.push_back(travel);

        // Subsequent points: print to each vertex
        for (size_t i = 1; i <= poly.size(); ++i)
        {
            const auto& pt = poly[i % poly.size()];
            GPoint gpt;
            gpt.type = GcodeType::G1;
            gpt.p1 = {.x = static_cast<float>(pt.x), .y = static_cast<float>(pt.y), .z = z};
            gpt.velocity = is_extrude ? config.print_speed : config.travel_speed;

            if (is_extrude)
            {
                // Compute extrusion: line_width * layer_height * segment_length * multiplier
                const auto& prev = poly[(i - 1) % poly.size()];
                double const dx = pt.x - prev.x;
                double const dy = pt.y - prev.y;
                double const seg_len = std::sqrt(dx * dx + dy * dy);
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

    // Each layer's PolygonsToGPoints call is a pure per-layer function of the
    // three polygon groups + z, so we generate the G-point lists in parallel
    // and only touch the shared PointsPath on the caller thread to preserve
    // deterministic ordering and thread safety.
    const int total = static_cast<int>(layer_data.size());
    struct LayerPts
    {
        std::vector<GPoint> outlines;
        std::vector<GPoint> fills;
        std::vector<GPoint> supports;
    };
    auto per_layer = HsBa::Slicer::Pipeline::ParallelMapLayers<LayerPts>(
        total,
        [&layer_data, &config](int i) -> LayerPts
        {
            const auto& layer = layer_data[static_cast<size_t>(i)];
            float const z = layer.z_height;
            LayerPts out;
            out.outlines = PolygonsToGPoints(layer.outlines, z, config, true);
            out.fills = PolygonsToGPoints(layer.fills, z, config, true);
            out.supports = PolygonsToGPoints(layer.supports, z, config, true);
            return out;
        });

    for (auto& lp : per_layer)
    {
        for (auto& pt : lp.outlines)
        {
            path->push_back(pt);
        }
        for (auto& pt : lp.fills)
        {
            path->push_back(pt);
        }
        for (auto& pt : lp.supports)
        {
            path->push_back(pt);
        }
    }

    return path;
}

HSBA_SLICER_LIB_API std::unique_ptr<GCodePath> GenerateGCodePathV2(const std::vector<LayerPathData>& layer_data,
                                                                   const FdmPathConfig& /*config*/,
                                                                   const GCodePrinterConfig& printer_config)
{
    auto path = std::make_unique<GCodePath>(printer_config);

    // Building the combined PolygonsD per layer dominates this stage and is
    // embarrassingly parallel; GCodePath::push_back remains on the caller
    // thread so layers_ ordering and callback semantics are unchanged.
    const int total = static_cast<int>(layer_data.size());
    struct LayerBundle
    {
        std::string layer_config;
        PolygonsD combined;
    };
    auto per_layer = HsBa::Slicer::Pipeline::ParallelMapLayers<LayerBundle>(
        total,
        [&layer_data](int i) -> LayerBundle
        {
            const auto& layer = layer_data[static_cast<size_t>(i)];
            LayerBundle bundle;
            bundle.layer_config = std::format("Z:{:.6f}", layer.z_height);
            bundle.combined.reserve(layer.outlines.size() + layer.fills.size() + layer.supports.size());
            bundle.combined.insert(bundle.combined.end(), layer.outlines.begin(), layer.outlines.end());
            bundle.combined.insert(bundle.combined.end(), layer.fills.begin(), layer.fills.end());
            bundle.combined.insert(bundle.combined.end(), layer.supports.begin(), layer.supports.end());
            return bundle;
        });

    for (auto& bundle : per_layer)
    {
        path->push_back(bundle.layer_config, bundle.combined);
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
    {
        wall.push_back(PathPoint3D{.x = pt.x, .y = pt.y, .z = pt.z});
    }

    // Vase mode emits only the continuous wall (no per-layer polygons); ToGCode
    // writes header + the single unbroken helix + footer.
    path->setContinuousWall(std::move(wall));
    return path;
}

}  // namespace HsBa::Slicer
