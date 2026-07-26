#include "file_transfer_pipeline.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Transfer/file_transfer.hpp"
#include "base/coroutine.hpp"

namespace HsBa::Slicer::Pipeline
{

struct InternalFileTransferResult
{
    bool success = false;
    int files_transferred = 0;
    int total_files = 0;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

struct InternalFileTransferConfig
{
    FileTransferConfig lib_config;
    HsBaFileTransferProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
};

namespace
{

struct OwnedCString
{
    char* data = nullptr;

    OwnedCString() = default;
    explicit OwnedCString(const std::string& str)
    {
        if (!str.empty())
        {
            data = static_cast<char*>(std::malloc(str.size() + 1));
            if (data)
                std::memcpy(data, str.c_str(), str.size() + 1);
        }
    }

    OwnedCString(const OwnedCString&) = delete;
    OwnedCString& operator=(const OwnedCString&) = delete;

    OwnedCString(OwnedCString&& other) noexcept : data(std::exchange(other.data, nullptr)) {}
    OwnedCString& operator=(OwnedCString&& other) noexcept
    {
        if (this != &other)
        {
            std::free(data);
            data = std::exchange(other.data, nullptr);
        }
        return *this;
    }

    ~OwnedCString() { std::free(data); }

    char* release() { return std::exchange(data, nullptr); }
};

}  // anonymous namespace

InternalFileTransferConfig BuildFileTransferConfig(const HsBaFileTransferPipelineConfig_t* cfg,
                                                   HsBaFileTransferProgressCallback cb, void* ud)
{
    InternalFileTransferConfig ic;
    ic.lib_config.host = cfg->host ? cfg->host : "";
    ic.lib_config.port = cfg->port ? cfg->port : "";
    ic.lib_config.pool_size = cfg->pool_size > 0 ? static_cast<std::size_t>(cfg->pool_size) : 4;

    if (cfg->file_paths && cfg->file_count > 0)
    {
        ic.lib_config.files.reserve(static_cast<std::size_t>(cfg->file_count));
        for (int i = 0; i < cfg->file_count; ++i)
        {
            if (cfg->file_paths[i])
            {
                ic.lib_config.files.emplace_back(cfg->file_paths[i]);
            }
        }
    }

    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaFileTransferPipelineResult_t ToCResult(const InternalFileTransferResult& ir)
{
    OwnedCString error(ir.error_message);

    HsBaFileTransferPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.files_transferred = ir.files_transferred;
    cr.total_files = ir.total_files;
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

Utils::Task<InternalFileTransferResult> RunFileTransferPipelineAsync(const InternalFileTransferConfig& cfg)
{
    InternalFileTransferResult result;
    auto start_time = std::chrono::steady_clock::now();
    result.total_files = static_cast<int>(cfg.lib_config.files.size());

    try
    {
        // Delegate to LibHsBaSlicer TransferFiles
        FileTransferProgressFunc progress_func = nullptr;
        if (cfg.progress_cb)
        {
            progress_func = [&cfg](int percent, std::string_view stage)
            { cfg.progress_cb(percent, std::string(stage).c_str(), cfg.progress_user_data); };
        }

        FileTransferResult lib_result = TransferFiles(cfg.lib_config, progress_func);

        result.success = lib_result.success;
        result.files_transferred = lib_result.files_transferred;
        result.total_files = lib_result.total_files;
        result.error_message = lib_result.error_message;
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.error_message = std::string("File transfer error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    co_return result;
}

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaFileTransferPipelineConfig_t HsBaCreateDefaultFileTransferConfig(void)
{
    return HsBaFileTransferConfigDefault();
}

HSBA_SLICER_API HsBaFileTransferPipelineResult_t HsBaRunFileTransferPipeline(
    const HsBaFileTransferPipelineConfig_t* config, HsBaFileTransferProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildFileTransferConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunFileTransferPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunFileTransferPipelineAsync(const HsBaFileTransferPipelineConfig_t* config,
                                                      HsBaFileTransferProgressCallback callback, void* user_data,
                                                      HsBaFileTransferResultCallback result_callback,
                                                      void* result_user_data)
{
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalFileTransferConfig>(
        HsBa::Slicer::Pipeline::BuildFileTransferConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunFileTransferPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalFileTransferResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreeFileTransferPipelineResult(HsBaFileTransferPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->error_message, nullptr));
}
