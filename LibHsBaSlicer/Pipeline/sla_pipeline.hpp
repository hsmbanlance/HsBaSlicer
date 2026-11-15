/** @file sla_pipeline.hpp
 * @brief Pipeline orchestration kernel for the SLA process (Slice -> Floor -> Support -> Export).
 *
 * This is the single source of truth for the SLA stage sequence, progress percentages, floor/raft
 * generation, support build (built-in or Lua), SlaPackage export, cooperative cancellation and
 * layer-geometry capture. The C ABI (`DllHsBaSlicer/sla_pipeline.cpp`) and the C++20 module
 * (`ModuleHsBaSlicer`) both call it, so their behavior stays identical.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_SLA_PIPELINE_KERNEL_HPP
#define HSBA_SLICER_SLA_PIPELINE_KERNEL_HPP

#include <string>

#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
#include "LibHsBaSlicer/export.h"
#include "pipelinetypes/pipeline_types.h"

namespace HsBa::Slicer::Kernel
{

/**
 * @brief Result of a synchronous SLA run.
 *
 * The function never throws for expected pipeline failures: they surface through
 * @p success/@p cancelled/@p error. The Dll converts this into `HsBaSlaPipelineResult_t`
 * (malloc'ing the strings and storing @p geometry into its snapshot pool); the module
 * maps it onto `SlaResult` and exposes @p geometry directly. Unlike FDM, SLA emits no
 * G-code text: @p export_path is the produced package location.
 */
struct SlaOutput
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;      ///< Produced SLA package path (empty on failure)
    std::string error;            ///< Human readable failure/cancellation reason (empty on success)
    double elapsed_seconds = 0.0;
    bool cancelled = false;       ///< True when a cancel request stopped the run
    GeometryCapture geometry;     ///< Populated only when config.capture_geometry was set
};

/**
 * @brief Run the full SLA pipeline synchronously.
 * @param cfg The C-ABI SLA configuration (read-only; its strings must stay valid for the call).
 * @param hooks Progress callback and cooperative cancellation flag (see Kernel::Hooks).
 * @return The native run output.
 */
HSBA_SLICER_LIB_API SlaOutput RunSla(const HsBaSlaPipelineConfig_t& cfg, const Hooks& hooks);

}  // namespace HsBa::Slicer::Kernel

#endif  // !HSBA_SLICER_SLA_PIPELINE_KERNEL_HPP
