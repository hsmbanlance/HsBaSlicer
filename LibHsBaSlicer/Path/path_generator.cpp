#include "path_generator.hpp"

#include <cmath>
#include <format>

namespace HsBa::Slicer
{
HSBA_SLICER_LIB_API std::vector<GPoint> PolygonsToGPoints(const PolygonsD& polys, float z, const FdmPathConfig& config,
                                                          bool is_extrude)
{
    std::vector<GPoint> points;
    points.reserve(polys.size() * 4);  // 预估

    for (const auto& poly : polys)
    {
        if (poly.empty())
            continue;

        // 第一个点：空走到起点
        GPoint travel;
        travel.type = GcodeType::G0;
        travel.p1 = {static_cast<float>(poly.front().x), static_cast<float>(poly.front().y), z};
        travel.velocity = config.travel_speed;
        travel.extrusion = 0.0;
        points.push_back(travel);

        // 后续点：打印到各顶点
        for (size_t i = 1; i <= poly.size(); ++i)
        {
            const auto& pt = poly[i % poly.size()];
            GPoint gpt;
            gpt.type = GcodeType::G1;
            gpt.p1 = {static_cast<float>(pt.x), static_cast<float>(pt.y), z};
            gpt.velocity = is_extrude ? config.print_speed : config.travel_speed;

            if (is_extrude)
            {
                // 计算挤出量：线宽 * 层高 * 段长 * 倍率
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

        // 1. 打印轮廓（外壁）
        auto outline_pts = PolygonsToGPoints(layer.outlines, z, config, true);
        for (auto& pt : outline_pts)
        {
            path->push_back(pt);
        }

        // 2. 打印填充（内部）
        auto fill_pts = PolygonsToGPoints(layer.fills, z, config, true);
        for (auto& pt : fill_pts)
        {
            path->push_back(pt);
        }

        // 3. 打印支撑
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

}  // namespace HsBa::Slicer
