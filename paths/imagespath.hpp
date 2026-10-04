/** @file imagespath.hpp
 * @brief Image-based path output (ImagesPath): a config sidecar plus named image payloads, saved via Lua or natively.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_IMAGES_PATH_HPP
#define HSBA_SLICER_IMAGES_PATH_HPP

#include <functional>
#include <unordered_map>

#include "IPath.hpp"

namespace HsBa::Slicer
{
/**
 * @class ImagesPath
 * @brief An IPath implementation packaging a config file plus a set of named image strings.
 *
 * Used by image-oriented outputs (e.g. SLA/SLS layer images): images are added by path/name
 * and saved either directly or through an optional Lua post-processing script (see IPath).
 */
class ImagesPath : public IPath
{
public:
    /**
     * @brief Construct an images path.
     * @param config_file Config file name/path carried alongside the images.
     * @param config_str Config content string.
     * @param callback Optional progress callbacks invoked with (percent, stage).
     */
    ImagesPath(std::string_view config_file, std::string_view config_str,
               const std::vector<std::function<void(double, std::string_view)>>& callback = {});
    virtual ~ImagesPath() = default;  ///< Virtual destructor.
    virtual void Save(const std::filesystem::path&) const override;
    virtual void Save(const std::filesystem::path&, std::string_view script,
                      const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString() const override;
    virtual std::string ToString(std::string_view script,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString(const std::string_view script, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
                      const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file,
                      std::string_view funcName, const std::function<void(lua_State*)>& lua_reg = {}) const override;
    /// Add (or replace) an image payload under the given path/name.
    void AddImage(std::string_view path, std::string_view image_str);

private:
    struct ConfigFile
    {
        std::string path;
        std::string configStr;
    };
    ConfigFile config_;
    std::unordered_map<std::string, std::string> images_;
    std::vector<std::function<void(double, std::string_view)>> callbacks_;
};
}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_IMAGES_PATH_HPP