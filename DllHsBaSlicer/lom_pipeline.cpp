/** @file lom_pipeline.cpp
 * @brief C ABI adapter for the LOM pipeline: marshals the shared kernel output into the C result.
 *
 * The full LOM stage logic now lives in the Lib kernel (LibHsBaSlicer/Pipeline/lom_pipeline.cpp),
 * which the C ABI and the C++20 module both call. This file only validates C arguments, wraps the
 * C progress callback into the kernel's native hooks, converts the native output into the malloc-
 * owned C result and stores the captured layer geometry into the Dll snapshot pool to obtain a
 * geometry_id.
 * @author HsBa
 */
#include "lom_pipeline.h"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "LibHsBaSlicer/Pipeline/lom_pipeline.hpp"
#include "geometry_store.hpp"
#include "job_control.h"

namespace HsBa::Slicer::Pipeline
{
namespace
{

// RAII guard: ensures malloc-allocated C strings are freed on exception/early return.
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
            {
                std::memcpy(data, str.c_str(), str.size() + 1);
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

HsBaLomPipelineResult_t ToCResult(const HsBa::Slicer::Kernel::LomOutput& out)
{
    OwnedCString export_path(out.export_path);
    OwnedCString error(out.error);

    long long geometry_id = 0;
    if (out.geometry.valid)
    {
        geometry_id = CaptureLayerGeometry(HSBA_PIPELINE_LOM, out.geometry.outlines, out.geometry.fills,
                                           out.geometry.supports, out.geometry.zs);
    }

    HsBaLomPipelineResult_t cr{};
    cr.success = out.success ? 1 : 0;
    cr.total_layers = out.total_layers;
    cr.export_path = export_path.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = out.elapsed_seconds;
    cr.geometry_id = geometry_id;
    return cr;
}

// Bridges the C progress callback into the kernel's native std::function hook.
HsBa::Slicer::Kernel::Hooks MakeHooks(HsBaLomProgressCallback callback, void* user_data)
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
struct LomAsyncThunk
{
    HsBaLomResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunLomPipelineAsync.
 *
 * The job owns the temporary result struct and its strings, and releases both once this
 * callback returns, matching the copy-then-HsBaFreeLomPipelineResult contract.
 */
void DeliverLomResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<LomAsyncThunk> owned(static_cast<LomAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaLomPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaLomPipelineConfig_t HsBaCreateDefaultLomConfig(void)
{
    return HsBaLomConfigDefault();
}

HSBA_SLICER_API HsBaLomPipelineResult_t HsBaRunLomPipeline(const HsBaLomPipelineConfig_t* config,
                                                           HsBaLomProgressCallback callback, void* user_data)
{
    if (!config)
    {
        HsBaLomPipelineResult_t cr{};
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
    auto out = HsBa::Slicer::Kernel::RunLom(*config, hooks);
    return HsBa::Slicer::Pipeline::ToCResult(out);
}

HSBA_SLICER_API void HsBaRunLomPipelineAsync(const HsBaLomPipelineConfig_t* config, HsBaLomProgressCallback callback,
                                             void* user_data, HsBaLomResultCallback result_callback,
                                             void* result_user_data)
{
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::LomAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_LOM, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverLomResult, thunk);
}

HSBA_SLICER_API void HsBaFreeLomPipelineResult(HsBaLomPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
