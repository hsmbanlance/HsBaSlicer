#pragma once
#ifndef HSBA_SLICER_WELD_ROBOT_PATH_HPP
#define HSBA_SLICER_WELD_ROBOT_PATH_HPP

#include <string>
#include <vector>

#include "robotpath.hpp"

namespace HsBa::Slicer
{
/// Welding process type
enum class WeldProcessType
{
    Unknown = -1,
    MIG_MAG,  ///< Gas Metal Arc Welding (GMAW)
    TIG,      ///< Gas Tungsten Arc Welding (GTAW)
    Plasma,   ///< Plasma Arc Welding (PAW)
    Laser,    ///< Laser Beam Welding (LBW)
};

/// Arc end (crater) handling method
enum class ArcEndType
{
    Normal,       ///< Simple arc extinction
    CraterFill,   ///< Fill crater before stopping
    SlowRetract,  ///< Slowly retract to avoid crater
};

/// Welding parameters for a single weld segment
struct WeldParam
{
    float current = 200.0f;            ///< Welding current [A]
    float voltage = 22.0f;             ///< Arc voltage [V]
    float wireFeedSpeed = 10.0f;       ///< Wire feed speed [m/min]
    float gasFlowRate = 15.0f;         ///< Shielding gas flow rate [L/min]
    float travelSpeed = 5.0f;          ///< Torch travel speed [mm/s]
    float arcStartCurrent = 150.0f;    ///< Arc start current [A] (hot start)
    float arcStartVoltage = 20.0f;     ///< Arc start voltage [V]
    float craterFillCurrent = 120.0f;  ///< Crater fill current [A]
    float craterFillVoltage = 18.0f;   ///< Crater fill voltage [V]
    WeldProcessType process = WeldProcessType::MIG_MAG;
    ArcEndType arcEnd = ArcEndType::Normal;
    int weldSchedule = 1;  ///< Weld schedule number (FANUC style)
};

/// Robot point with associated welding parameters
struct WeldRLPoint
{
    RLPoint point;           ///< Base robot path point
    WeldParam weld;          ///< Welding parameters for this point
    bool isWelding = false;  ///< Whether this point is part of a weld seam
};

/**
 * @brief Robot path with welding parameters for common robot language generation.
 *
 * Generates brand-specific robot code (ABB, KUKA, FANUC) that includes
 * arc start/end commands and welding parameter instructions.
 */
class WeldRobotPath : public RobotPath
{
public:
    WeldRobotPath(RLType robotType = RLType::Unknown, OutPoints3 startPoint = {}, std::string startProgramFunc = "",
                  std::string endProgramFunc = "");
    ~WeldRobotPath() override = default;

    /// Add a weld point with welding parameters
    void push_back(const WeldRLPoint& weldPoint);
    /// Add a non-weld movement point (inherited behavior)
    using RobotPath::push_back;

    /// Get all weld points
    const std::vector<WeldRLPoint>& weldPoints() const;

    void Save(const std::filesystem::path&) const override;
    std::string ToString() const override;
    std::string ToString(std::string_view script, const std::function<void(lua_State*)>& lua_reg = {}) const override;

private:
    std::vector<WeldRLPoint> weldPoints_;

    std::string GenerateWeldAbbCode() const;
    std::string GenerateWeldKukaCode() const;
    std::string GenerateWeldFanucCode() const;

    /// Push weld parameters into Lua table at stack top
    static void PushWeldParamToLua(lua_State* L, const WeldParam& wp);
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_WELD_ROBOT_PATH_HPP
