#pragma once
#ifndef HSBA_SLICER_PIPELINE_CONFIG2MSG_HPP
#define HSBA_SLICER_PIPELINE_CONFIG2MSG_HPP

#include "pipelinetypes/pipeline_types.h"

#include "fdm_pipeline.pb.h"
#include "sla_pipeline.pb.h"
#include "sls_pipeline.pb.h"

namespace HsBa::Slicer
{

/// @brief Convert FDM pipeline C config to proto message.
void FdmConfigToMsg(const HsBaFdmPipelineConfig_t& config, HsbaProto::msg_fdm_pipeline_config* msg);

/// @brief Convert FDM pipeline C result to proto message.
void FdmResultToMsg(const HsBaFdmPipelineResult_t& result, HsbaProto::msg_fdm_pipe_result* msg);

/// @brief Convert SLA pipeline C config to proto message.
void SlaConfigToMsg(const HsBaSlaPipelineConfig_t& config, HsbaProto::sla_pipe_config* msg);

/// @brief Convert SLA pipeline C result to proto message.
void SlaResultToMsg(const HsBaSlaPipelineResult_t& result, HsbaProto::sla_pipe_result* msg);

/// @brief Convert SLS pipeline C config to proto message.
void SlsConfigToMsg(const HsBaSlsPipelineConfig_t& config, HsbaProto::sls_pipe_config* msg);

/// @brief Convert SLS pipeline C result to proto message.
void SlsResultToMsg(const HsBaSlsPipelineResult_t& result, HsbaProto::sls_pipe_result* msg);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PIPELINE_CONFIG2MSG_HPP
