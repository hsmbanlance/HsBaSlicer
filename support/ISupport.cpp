#include "ISupport.hpp"

namespace HsBa::Slicer::Support
{
std::vector<PolygonsD> ISupport::GenerateAll(const std::vector<PolygonsD>& layers, const SupportConfig& config)
{
    std::vector<PolygonsD> result;
    result.reserve(layers.size());

    for (std::size_t i = 0; i < layers.size(); ++i)
    {
        const PolygonsD& prev = (i == 0) ? PolygonsD{} : layers[i - 1];
        result.push_back(Generate(layers[i], prev, config.layer_height, config));
    }

    return result;
}
}  // namespace HsBa::Slicer::Support
