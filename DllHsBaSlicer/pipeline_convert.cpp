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
