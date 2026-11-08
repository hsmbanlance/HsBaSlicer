/** @file custom_pipeline.cpp
 * @brief Implementation of the fully Lua-driven custom pipeline C ABI.
 * @author HsBa
 */
#include "custom_pipeline.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Extends/lua_pipeline.hpp"
#include "base/coroutine.hpp"
#include "base/error.hpp"
#include "job_control.h"

namespace HsBa::Slicer::Pipeline
{
namespace
{

struct InternalCustomResult
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;
    std::string result_string;
    std::string error_message;
    double elapsed_seconds = 0.0;
    bool cancelled = false;
};

struct InternalCustomConfig
{
    std::string pipeline_lua_script;
    std::string pipeline_lua_source;
    std::string entry_func;
    std::string config_json;
    std::string model_name;
    std::string model_path;
    std::string output_path;
    HsBaCustomProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
    const void* cancel_flag = nullptr;  ///< std::atomic<bool>* set by the host or by its job handle

    // A Lua pipeline is one opaque library call: it neither polls the flag nor can it be
    // interrupted without unwinding through the Lua VM, so cancellation is honoured right
    // before the call and, when requested during it, on the way out (see the Run body).
    // capture_geometry is ignored because the geometry a script produces is unknown here.
    /** @brief Whether the host asked this run to stop. */
    bool CancelRequested() const
    {
        return cancel_flag && static_cast<const std::atomic<bool>*>(cancel_flag)->load(std::memory_order_relaxed);
    }

    /** @brief Throw at a cancellation point so the run unwinds as a cancelled result. */
    void CheckCancel() const
    {
        if (CancelRequested())
        {
            throw CancelledError("pipeline cancelled");
        }
    }
};

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

InternalCustomConfig BuildCustomConfig(const HsBaCustomPipelineConfig_t* cfg, HsBaCustomProgressCallback cb, void* ud)
{
    InternalCustomConfig ic;
    ic.pipeline_lua_script = cfg->pipeline_lua_script ? cfg->pipeline_lua_script : "";
    ic.pipeline_lua_source = cfg->pipeline_lua_source ? cfg->pipeline_lua_source : "";
    ic.entry_func = cfg->entry_func ? cfg->entry_func : "";
    ic.config_json = cfg->config_json ? cfg->config_json : "";
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    ic.cancel_flag = cfg->cancel_flag;
    return ic;
}

HsBaCustomPipelineResult_t ToCCustomResult(const InternalCustomResult& ir)
{
    OwnedCString output_path(ir.output_path);
    OwnedCString result_string(ir.result_string);
    OwnedCString error(ir.error_message);

    HsBaCustomPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.output_path = output_path.release();
    cr.result_string = result_string.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    cr.geometry_id = 0;  // A Lua pipeline never contributes a layer-geometry snapshot
    return cr;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
Utils::Task<InternalCustomResult> RunCustomPipelineAsync(const InternalCustomConfig& cfg)
{
    // Load the AnyObject/Lua registration functions for common custom types into the generic registry pool for the
    // pipeline Lua environment to use
    HsBa::Slicer::InstallCommonAnyObjectTypes();

    InternalCustomResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (cfg.pipeline_lua_script.empty() && cfg.pipeline_lua_source.empty())
    {
        result.error_message = "Custom pipeline requires pipeline_lua_script or pipeline_lua_source";
        auto end_time = std::chrono::steady_clock::now();
        result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();
        co_return result;
    }

    LuaPipelineContext ctx;
    ctx.script = cfg.pipeline_lua_source;
    ctx.script_file = cfg.pipeline_lua_script;
    ctx.entry_func = cfg.entry_func.empty() ? "run_pipeline" : cfg.entry_func;
    ctx.config_json = cfg.config_json;
    ctx.output_path = cfg.output_path;
    ctx.model_name = cfg.model_name;
    ctx.model_path = cfg.model_path;
    ctx.progress_cb = [&cfg](int percent, std::string_view stage)
    {
        if (cfg.progress_cb)
        {
            cfg.progress_cb(percent, std::string(stage).c_str(), cfg.progress_user_data);
        }
    };

    try
    {
        cfg.CheckCancel();
        const LuaPipelineOutput out = RunLuaPipeline(ctx);

        result.success = out.success;
        result.total_layers = out.total_layers;
        result.output_path = out.output_path;
        result.result_string = out.result_string;
        result.error_message = out.error_message;
        if (cfg.CancelRequested())
        {
            // The script ran to completion because it cannot be interrupted, but the host
            // asked to stop, so the run is published as cancelled instead of successful.
            result.success = false;
            result.cancelled = true;
            result.error_message = "Cancelled: pipeline cancelled during the Lua script";
        }
    }
    catch (const CancelledError& e)
    {
        result.success = false;
        result.cancelled = true;
        // The "Cancelled" prefix is what the C ABI callers use to tell a cancelled run
        // apart from a failed one, since the result structs carry no status field.
        result.error_message = std::string("Cancelled: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    co_return result;
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

}  // anonymous namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaCustomPipelineConfig_t HsBaCreateDefaultCustomConfig(void)
{
    return HsBaCustomConfigDefault();
}

HSBA_SLICER_API HsBaCustomPipelineResult_t HsBaRunCustomPipeline(const HsBaCustomPipelineConfig_t* config,
                                                                 HsBaCustomProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildCustomConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunCustomPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCCustomResult(ir);
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
