#include "custom_pipeline.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Extends/lua_pipeline.hpp"
#include "base/coroutine.hpp"

namespace HsBa::Slicer::Pipeline
{

struct InternalCustomResult
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;
    std::string result_string;
    std::string error_message;
    double elapsed_seconds = 0.0;
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
};

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
                std::memcpy(data, str.data(), str.size());
            if (data)
                data[str.size()] = '\0';
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
    return cr;
}

Utils::Task<InternalCustomResult> RunCustomPipelineAsync(const InternalCustomConfig& cfg)
{
    // 把常用自定义类型的 AnyObject/Lua 注册函数装入通用注册池，供流水线 Lua 环境使用
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
            cfg.progress_cb(percent, std::string(stage).c_str(), cfg.progress_user_data);
    };

    const LuaPipelineOutput out = RunLuaPipeline(ctx);

    result.success = out.success;
    result.total_layers = out.total_layers;
    result.output_path = out.output_path;
    result.result_string = out.result_string;
    result.error_message = out.error_message;

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    co_return result;
}

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
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalCustomConfig>(
        HsBa::Slicer::Pipeline::BuildCustomConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunCustomPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalCustomResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCCustomResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreeCustomPipelineResult(HsBaCustomPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->output_path, nullptr));
    std::free(std::exchange(result->result_string, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
