/** @file config_json_pipeline.cpp
 * @brief Implementation of the PipelineConfig <-> JSON C ABI; delegates the codec to the Lib layer.
 * @author HsBa
 */
#include "config_json_pipeline.h"

#include <string>

#include "LibHsBaSlicer/ParamStore/config_json_ops.hpp"
#include "LibHsBaSlicer/ParamStore/param_store_ops.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

// C enum -> Lib enum (explicit switch, mirroring param_store_pipeline.cpp; not relying on integer order).
ParamPipelineKind ToLibKind(HsBaPipelineKind k)
{
    switch (k)
    {
    case HSBA_PIPELINE_SLA:
        return ParamPipelineKind::Sla;
    case HSBA_PIPELINE_SLS:
        return ParamPipelineKind::Sls;
    case HSBA_PIPELINE_SLM:
        return ParamPipelineKind::Slm;
    case HSBA_PIPELINE_LOM:
        return ParamPipelineKind::Lom;
    case HSBA_PIPELINE_TDP:
        return ParamPipelineKind::Tdp;
    case HSBA_PIPELINE_WAAM:
        return ParamPipelineKind::Waam;
    case HSBA_PIPELINE_CUSTOM:
        return ParamPipelineKind::Custom;
    case HSBA_PIPELINE_FILETRANSFER:
        return ParamPipelineKind::FileTransfer;
    case HSBA_PIPELINE_FDM:
    default:
        return ParamPipelineKind::Fdm;
    }
}

bool ValidKind(HsBaPipelineKind k)
{
    const int v = static_cast<int>(k);
    return v >= 0 && v < HSBA_PIPELINE_KIND_COUNT;
}

// Per-thread scratch buffer backing a returned const char*; the library keeps ownership so the
// caller never frees. Each entry point uses its own buffer, so the two string results do not
// clobber each other, only repeat calls of the same entry point invalidate the previous pointer.
const char* ToScratch(std::string& scratch, std::string value)
{
    scratch = std::move(value);
    return scratch.c_str();
}

// Converge any Lib-layer exception into an empty result so nothing crosses the extern "C" boundary.
template <typename F>
const char* Guarded(std::string& scratch, F&& body)
{
    try
    {
        return ToScratch(scratch, body());
    }
    catch (const std::exception&)
    {
        return ToScratch(scratch, std::string());
    }
    catch (...)
    {
        return ToScratch(scratch, std::string());
    }
}

}  // namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API const char* HsBaGetDefaultConfigJson(HsBaPipelineKind kind)
{
    thread_local std::string scratch;
    if (!HsBa::Slicer::Pipeline::ValidKind(kind))
    {
        return HsBa::Slicer::Pipeline::ToScratch(scratch, std::string());
    }
    return HsBa::Slicer::Pipeline::Guarded(
        scratch, [&] { return HsBa::Slicer::DefaultConfigJson(HsBa::Slicer::Pipeline::ToLibKind(kind)); });
}

HSBA_SLICER_API const char* HsBaConfigToJson(HsBaPipelineKind kind, const void* cfg)
{
    thread_local std::string scratch;
    if (!HsBa::Slicer::Pipeline::ValidKind(kind) || !cfg)
    {
        return HsBa::Slicer::Pipeline::ToScratch(scratch, std::string());
    }
    return HsBa::Slicer::Pipeline::Guarded(
        scratch, [&] { return HsBa::Slicer::ConfigToJson(HsBa::Slicer::Pipeline::ToLibKind(kind), cfg); });
}

HSBA_SLICER_API int HsBaConfigFromJson(HsBaPipelineKind kind, const char* json, void* cfg)
{
    if (!HsBa::Slicer::Pipeline::ValidKind(kind) || !json || !cfg)
    {
        return 0;
    }
    return HsBa::Slicer::ConfigFromJson(HsBa::Slicer::Pipeline::ToLibKind(kind), json, cfg) ? 1 : 0;
}
