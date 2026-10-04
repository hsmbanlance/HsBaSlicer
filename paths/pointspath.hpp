/** @file pointspath.hpp
 * @brief Point-based G-code path (PointsPath) and the G-code point/motion types it uses.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_POINTS_PATH_HPP
#define HSBA_SLICER_POINTS_PATH_HPP

#include <optional>
#include <vector>

#include "IPath.hpp"

namespace HsBa::Slicer
{
/// G-code command constants
constexpr int GCODE_G17_VALUE = 17;         ///< Select the XY plane.
constexpr int GCODE_G90_VALUE = 90;         ///< Absolute positioning mode.
constexpr float DEFAULT_VELOCITY = 100.0f;  ///< Default velocity for a GPoint.

/// Supported G-code motion/setting codes.
enum class GcodeType
{
    G0,  ///< Rapid move.
    G1,  ///< Linear move.
    G2,  ///< Clockwise arc.
    G3,  ///< Counter-clockwise arc.
    G17 = GCODE_G17_VALUE,  ///< Select XY plane.
    G18,                    ///< Select XZ plane.
    G19,                    ///< Select YZ plane.
    G20,                    ///< Units: inches.
    G21,                    ///< Units: millimeters.
    G90 = GCODE_G90_VALUE,  ///< Absolute positioning.
    G91                     ///< Relative positioning.
};

/// Length units used when emitting G-code.
enum class GCodeUnits
{
    Inch,  ///< Inches.
    mm     ///< Millimeters.
};

/// A single G-code motion command with its target/arc point, velocity and extrusion.
struct GPoint
{
    GcodeType type = GcodeType::G1;  ///< Motion/setting code.
    OutPoints3 p1;                   ///< Target point of the move.
    OutPoints3 center;               ///< Transition/mid point of an arc motion.
    float velocity = DEFAULT_VELOCITY;  ///< Feed velocity for the move.
    double extrusion = 0.0;             ///< Extrusion amount (filament length) for the move.
};

/**
 * @class PointsPath
 * @brief An IPath implementation built from an ordered sequence of G-code points.
 *
 * Accumulates GPoint moves and serializes them to G-code text, either natively or
 * through an optional Lua post-processing script (see IPath).
 */
class PointsPath : public IPath
{
public:
    /**
     * @brief Construct a point path.
     * @param units Length units used for output.
     * @param p Initial start point of the path.
     */
    PointsPath(GCodeUnits units = GCodeUnits::mm, OutPoints3 p = {0.0, 0.0, 0.0});
    /// Append a G-code point to the path.
    void push_back(const GPoint& point);
    virtual ~PointsPath() = default;  ///< Virtual destructor.
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
    /// Access the i-th G-code point.
    inline virtual GPoint operator[](size_t i) { return points_[i]; }

private:
    std::vector<GPoint> points_;
    OutPoints3 startPoint_;
    GCodeUnits units_ = GCodeUnits::mm;
};

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_POINTS_PATH_HPP
