#include "param_store_pipeline.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

#include "LibHsBaSlicer/ParamStore/param_store_ops.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

// 把 std::string 复制成由 std::malloc 持有的堆字符串（空串返回 nullptr），
// 交由调用方通过 HsBaFreeParamStoreResult 释放。镜像 file_transfer_pipeline.cpp 的 OwnedCString。
char* DupToHeap(std::string_view s)
{
    if (s.empty())
        return nullptr;
    char* data = static_cast<char*>(std::malloc(s.size() + 1));
    if (data)
        std::memcpy(data, s.data(), s.size());
    if (data)
        data[s.size()] = '\0';
    return data;
}

std::string Safe(const char* p)
{
    return p ? std::string(p) : std::string();
}

// C enum -> Lib enum（显式 switch，不依赖底层整型顺序）。
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

ParamBackend ToLibBackend(HsBaParamStoreBackend b)
{
    switch (b)
    {
    case HSBA_PARAM_BACKEND_MYSQL:
        return ParamBackend::MySql;
    case HSBA_PARAM_BACKEND_POSTGRESQL:
        return ParamBackend::PostgreSql;
    case HSBA_PARAM_BACKEND_SQLITE:
    default:
        return ParamBackend::Sqlite;
    }
}

ParamStoreConn ToLibConn(const HsBaParamStoreConn_t* c)
{
    ParamStoreConn o;
    o.backend = ToLibBackend(c->backend);
    o.sqlitePath = Safe(c->sqlite_path);
    o.host = Safe(c->host);
    o.user = Safe(c->user);
    o.password = Safe(c->password);
    o.database = Safe(c->database);
    o.port = c->port;
    return o;
}

HsBaParamStoreResult_t MakeFailure(const char* msg, double elapsed)
{
    HsBaParamStoreResult_t r{};
    r.success = 0;
    r.param_id = 0;
    r.error_message = DupToHeap(msg ? msg : "error");
    r.elapsed_seconds = elapsed;
    return r;
}

HsBaParamStoreResult_t ToCResult(const ParamStoreOutcome& o, double elapsed)
{
    HsBaParamStoreResult_t r{};
    r.success = o.success ? 1 : 0;
    r.param_id = o.paramId;
    r.error_message = DupToHeap(o.error);
    r.elapsed_seconds = elapsed;
    return r;
}

bool ValidKind(HsBaPipelineKind k)
{
    const int v = static_cast<int>(k);
    return v >= 0 && v < HSBA_PIPELINE_KIND_COUNT;
}

}  // namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaParamStoreResult_t HsBaSavePipelineParams(const HsBaParamStoreConn_t* conn, HsBaPipelineKind kind,
                                                              const char* table, const char* key, const void* config)
{
    if (!conn || !key || !config || !HsBa::Slicer::Pipeline::ValidKind(kind))
        return HsBa::Slicer::Pipeline::MakeFailure("invalid arguments to HsBaSavePipelineParams", 0.0);

    auto start = std::chrono::steady_clock::now();
    auto lib_conn = HsBa::Slicer::Pipeline::ToLibConn(conn);
    auto outcome = HsBa::Slicer::SavePipelineParams(lib_conn, HsBa::Slicer::Pipeline::ToLibKind(kind),
                                                    table ? table : "", key, config);
    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    return HsBa::Slicer::Pipeline::ToCResult(outcome, elapsed);
}

HSBA_SLICER_API HsBaParamStoreResult_t HsBaLoadPipelineParams(const HsBaParamStoreConn_t* conn, HsBaPipelineKind kind,
                                                              const char* table, const char* key, void* out_config)
{
    if (!conn || !key || !out_config || !HsBa::Slicer::Pipeline::ValidKind(kind))
        return HsBa::Slicer::Pipeline::MakeFailure("invalid arguments to HsBaLoadPipelineParams", 0.0);

    auto start = std::chrono::steady_clock::now();
    auto lib_conn = HsBa::Slicer::Pipeline::ToLibConn(conn);
    auto outcome = HsBa::Slicer::LoadPipelineParams(lib_conn, HsBa::Slicer::Pipeline::ToLibKind(kind),
                                                    table ? table : "", key, out_config);
    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    return HsBa::Slicer::Pipeline::ToCResult(outcome, elapsed);
}

HSBA_SLICER_API void HsBaFreeLoadedPipelineConfig(HsBaPipelineKind kind, void* config)
{
    if (!config || !HsBa::Slicer::Pipeline::ValidKind(kind))
        return;
    HsBa::Slicer::FreeLoadedConfigStrings(HsBa::Slicer::Pipeline::ToLibKind(kind), config);
}

HSBA_SLICER_API void HsBaFreeParamStoreResult(HsBaParamStoreResult_t* result)
{
    if (!result)
        return;
    std::free(result->error_message);
    result->error_message = nullptr;
}
