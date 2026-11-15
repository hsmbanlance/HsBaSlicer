/** @file waam_pipeline.hpp
 * @brief Pipeline orchestration kernel for the WAAM process (Bead slicing -> Robot path export).
 *
 * Single source of truth for the WAAM stage sequence, progress percentages, seam staggering,
 * spiral weld-bead derivation, robot path export (built-in or Lua), cooperative cancellation and
 * layer-geometry capture. The C ABI (`DllHsBaSlicer/waam_pipeline.cpp`) and the C++20 module both
 * call it.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_WAAM_PIPELINE_KERNEL_HPP
#define HSBA_SLICER_WAAM_PIPELINE_KERNEL_HPP

#include <string>

#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
#include "LibHsBaSlicer/export.h"
#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::Kernel
{

/**
 * @brief Result of a synchronous WAAM run.
 *
 * Never throws for expected pipeline failures; they surface through @p success/@p cancelled/@p
 * error. WAAM emits a robot program: @p output_path is the produced file location.
 */
struct WaamOutput
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;      ///< Produced WAAM robot path (empty on failure)
    std::string error;            ///< Human readable failure/cancellation reason (empty on success)
    double elapsed_seconds = 0.0;
    bool cancelled = false;       ///< True when a cancel request stopped the run
    GeometryCapture geometry;     ///< Populated only when config.capture_geometry was set
};

/**
 * @brief Run the full WAAM pipeline synchronously.
 * @param cfg The C-ABI WAAM configuration (read-only; its strings must stay valid for the call).
 * @param hooks Progress callback and cooperative cancellation flag (see Kernel::Hooks).
 * @return The native run output.
 */
HSBA_SLICER_LIB_API WaamOutput RunWaam(const HsBaWaamPipelineConfig_t& cfg, const Hooks& hooks);

}  // namespace HsBa::Slicer::Kernel

#endif  // !HSBA_SLICER_WAAM_PIPELINE_KERNEL_HPP
