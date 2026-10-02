#include "pipeline_convert.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include "convert/Msg2PipelineConfig.hpp"
#include "convert/PipelineConfig2Msg.hpp"

namespace
{

void FreeIfNotNull(char*& ptr)
{
    std::free(std::exchange(ptr, nullptr));
}

}  // anonymous namespace

// ========== FDM ==========

HSBA_SLICER_API int HsBaFdmConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFdmPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;

    HsbaProto::msg_fdm_pipeline_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToFdmConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaFdmConfigToProtoBytes(const HsBaFdmPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;

    HsbaProto::msg_fdm_pipeline_config msg;
    HsBa::Slicer::FdmConfigToMsg(*config, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaFdmResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFdmPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;

    HsbaProto::msg_fdm_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToFdmResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaFdmResultToProtoBytes(const HsBaFdmPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;

    HsbaProto::msg_fdm_pipe_result msg;
    HsBa::Slicer::FdmResultToMsg(*result, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== SLA ==========

HSBA_SLICER_API int HsBaSlaConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlaPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;

    HsbaProto::sla_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToSlaConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaSlaConfigToProtoBytes(const HsBaSlaPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;

    HsbaProto::sla_pipe_config msg;
    HsBa::Slicer::SlaConfigToMsg(*config, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaSlaResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlaPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;

    HsbaProto::sla_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToSlaResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaSlaResultToProtoBytes(const HsBaSlaPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;

    HsbaProto::sla_pipe_result msg;
    HsBa::Slicer::SlaResultToMsg(*result, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== SLS ==========

HSBA_SLICER_API int HsBaSlsConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlsPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;

    HsbaProto::sls_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToSlsConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaSlsConfigToProtoBytes(const HsBaSlsPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;

    HsbaProto::sls_pipe_config msg;
    HsBa::Slicer::SlsConfigToMsg(*config, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaSlsResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlsPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;

    HsbaProto::sls_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToSlsResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaSlsResultToProtoBytes(const HsBaSlsPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;

    HsbaProto::sls_pipe_result msg;
    HsBa::Slicer::SlsResultToMsg(*result, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== File Transfer ==========

HSBA_SLICER_API int HsBaFileTransferConfigFromProtoBytes(const void* proto_data, int proto_size,
                                                         HsBaFileTransferPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;

    HsbaProto::file_transfer_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToFileTransferConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaFileTransferConfigToProtoBytes(const HsBaFileTransferPipelineConfig_t* config, void** out_data,
                                                       int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;

    HsbaProto::file_transfer_pipe_config msg;
    HsBa::Slicer::FileTransferConfigToMsg(*config, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaFileTransferResultFromProtoBytes(const void* proto_data, int proto_size,
                                                         HsBaFileTransferPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;

    HsbaProto::file_transfer_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToFileTransferResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaFileTransferResultToProtoBytes(const HsBaFileTransferPipelineResult_t* result, void** out_data,
                                                       int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;

    HsbaProto::file_transfer_pipe_result msg;
    HsBa::Slicer::FileTransferResultToMsg(*result, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== Custom Lua pipeline ==========

HSBA_SLICER_API int HsBaCustomConfigFromProtoBytes(const void* proto_data, int proto_size,
                                                   HsBaCustomPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;

    HsbaProto::custom_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToCustomConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaCustomConfigToProtoBytes(const HsBaCustomPipelineConfig_t* config, void** out_data,
                                                 int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;

    HsbaProto::custom_pipe_config msg;
    HsBa::Slicer::CustomConfigToMsg(*config, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaCustomResultFromProtoBytes(const void* proto_data, int proto_size,
                                                   HsBaCustomPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;

    HsbaProto::custom_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;

    HsBa::Slicer::MsgToCustomResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaCustomResultToProtoBytes(const HsBaCustomPipelineResult_t* result, void** out_data,
                                                 int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;

    HsbaProto::custom_pipe_result msg;
    HsBa::Slicer::CustomResultToMsg(*result, &msg);

    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;

    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }

    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== SLM ==========

HSBA_SLICER_API int HsBaSlmConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlmPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;
    HsbaProto::slm_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToSlmConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaSlmConfigToProtoBytes(const HsBaSlmPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;
    HsbaProto::slm_pipe_config msg;
    HsBa::Slicer::SlmConfigToMsg(*config, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaSlmResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlmPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;
    HsbaProto::slm_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToSlmResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaSlmResultToProtoBytes(const HsBaSlmPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;
    HsbaProto::slm_pipe_result msg;
    HsBa::Slicer::SlmResultToMsg(*result, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== LOM ==========

HSBA_SLICER_API int HsBaLomConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaLomPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;
    HsbaProto::lom_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToLomConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaLomConfigToProtoBytes(const HsBaLomPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;
    HsbaProto::lom_pipe_config msg;
    HsBa::Slicer::LomConfigToMsg(*config, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaLomResultFromProtoBytes(const void* proto_data, int proto_size, HsBaLomPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;
    HsbaProto::lom_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToLomResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaLomResultToProtoBytes(const HsBaLomPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;
    HsbaProto::lom_pipe_result msg;
    HsBa::Slicer::LomResultToMsg(*result, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== 3DP ==========

HSBA_SLICER_API int HsBaTdpConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaTdpPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;
    HsbaProto::tdp_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToTdpConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaTdpConfigToProtoBytes(const HsBaTdpPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;
    HsbaProto::tdp_pipe_config msg;
    HsBa::Slicer::TdpConfigToMsg(*config, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaTdpResultFromProtoBytes(const void* proto_data, int proto_size, HsBaTdpPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;
    HsbaProto::tdp_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToTdpResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaTdpResultToProtoBytes(const HsBaTdpPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;
    HsbaProto::tdp_pipe_result msg;
    HsBa::Slicer::TdpResultToMsg(*result, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== WAAM ==========

HSBA_SLICER_API int HsBaWaamConfigFromProtoBytes(const void* proto_data, int proto_size,
                                                 HsBaWaamPipelineConfig_t* config)
{
    if (!proto_data || proto_size <= 0 || !config)
        return 0;
    HsbaProto::waam_pipe_config msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToWaamConfig(msg, config);
    return 1;
}

HSBA_SLICER_API int HsBaWaamConfigToProtoBytes(const HsBaWaamPipelineConfig_t* config, void** out_data, int* out_size)
{
    if (!config || !out_data || !out_size)
        return 0;
    HsbaProto::waam_pipe_config msg;
    HsBa::Slicer::WaamConfigToMsg(*config, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

HSBA_SLICER_API int HsBaWaamResultFromProtoBytes(const void* proto_data, int proto_size,
                                                 HsBaWaamPipelineResult_t* result)
{
    if (!proto_data || proto_size <= 0 || !result)
        return 0;
    HsbaProto::waam_pipe_result msg;
    if (!msg.ParseFromArray(proto_data, proto_size))
        return 0;
    HsBa::Slicer::MsgToWaamResult(msg, result);
    return 1;
}

HSBA_SLICER_API int HsBaWaamResultToProtoBytes(const HsBaWaamPipelineResult_t* result, void** out_data, int* out_size)
{
    if (!result || !out_data || !out_size)
        return 0;
    HsbaProto::waam_pipe_result msg;
    HsBa::Slicer::WaamResultToMsg(*result, &msg);
    int size = static_cast<int>(msg.ByteSizeLong());
    void* buf = std::malloc(static_cast<size_t>(size));
    if (!buf)
        return 0;
    if (!msg.SerializeToArray(buf, size))
    {
        std::free(buf);
        return 0;
    }
    *out_data = buf;
    *out_size = size;
    return 1;
}

// ========== Cleanup helpers ==========

HSBA_SLICER_API void HsBaFreeFdmConfigStrings(HsBaFdmPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->support_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->support_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->infill_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->infill_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeSlaConfigStrings(HsBaSlaPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->support_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->support_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->floor_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->floor_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeSlsConfigStrings(HsBaSlsPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeFileTransferConfigStrings(HsBaFileTransferPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->host));
    FreeIfNotNull(const_cast<char*&>(config->port));
    if (config->file_paths)
    {
        for (int i = 0; i < config->file_count; ++i)
        {
            std::free(const_cast<char*>(config->file_paths[i]));
        }
        std::free(const_cast<char**>(config->file_paths));
        config->file_paths = nullptr;
        config->file_count = 0;
    }
}

HSBA_SLICER_API void HsBaFreeCustomConfigStrings(HsBaCustomPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->pipeline_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->pipeline_lua_source));
    FreeIfNotNull(const_cast<char*&>(config->entry_func));
    FreeIfNotNull(const_cast<char*&>(config->config_json));
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeSlmConfigStrings(HsBaSlmPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeLomConfigStrings(HsBaLomPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeTdpConfigStrings(HsBaTdpPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->export_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}

HSBA_SLICER_API void HsBaFreeWaamConfigStrings(HsBaWaamPipelineConfig_t* config)
{
    if (!config)
        return;
    FreeIfNotNull(const_cast<char*&>(config->model_name));
    FreeIfNotNull(const_cast<char*&>(config->model_path));
    FreeIfNotNull(const_cast<char*&>(config->path_lua_script));
    FreeIfNotNull(const_cast<char*&>(config->path_lua_func));
    FreeIfNotNull(const_cast<char*&>(config->output_path));
}
