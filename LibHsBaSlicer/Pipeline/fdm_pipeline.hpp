/** @file fdm_pipeline.hpp
 * @brief Pipeline orchestration kernel for the FDM process (Slice -> Support -> Fill -> Path).
 *
 * This is the single source of truth for the FDM stage sequence, progress percentages,
 * spiral/vase handling, top/bottom solid layering, density-driven spacing, seam staggering,
 * model-preview thumbnails, cooperative cancellation and layer-geometry capture. The C ABI
 * (`DllHsBaSlicer/fdm_pipeline.cpp`) and the C++20 module (`ModuleHsBaSlicer`) both call it,
 * so their behavior stays identical.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_FDM_PIPELINE_KERNEL_HPP
#define HSBA_SLICER_FDM_PIPELINE_KERNEL_HPP

#include <string>

#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
#include "LibHsBaSlicer/export.h"
#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::Kernel
{

/**
 * @brief Result of a synchronous FDM run.
 *
 * The function never throws for expected pipeline failures: they surface through
 * @p success/@p cancelled/@p error. The Dll converts this into `HsBaFdmPipelineResult_t`
 * (malloc'ing the strings and storing @p geometry into its snapshot pool); the module
 * maps it onto `FdmResult` and exposes @p geometry directly.
 */
struct FdmOutput
{
    bool success = false;
    int total_layers = 0;
    std::string gcode;            ///< Generated G-code text (empty on failure)
    std::string error;            ///< Human readable failure/cancellation reason (empty on success)
    double elapsed_seconds = 0.0;
    bool cancelled = false;       ///< True when a cancel request stopped the run
    GeometryCapture geometry;     ///< Populated only when config.capture_geometry was set
};

/**
 * @brief Run the full FDM pipeline synchronously.
 * @param cfg The C-ABI FDM configuration (read-only; its strings must stay valid for the call).
 * @param hooks Progress callback and cooperative cancellation flag (see Kernel::Hooks).
 * @return The native run output.
 */
HSBA_SLICER_LIB_API FdmOutput RunFdm(const HsBaFdmPipelineConfig_t& cfg, const Hooks& hooks);

}  // namespace HsBa::Slicer::Kernel

#endif  // !HSBA_SLICER_FDM_PIPELINE_KERNEL_HPP
