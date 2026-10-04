/** @file waam_export.cpp
 * @brief Implementation of the WAAM robot-program export and robot path save API.
 * @author HsBa
 */
#include "waam_export.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "base/error.hpp"
#include "paths/weldrobotpath.hpp"
#include "spiral_path.hpp"

namespace HsBa::Slicer
{

namespace
{

/// @brief Map the C ABI robot type code to the internal RLType enum.
/// 0=ABB, 1=KUKA, 2=FANUC, anything else=Unknown.
RLType ToRobotType(int robot_type)
{
    switch (robot_type)
    {
    case 0:
        return RLType::Abb;
    case 1:
        return RLType::Kuka;
    case 2:
        return RLType::Fanuc;
    default:
        return RLType::Unknown;
    }
}

/// @brief Map the C ABI welding process code to WeldProcessType.
/// 0=arc (MIG/MAG), 1=laser, anything else=MIG/MAG.
WeldProcessType ToWeldProcess(int process)
{
    return (process == 1) ? WeldProcessType::Laser : WeldProcessType::MIG_MAG;
}

/// @brief Build a WeldParam from the WAAM package welding settings.
WeldParam BuildWeldParam(const WaamWeldParams& w)
{
    WeldParam wp;
    wp.current = w.current;
    wp.voltage = w.voltage;
    wp.wireFeedSpeed = w.wire_feed_speed;
    wp.gasFlowRate = w.gas_flow_rate;
    wp.travelSpeed = w.travel_speed;
    wp.process = ToWeldProcess(w.process);
    wp.arcEnd = ArcEndType::CraterFill;
    return wp;
}

}  // anonymous namespace

HSBA_SLICER_LIB_API bool SaveWaamRobotPath(const WaamRobotPackage& pkg, const std::string& output_path,
                                           const std::string& lua_script, const std::string& lua_func,
                                           std::string* error_out)
{
    try
    {
        const std::size_t layer_count = pkg.layer_outlines.size();
        if (layer_count == 0)
        {
            if (error_out)
                *error_out = "WAAM package has no layers";
            return false;
        }

        WeldParam base_weld = BuildWeldParam(pkg.weld);
        float travel = (pkg.weld.travel_speed > 0.0f) ? pkg.weld.travel_speed : DEFAULT_ROBOT_VELOCITY;

        WeldRobotPath path(ToRobotType(pkg.robot_type));

        // Spiral/vase mode: merge every layer's outer contour into a single
        // continuous, Z-rising weld bead. The torch does NOT lift, retract or
        // arc-interrupt between layers - one uninterrupted MoveL deposition.
        if (pkg.spiral_mode)
        {
            std::vector<double> zs(pkg.layer_z_heights.begin(), pkg.layer_z_heights.end());
            const std::vector<SpiralPoint> helix = SpiralizeOuterWall(pkg.layer_outlines, zs);
            if (helix.size() >= 2)
            {
                // Rapid (non-welding) move to the base of the helix.
                RLPoint travel_point;
                travel_point.end = {static_cast<float>(helix[0].x), static_cast<float>(helix[0].y),
                                    static_cast<float>(helix[0].z)};
                travel_point.middle = {};
                travel_point.velocity = DEFAULT_ROBOT_VELOCITY;
                travel_point.type = RLPointType::MoveJ;
                travel_point.programIndex = 0;
                path.push_back(travel_point);

                // Deposit the rising helix as one continuous weld line.
                for (size_t i = 1; i < helix.size(); ++i)
                {
                    WeldRLPoint wp;
                    wp.point.end = {static_cast<float>(helix[i].x), static_cast<float>(helix[i].y),
                                    static_cast<float>(helix[i].z)};
                    wp.point.middle = {};
                    wp.point.velocity = travel;
                    wp.point.type = RLPointType::MoveL;
                    wp.point.programIndex = 0;
                    wp.weld = base_weld;
                    wp.isWelding = true;
                    path.push_back(wp);
                }

                const std::filesystem::path out_spiral(output_path);
                if (!lua_script.empty())
                    path.Save(out_spiral, std::filesystem::path(lua_script), std::string_view(lua_func));
                else
                    path.Save(out_spiral);
                return true;
            }
            // helix degenerate (no closed contours) -> fall through to per-layer deposition
        }

        for (std::size_t li = 0; li < layer_count; ++li)
        {
            const float z = (li < pkg.layer_z_heights.size()) ? pkg.layer_z_heights[li] : 0.0f;
            const PolygonsD& contours = pkg.layer_outlines[li];

            for (const PolygonD& contour : contours)
            {
                if (contour.size() < 2)
                    continue;

                // Rapid move to the contour start (non-welding travel point).
                RLPoint travel_point;
                travel_point.end = {static_cast<float>(contour.front().x), static_cast<float>(contour.front().y), z};
                travel_point.middle = {};
                travel_point.velocity = DEFAULT_ROBOT_VELOCITY;
                travel_point.type = RLPointType::MoveJ;
                travel_point.programIndex = 0;
                path.push_back(travel_point);

                // Deposit along the contour: every vertex becomes a weld point.
                for (const Point2D& pt : contour)
                {
                    WeldRLPoint wp;
                    wp.point.end = {static_cast<float>(pt.x), static_cast<float>(pt.y), z};
                    wp.point.middle = {};
                    wp.point.velocity = travel;
                    wp.point.type = RLPointType::MoveL;
                    wp.point.programIndex = 0;
                    wp.weld = base_weld;
                    wp.isWelding = true;
                    path.push_back(wp);
                }
            }
        }

        const std::filesystem::path out(output_path);
        if (!lua_script.empty())
        {
            path.Save(out, std::filesystem::path(lua_script), std::string_view(lua_func));
        }
        else
        {
            path.Save(out);
        }
        return true;
    }
    catch (const RuntimeError& e)
    {
        if (error_out)
            *error_out = e.what();
        return false;
    }
}

}  // namespace HsBa::Slicer
