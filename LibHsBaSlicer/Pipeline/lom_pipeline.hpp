/** @file lom_pipeline.hpp
 * @brief Pipeline orchestration kernel for the LOM process (Sheet slicing -> Lua export).
 *
 * Single source of truth for the LOM stage sequence, progress percentages, cut/bond parameter
 * JSON, Lua package export, cooperative cancellation and layer-geometry capture. The C ABI
 * (`DllHsBaSlicer/lom_pipeline.cpp`) and the C++20 module both call it.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_LOM_PIPELINE_KERNEL_HPP
#define HSBA_SLICER_LOM_PIPELINE_KERNEL_HPP

#include <string>

#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
#include "LibHsBaSlicer/export.h"
#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::Kernel
{

/**
 * @brief Result of a synchronous LOM run.
 *
 * Never throws for expected pipeline failures; they surface through @p success/@p cancelled/@p
 * error. LOM emits no G-code: @p export_path is the Lua-produced package location.
 */
struct LomOutput
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;      ///< Produced LOM package path (empty on failure)
    std::string error;            ///< Human readable failure/cancellation reason (empty on success)
    double elapsed_seconds = 0.0;
    bool cancelled = false;       ///< True when a cancel request stopped the run
    GeometryCapture geometry;     ///< Populated only when config.capture_geometry was set
};

/**
 * @brief Run the full LOM pipeline synchronously.
 * @param cfg The C-ABI LOM configuration (read-only; its strings must stay valid for the call).
 * @param hooks Progress callback and cooperative cancellation flag (see Kernel::Hooks).
 * @return The native run output.
 */
HSBA_SLICER_LIB_API LomOutput RunLom(const HsBaLomPipelineConfig_t& cfg, const Hooks& hooks);

}  // namespace HsBa::Slicer::Kernel

#endif  // !HSBA_SLICER_LOM_PIPELINE_KERNEL_HPP
