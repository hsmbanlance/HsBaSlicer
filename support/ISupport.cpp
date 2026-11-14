/** @file ISupport.cpp
 * @brief Default implementations for the ISupport interface.
 * @author HsBa
 */
#include "ISupport.hpp"

#include <algorithm>
#include <functional>
#include <utility>

#include "OverhangDetector.hpp"
#include "base/parallel.hpp"

namespace HsBa::Slicer::Support
{
namespace
{
// Local wrappers to resolve ADL ambiguity between HsBa::Slicer:: and Clipper2Lib:: overloads
PolygonsD UnionD(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fr)
{
    return Clipper2Lib::Union(left, right, fr);
}

PolygonsD DifferenceD(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fr)
{
    return Clipper2Lib::Difference(left, right, fr);
}
}  // namespace

std::vector<PolygonsD> ISupport::GenerateAll(const std::vector<PolygonsD>& layers, const SupportConfig& config)
{
    const int total = static_cast<int>(layers.size());
    if (total <= 0)
    {
        return {};
    }

    // Phase 1 - detection. Layer i is compared with layer i-1 only, so the probe can be
    // spread over the shared pipeline ThreadPool; HSBA_PIPELINE_THREADS=1 restores the
    // original serial iteration for regression comparison. The bottom layer is never
    // probed: it rests on the build plate, and Detect(current, {}) reports the whole
    // footprint as an overhang, which used to bury a support slab inside the first slice.
    const int probed = total - 1;
    std::vector<PolygonsD> overhangs(static_cast<size_t>(std::max(probed, 0)));
    if (probed > 0)
    {
        auto const probe = [&layers, &config](int i) -> PolygonsD
        { return OverhangDetector::Detect(layers[static_cast<size_t>(i + 1)], layers[static_cast<size_t>(i)], config); };
        if (IsLayerIndependent())
        {
            const std::function<PolygonsD(int)> detect_layer = probe;
            overhangs = HsBa::Slicer::Pipeline::ParallelMapLayers<PolygonsD>(probed, detect_layer);
        }
        else
        {
            for (int i = 0; i < probed; ++i)
            {
                overhangs[static_cast<size_t>(i)] = probe(i);
            }
        }
    }

    // Phase 2 - downward column growth. A support cross-section is not an answer on its
    // own: whatever layer i leaves unsupported has to be re-deposited on layer i-1, and so
    // on, until the column reaches the plate or lands on solid model. Carrying the region
    // (not the generated pattern) keeps the column aligned instead of letting it widen by
    // the pattern offset on every layer.
    std::vector<PolygonsD> result(static_cast<size_t>(total));
    PolygonsD pending;  // area at the layer being written that nothing above it carries
    for (int i = total - 1; i >= 1; --i)
    {
        const PolygonsD& overhang = overhangs[static_cast<size_t>(i - 1)];
        if (pending.empty())
        {
            pending = overhang;
        }
        else if (!overhang.empty())
        {
            // Both sides come out of Clipper with consistent orientation, NonZero merges them.
            pending = UnionD(pending, overhang, Clipper2Lib::FillRule::NonZero);
        }
        if (pending.empty())
        {
            continue;
        }

        const PolygonsD& carrier = layers[static_cast<size_t>(i - 1)];
        // EvenOdd keeps holes of the model layer hollow whatever ring orientation the
        // upstream produced; the region itself is a plain donut set, so it is equivalent here.
        pending = carrier.empty() ? pending : DifferenceD(pending, carrier, Clipper2Lib::FillRule::EvenOdd);
        if (pending.empty())
        {
            continue;
        }
        result[static_cast<size_t>(i - 1)] = GenerateFromOverhangs(pending, config.layer_height, config);
    }
    return result;
}
}  // namespace HsBa::Slicer::Support
