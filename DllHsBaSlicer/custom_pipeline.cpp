/** @file custom_pipeline.cpp
 * @brief C ABI adapter for the custom pipeline: marshals the shared kernel output into the C result.
 *
 * The full custom-pipeline entry now lives in the Lib kernel (LibHsBaSlicer/Pipeline/custom_pipeline.cpp),
 * which the C ABI and the C++20 module both call. This file only validates C arguments, wraps the
 * C progress callback into the kernel's native hooks and converts the native output into the
 * malloc-owned C result. A Lua pipeline never contributes a layer-geometry snapshot.
 * @author HsBa
 */
#include "custom_pipeline.h"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "LibHsBaSlicer/Pipeline/custom_pipeline.hpp"
#include "job_control.h"

namespace HsBa::Slicer::Pipeline
{
namespace
{

struct OwnedCString
{
    char* data = nullptr;

    OwnedCString() = default;
    explicit OwnedCString(std::string_view str)
    {
        if (!str.empty())
        {
            data = static_cast<char*>(std::malloc(str.size() + 1));
            if (data)
            {
                std::memcpy(data, str.data(), str.size());
            }
            if (data)
            {
                data[str.size()] = '\0';
            }
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

HsBaCustomPipelineResult_t ToCCustomResult(const HsBa::Slicer::Kernel::CustomOutput& out)
{
    OwnedCString output_path(out.output_path);
    OwnedCString result_string(out.result_string);
    OwnedCString error(out.error);

    HsBaCustomPipelineResult_t cr{};
    cr.success = out.success ? 1 : 0;
    cr.total_layers = out.total_layers;
    cr.output_path = output_path.release();
    cr.result_string = result_string.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = out.elapsed_seconds;
    cr.geometry_id = 0;  // A Lua pipeline never contributes a layer-geometry snapshot
    return cr;
}

// Bridges the C progress callback into the kernel's native std::function hook.
HsBa::Slicer::Kernel::Hooks MakeHooks(HsBaCustomProgressCallback callback, void* user_data)
{
    HsBa::Slicer::Kernel::Hooks hooks;
    if (callback)
    {
        hooks.progress = [callback, user_data](int percent, std::string_view stage)
        {
            std::string text(stage);
            callback(percent, text.c_str(), user_data);
        };
    }
    return hooks;
}

// Carries the caller's result callback across the job thread boundary.
struct CustomAsyncThunk
{
    HsBaCustomResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunCustomPipelineAsync.
 *
 * The job owns the temporary result struct and its strings, and releases both once this
 * callback returns, matching the copy-then-HsBaFreeCustomPipelineResult contract.
 */
void DeliverCustomResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<CustomAsyncThunk> owned(static_cast<CustomAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaCustomPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaCustomPipelineConfig_t HsBaCreateDefaultCustomConfig(void)
{
    return HsBaCustomConfigDefault();
}

HSBA_SLICER_API HsBaCustomPipelineResult_t HsBaRunCustomPipeline(const HsBaCustomPipelineConfig_t* config,
                                                                 HsBaCustomProgressCallback callback, void* user_data)
{
    if (!config)
    {
        HsBaCustomPipelineResult_t cr{};
        cr.success = 0;
        cr.total_layers = 0;
        cr.error_message = static_cast<char*>(std::malloc(std::strlen("null configuration") + 1));
        if (cr.error_message)
        {
            std::memcpy(cr.error_message, "null configuration", std::strlen("null configuration") + 1);
        }
        return cr;
    }
    auto hooks = HsBa::Slicer::Pipeline::MakeHooks(callback, user_data);
    auto out = HsBa::Slicer::Kernel::RunCustom(*config, hooks);
    return HsBa::Slicer::Pipeline::ToCCustomResult(out);
}

HSBA_SLICER_API void HsBaRunCustomPipelineAsync(const HsBaCustomPipelineConfig_t* config,
                                                HsBaCustomProgressCallback callback, void* user_data,
                                                HsBaCustomResultCallback result_callback, void* result_user_data)
{
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::CustomAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_CUSTOM, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverCustomResult, thunk);
}

HSBA_SLICER_API void HsBaFreeCustomPipelineResult(HsBaCustomPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->output_path, nullptr));
    std::free(std::exchange(result->result_string, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
