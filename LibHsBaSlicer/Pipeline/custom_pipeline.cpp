/** @file custom_pipeline.cpp
 * @brief Custom (fully Lua-driven) pipeline orchestration kernel implementation.
 *
 * Ported from the DllHsBaSlicer coroutine `RunCustomPipelineAsync`, made synchronous so the same
 * code drives the Dll blocking C entry, the job thread and the C++20 module. A Lua pipeline is one
 * opaque library call: it neither polls the cancel flag nor can it be interrupted without unwinding
 * through the Lua VM, so cancellation is honoured right before the call and, when requested during
 * it, on the way out. capture_geometry is ignored because the geometry a script produces is unknown
 * here.
 * @author HsBa
 */
#include "LibHsBaSlicer/Pipeline/custom_pipeline.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Extends/lua_pipeline.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer::Kernel
{
namespace
{

struct Cfg
{
    std::string pipeline_lua_script;
    std::string pipeline_lua_source;
    std::string entry_func;
    std::string config_json;
    std::string model_name;
    std::string model_path;
    std::string output_path;
    std::function<void(int, std::string_view)> progress;
    const std::atomic<bool>* cancel_flag = nullptr;

    bool CancelRequested() const
    {
        return cancel_flag && cancel_flag->load(std::memory_order_relaxed);
    }

    void CheckCancel() const
    {
        if (CancelRequested())
        {
            throw CancelledError("pipeline cancelled");
        }
    }
};

Cfg BuildCfg(const HsBaCustomPipelineConfig_t& cfg, const Hooks& hooks)
{
    Cfg ic;
    ic.pipeline_lua_script = cfg.pipeline_lua_script ? cfg.pipeline_lua_script : "";
    ic.pipeline_lua_source = cfg.pipeline_lua_source ? cfg.pipeline_lua_source : "";
    ic.entry_func = cfg.entry_func ? cfg.entry_func : "";
    ic.config_json = cfg.config_json ? cfg.config_json : "";
    ic.model_name = cfg.model_name ? cfg.model_name : "";
    ic.model_path = cfg.model_path ? cfg.model_path : "";
    ic.output_path = cfg.output_path ? cfg.output_path : "";
    ic.progress = hooks.progress;
    ic.cancel_flag = static_cast<const std::atomic<bool>*>(cfg.cancel_flag);
    return ic;
}

CustomOutput RunPipeline(const Cfg& cfg)
{
    // Load the AnyObject/Lua registration functions for common custom types into the generic registry pool for the
    // pipeline Lua environment to use
    InstallCommonAnyObjectTypes();

    CustomOutput result;
    auto start_time = std::chrono::steady_clock::now();

    if (cfg.pipeline_lua_script.empty() && cfg.pipeline_lua_source.empty())
    {
        result.error = "Custom pipeline requires pipeline_lua_script or pipeline_lua_source";
        auto end_time = std::chrono::steady_clock::now();
        result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();
        return result;
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
        if (cfg.progress)
        {
            cfg.progress(percent, stage);
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
        result.error = out.error_message;
        if (cfg.CancelRequested())
        {
            // The script ran to completion because it cannot be interrupted, but the host
            // asked to stop, so the run is published as cancelled instead of successful.
            result.success = false;
            result.cancelled = true;
            result.error = "Cancelled: pipeline cancelled during the Lua script";
        }
    }
    catch (const CancelledError& e)
    {
        result.success = false;
        result.cancelled = true;
        // The "Cancelled" prefix is what the C ABI callers use to tell a cancelled run
        // apart from a failed one, since the result structs carry no status field.
        result.error = std::string("Cancelled: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    return result;
}

}  // namespace

CustomOutput RunCustom(const HsBaCustomPipelineConfig_t& cfg, const Hooks& hooks)
{
    return RunPipeline(BuildCfg(cfg, hooks));
}

}  // namespace HsBa::Slicer::Kernel
