/** @file kernel_types.hpp
 * @brief Shared C++ types for the pipeline orchestration kernel.
 *
 * The kernel carries the full pipeline stage logic that previously lived inside the
 * DllHsBaSlicer `*_pipeline.cpp` coroutines, so that the C ABI (Dll) and the C++20
 * module (ModuleHsBaSlicer) drive one shared implementation instead of two divergent
 * ones. Every kernel entry takes the C-ABI config struct (the common currency both
 * callers already hold) plus a set of native C++ hooks, and returns a native output
 * struct; the callers own the boundary conversion (C-ABI marshalling for the Dll,
 * direct return for the module).
 *
 * The kernel is deliberately synchronous: `ParallelForLayers` in base/parallel.hpp is
 * byte-identical to the coroutine wrapper `ParallelForLayersAsync` when driven blocking,
 * which is exactly how both the Dll synchronous entry (`task.get_result()`) and the
 * module's `run()` consume it.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_KERNEL_TYPES_HPP
#define HSBA_SLICER_KERNEL_TYPES_HPP

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "2D/FloatPolygons.hpp"

namespace HsBa::Slicer::Kernel
{

/**
 * @brief Per-layer geometry a run produced, captured only when the config asked for it.
 *
 * Mirrors the Dll geometry snapshot content, but as native C++ vectors so the module can
 * hand it back directly while the Dll still funnels it into its id-based snapshot pool.
 */
struct GeometryCapture
{
    bool valid = false;                 ///< False when capture_geometry was off or nothing was produced
    std::vector<PolygonsD> outlines;    ///< Part contours per layer
    std::vector<PolygonsD> fills;       ///< FDM infill / SLA floor rings per layer
    std::vector<PolygonsD> supports;    ///< Support cross sections per layer
    std::vector<double> zs;             ///< Layer platform heights (mm)
    std::vector<double> path_xyz;       ///< Flattened x,y,z triples of a continuous (spiral) path (mm); empty = none
};

/**
 * @brief Native callbacks the kernel invokes during a run, replacing the C-ABI callbacks.
 *
 * The Dll wraps its `HsBaProgressCallback`/`void*` into @p progress and forwards the
 * job-owned `std::atomic<bool>*` into @p cancel_flag; the module supplies its own
 * `std::function` and its own cancel flag from a Job.
 */
struct Hooks
{
    /// @brief Progress report (percent 0-100, human readable stage text); may be null.
    std::function<void(int, std::string_view)> progress;
    /// @brief Cooperative cancellation flag polled at stage boundaries and inside layer loops; null = not cancellable.
    const std::atomic<bool>* cancel_flag = nullptr;
};

}  // namespace HsBa::Slicer::Kernel

#endif  // !HSBA_SLICER_KERNEL_TYPES_HPP
