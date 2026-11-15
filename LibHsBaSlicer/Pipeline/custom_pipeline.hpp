/** @file custom_pipeline.hpp
 * @brief Pipeline orchestration kernel for the fully Lua-driven custom pipeline.
 *
 * Single source of truth for the custom pipeline entry: build a LuaPipelineContext from the
 * C-ABI config, honour cooperative cancellation around the opaque Lua call, and surface the
 * Lua output. The C ABI (`DllHsBaSlicer/custom_pipeline.cpp`) and the C++20 module both call it.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_CUSTOM_PIPELINE_KERNEL_HPP
#define HSBA_SLICER_CUSTOM_PIPELINE_KERNEL_HPP

#include <string>

#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
#include "LibHsBaSlicer/export.h"
#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::Kernel
{

/**
 * @brief Result of a synchronous custom-pipeline run.
 *
 * Never throws for expected failures; they surface through @p success/@p cancelled/@p error. A
 * Lua pipeline produces no layer-geometry snapshot, so this output carries none.
 */
struct CustomOutput
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;      ///< Value set by the script via `HsBa.setOutputPath()`
    std::string result_string;    ///< String returned by the Lua entry function
    std::string error;            ///< Human readable failure/cancellation reason (empty on success)
    double elapsed_seconds = 0.0;
    bool cancelled = false;       ///< True when a cancel request stopped the run
};

/**
 * @brief Run the full custom pipeline synchronously.
 * @param cfg The C-ABI custom configuration (read-only; its strings must stay valid for the call).
 * @param hooks Progress callback and cooperative cancellation flag (see Kernel::Hooks).
 * @return The native run output.
 */
HSBA_SLICER_LIB_API CustomOutput RunCustom(const HsBaCustomPipelineConfig_t& cfg, const Hooks& hooks);

}  // namespace HsBa::Slicer::Kernel

#endif  // !HSBA_SLICER_CUSTOM_PIPELINE_KERNEL_HPP
