/** @file PipelineConfig2Msg.hpp
 * @brief Converters from native pipeline configuration structs to HsbaProto pipeline-config messages.
 * @author HsBa
 */
#pragma once
#ifndef HSBA_SLICER_PIPELINE_CONFIG2MSG_HPP
#define HSBA_SLICER_PIPELINE_CONFIG2MSG_HPP

#include "pipelinetypes/pipeline_types.h"

#include "custom_pipeline.pb.h"
#include "fdm_pipeline.pb.h"
#include "file_transfer_pipeline.pb.h"
#include "lom_pipeline.pb.h"
#include "sla_pipeline.pb.h"
#include "slm_pipeline.pb.h"
#include "sls_pipeline.pb.h"
#include "tdp_pipeline.pb.h"
#include "waam_pipeline.pb.h"

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

/// @brief Convert file transfer pipeline C config to proto message.
void FileTransferConfigToMsg(const HsBaFileTransferPipelineConfig_t& config, HsbaProto::file_transfer_pipe_config* msg);

/// @brief Convert file transfer pipeline C result to proto message.
void FileTransferResultToMsg(const HsBaFileTransferPipelineResult_t& result, HsbaProto::file_transfer_pipe_result* msg);

/// @brief Convert custom Lua pipeline C config to proto message.
void CustomConfigToMsg(const HsBaCustomPipelineConfig_t& config, HsbaProto::custom_pipe_config* msg);

/// @brief Convert custom Lua pipeline C result to proto message.
void CustomResultToMsg(const HsBaCustomPipelineResult_t& result, HsbaProto::custom_pipe_result* msg);

/// @brief Convert SLM pipeline C config to proto message.
void SlmConfigToMsg(const HsBaSlmPipelineConfig_t& config, HsbaProto::slm_pipe_config* msg);

/// @brief Convert SLM pipeline C result to proto message.
void SlmResultToMsg(const HsBaSlmPipelineResult_t& result, HsbaProto::slm_pipe_result* msg);

/// @brief Convert LOM pipeline C config to proto message.
void LomConfigToMsg(const HsBaLomPipelineConfig_t& config, HsbaProto::lom_pipe_config* msg);

/// @brief Convert LOM pipeline C result to proto message.
void LomResultToMsg(const HsBaLomPipelineResult_t& result, HsbaProto::lom_pipe_result* msg);

/// @brief Convert 3DP pipeline C config to proto message.
void TdpConfigToMsg(const HsBaTdpPipelineConfig_t& config, HsbaProto::tdp_pipe_config* msg);

/// @brief Convert 3DP pipeline C result to proto message.
void TdpResultToMsg(const HsBaTdpPipelineResult_t& result, HsbaProto::tdp_pipe_result* msg);

/// @brief Convert WAAM pipeline C config to proto message.
void WaamConfigToMsg(const HsBaWaamPipelineConfig_t& config, HsbaProto::waam_pipe_config* msg);

/// @brief Convert WAAM pipeline C result to proto message.
void WaamResultToMsg(const HsBaWaamPipelineResult_t& result, HsbaProto::waam_pipe_result* msg);

}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_PIPELINE_CONFIG2MSG_HPP
