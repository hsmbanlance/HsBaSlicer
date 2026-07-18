#pragma once
#ifndef HSBA_SLICER_MSG2PIPELINE_CONFIG_HPP
#define HSBA_SLICER_MSG2PIPELINE_CONFIG_HPP

#include "DllHsBaSlicer/fdm_pipeline.h"
#include "DllHsBaSlicer/sla_pipeline.h"

#include "fdm_pipeline.pb.h"
#include "sla_pipeline.pb.h"

namespace HsBa::Slicer
{

/// @brief Convert proto message to FDM pipeline C config.
/// @note String fields are allocated with malloc; caller must free the struct
///       or pass it through HsBaRunFdmPipeline which copies internally.
void MsgToFdmConfig(const HsbaProto::msg_fdm_pipeline_config& msg, HsBaFdmPipelineConfig_t* config);

/// @brief Convert proto message to FDM pipeline C result.
/// @note String fields (gcode_content, error_message) are allocated with malloc;
///       caller must call HsBaFreePipelineResult to release.
void MsgToFdmResult(const HsbaProto::msg_fdm_pipe_result& msg, HsBaFdmPipelineResult_t* result);

/// @brief Convert proto message to SLA pipeline C config.
/// @note String fields are allocated with malloc; caller must free the struct
///       or pass it through HsBaRunSlaPipeline which copies internally.
void MsgToSlaConfig(const HsbaProto::sla_pipe_config& msg, HsBaSlaPipelineConfig_t* config);

/// @brief Convert proto message to SLA pipeline C result.
/// @note String fields (export_path, error_message) are allocated with malloc;
///       caller must call HsBaFreeSlaPipelineResult to release.
void MsgToSlaResult(const HsbaProto::sla_pipe_result& msg, HsBaSlaPipelineResult_t* result);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_MSG2PIPELINE_CONFIG_HPP
