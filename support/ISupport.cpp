/** @file ISupport.cpp
 * @brief Default implementations for the ISupport interface.
 * @author HsBa
 */
#include "ISupport.hpp"

#include <functional>
#include <utility>

#include "base/parallel.hpp"

namespace HsBa::Slicer::Support
{
std::vector<PolygonsD> ISupport::GenerateAll(const std::vector<PolygonsD>& layers, const SupportConfig& config)
{
    const int total = static_cast<int>(layers.size());
    if (total <= 0)
    {
        return {};
    }

    // Per-layer Generate reads only layers[i] and layers[i-1] (input, not prior
    // results), so when the concrete generator declares itself independent the
    // base loop can be dispatched across the shared pipeline ThreadPool without
    // changing observable output. HSBA_PIPELINE_THREADS=1 restores the original
    // serial iteration for regression comparison.
    if (IsLayerIndependent())
    {
        return HsBa::Slicer::Pipeline::ParallelMapLayers<PolygonsD>(
            total,
            [this, &layers, &config](int i) -> PolygonsD
            {
                const PolygonsD& prev = (i == 0) ? PolygonsD{} : layers[static_cast<size_t>(i - 1)];
                return Generate(layers[static_cast<size_t>(i)], prev, config.layer_height, config);
            });
    }

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
