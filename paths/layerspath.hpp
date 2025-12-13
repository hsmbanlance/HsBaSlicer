#pragma once
#ifndef HSBA_SLICER_LAYERS_PATH_HPP
#define HSBA_SLICER_LAYERS_PATH_HPP

#include <vector>
#include <functional>

#include "IPath.hpp"
#include "2D/FloatPolygons.hpp"

namespace HsBa::Slicer
{
    class LayersPath : public IPath
    {
    public:
        LayersPath(const std::function<void(std::string_view, std::string_view)>& callback = [](std::string_view, std::string_view){});
        virtual ~LayersPath() = default;
        virtual void Save(const std::filesystem::path& path) const override;
        virtual void Save(const std::filesystem::path& path, std::string_view script) const override;
        virtual std::string ToString() const override;
        virtual std::string ToString(const std::string_view script) const override;
        virtual std::string ToString(const std::filesystem::path& script_file, const std::string_view funcName) const override;
        virtual std::string ToString(const std::string_view script, const std::string_view funcName) const override;
        virtual void Save(const std::filesystem::path& path, std::string_view script, std::string_view funcName) const override;
        virtual void Save(const std::filesystem::path& path, const std::filesystem::path& script_file, std::string_view funcName) const override;
        void push_back(const std::string& layerConfig, const PolygonsD& layer);
    private:
        struct LayersData
        {
            std::string layerConfig;
            PolygonsD layer;
        };
        std::function<void(std::string_view, std::string_view)> callback_;
        std::vector<LayersData> layers_;
    };

} // namepace HsBa::Slicer

#endif // !HSBA_SLICER_LAYERS_PATH_HPP