/** @file robotpath.hpp
 * @brief Industrial-robot path output (RobotPath) and the robot point/type enums used to emit brand robot code.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_ROBOT_PATH_HPP
#define HSBA_SLICER_ROBOT_PATH_HPP

#include <vector>

#include "IPath.hpp"

namespace HsBa::Slicer
{
/// Robot path constants
constexpr int ROBOT_UNDEFINED_TYPE = 255;         ///< Sentinel value for an undefined robot type.
constexpr float DEFAULT_ROBOT_VELOCITY = 100.0f;  ///< Default velocity for robot movements.

/// Robot motion/program command kinds.
enum class RLPointType
{
    MoveJ,          ///< Joint (PTP) move.
    MoveL,          ///< Linear move.
    MoveC,          ///< Circular move.
    ProgramLStart,  ///< Start of a linear program block.
    ProgramStart,   ///< Start of a program block.
    ProgramCStart,  ///< Start of a circular program block.
    ProgramL,       ///< Linear program move.
    ProgramC,       ///< Circular program move.
    ProgramLEnd,    ///< End of a linear program block.
    ProgramCEnd,    ///< End of a circular program block.
};

/// Supported robot language brands.
enum class RLType
{
    Unknown = -1,                   ///< Unspecified/unknown brand.
    Abb,                            ///< ABB RobotStudio code.
    Kuka,                           ///< KUKA KRL code.
    Fanuc,                          ///< FANUC TP code.
    Undefine = ROBOT_UNDEFINED_TYPE, ///< Explicit undefined marker.
};

/// A single robot path point with its end/middle position, velocity and command kind.
struct RLPoint
{
    OutPoints3 end;                              ///< End position of the move.
    OutPoints3 middle;                           ///< Middle/via position (for circular moves).
    float velocity = DEFAULT_ROBOT_VELOCITY;     ///< Move velocity.
    RLPointType type = RLPointType::MoveL;       ///< Motion/program command kind.
    size_t programIndex = 0;                     ///< Index used for program-block pairing.
};

/**
 * @class RobotPath
 * @brief An IPath implementation producing brand-specific robot language programs (ABB/KUKA/FANUC).
 *
 * Accumulates RLPoint moves and, on Save/ToString, emits a program for the target robot brand,
 * optionally post-processed by a Lua script (see IPath).
 */
class RobotPath : public IPath
{
public:
    /**
     * @brief Construct a robot path.
     * @param robotType Target robot language brand.
     * @param startPoint Initial robot position.
     * @param startProgramFunc Program preamble emitted at the start.
     * @param endProgramFunc Program epilogue emitted at the end.
     */
    RobotPath(RLType robotType = RLType::Unknown, OutPoints3 startPoint = {}, std::string startProgramFunc = "",
              std::string endProgramFunc = "");
    /// Append a robot point to the path.
    void push_back(const RLPoint& point);
    virtual ~RobotPath() = default;  ///< Virtual destructor.
    /// Get the target robot language brand.
    RLType getRobotType() const;
    virtual void Save(const std::filesystem::path&) const override;
    virtual void Save(const std::filesystem::path&, std::string_view script,
                      const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString() const override;
    virtual std::string ToString(std::string_view script,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
                      const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file,
                      std::string_view funcName, const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString(const std::string_view script, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    /// Access the i-th robot point.
    inline virtual RLPoint operator[](size_t i) { return points_[i]; }

private:
    RLType robotType_;
    OutPoints3 startPoint_;
    std::vector<RLPoint> points_;
    std::string startProgramFunc_;
    std::string endProgramFunc_;
    std::string GenerateAbbCode() const;
    std::string GenerateKukaCode() const;
    std::string GenerateFanucCode() const;
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_ROBOT_PATH_HPP
