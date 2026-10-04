/** @file layerspath.hpp
 * @brief Layer-based path output (LayersPath): per-layer polygons plus config, saved natively or via Lua.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LAYERS_PATH_HPP
#define HSBA_SLICER_LAYERS_PATH_HPP

#include <functional>
#include <vector>

#include "2D/FloatPolygons.hpp"
#include "IPath.hpp"

namespace HsBa::Slicer
{
/**
 * @class LayersPath
 * @brief An IPath implementation holding an ordered set of per-layer polygon groups with config.
 *
 * Each layer pairs a free-form config string with its double-precision polygons and can be
 * serialized natively or post-processed by an optional Lua script (see IPath). A progress/status
 * callback can be supplied to report per-layer work.
 */
class LayersPath : public IPath
{
public:
    /**
     * @brief Construct a layer path.
     * @param callback Optional status callback invoked with (key, value) during output.
     */
    LayersPath(const std::function<void(std::string_view, std::string_view)>& callback = [](std::string_view,
                                                                                            std::string_view) {});
    virtual ~LayersPath() = default;  ///< Virtual destructor.
    virtual void Save(const std::filesystem::path& path) const override;
    virtual void Save(const std::filesystem::path& path, std::string_view script,
                      const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString() const override;
    virtual std::string ToString(const std::string_view script,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual std::string ToString(const std::string_view script, const std::string_view funcName,
                                 const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName,
                      const std::function<void(lua_State*)>& lua_reg = {}) const override;
    virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file,
                      std::string_view funcName, const std::function<void(lua_State*)>& lua_reg = {}) const override;
    /// Append a layer as a config string plus its polygon group.
    void push_back(const std::string& layerConfig, const PolygonsD& layer);

protected:
    struct LayersData
    {
        std::string layerConfig;
        PolygonsD layer;
    };
    std::function<void(std::string_view, std::string_view)> callback_;
    std::vector<LayersData> layers_;
};

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LAYERS_PATH_HPP