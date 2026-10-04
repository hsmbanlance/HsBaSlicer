/** @file IPath.hpp
 * @brief Abstract output-path interface for slicer path products (points, layers, images, robot, G-code).
 * @author HsBa
 */
#pragma once

#ifndef HSBA_SLICER_IPATH_HPP
#define HSBA_SLICER_IPATH_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

// forward-declare lua state to avoid including lua.hpp in this core header
struct lua_State;

namespace HsBa::Slicer
{
/**
 * @class IPath
 * @brief Abstract interface for a slice/path output product that can be saved to a file or serialized to a string.
 *
 * Concrete implementations (PointsPath, LayersPath, ImagesPath, RobotPath, WeldRobotPath,
 * GCodePath) produce format-specific output. Each Save/ToString family supports an optional
 * Lua post-processing step: an inline script or script file plus a function name, and a
 * registration callback that can expose additional Lua functions to that script.
 */
class IPath
{
public:
    virtual ~IPath() = default;  ///< Virtual destructor.

    /// Save the path to a file in the implementation's native format.
    virtual void Save(const std::filesystem::path&) const = 0;
    /// Save the path using an inline Lua script to post-process the output.
    virtual void Save(const std::filesystem::path&, std::string_view script,
                      const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
    /// Save the path using an inline Lua script and a named entry function.
    virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
                      const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
    /// Save the path using a Lua script file and a named entry function.
    virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file,
                      std::string_view funcName, const std::function<void(lua_State*)>& lua_reg = {}) const = 0;

    /// Serialize the path to a string in the implementation's native format.
    virtual std::string ToString() const = 0;
    /// Serialize the path to a string, post-processed by an inline Lua script.
    virtual std::string ToString(const std::string_view script,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
    /// Serialize the path to a string via an inline Lua script and a named entry function.
    virtual std::string ToString(const std::string_view script, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
    /// Serialize the path to a string via a Lua script file and a named entry function.
    virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const = 0;
};

/// A 3D output point (planar X/Y plus absolute build height Z) used across path types.
struct OutPoints3
{
    float x = 0.0f;  ///< X coordinate.
    float y = 0.0f;  ///< Y coordinate.
    float z = 0.0f;  ///< Z coordinate (build height).
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_IPATH_HPP