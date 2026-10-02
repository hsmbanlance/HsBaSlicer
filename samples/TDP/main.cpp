/**
 * @file main.cpp
 * @brief HsBaSlicer 3DP (binder jetting) Pipeline usage examples
 *
 * Demonstrates how to configure and run the 3DP slicing pipeline (sync & async),
 * with Lua-driven export (zip archive + database registration).
 *
 * 3DP (Three-Dimensional Printing / binder jetting) is a powder-bed process:
 *   - Liquid binder is jetted onto a powder bed layer by layer
 *   - Head / drop / curing parameters are handed to the Lua export via config JSON
 *   - Output format is entirely controlled by the Lua export script
 *
 * Platforms: Windows / Linux / macOS / Android / iOS
 */

#include <format>
#include <string>
#include <string_view>

#include "tdp_pipeline.h"

#ifndef HSBA_GAME_CONSOLE
#include "logger/logger.hpp"
using HsBa::Slicer::Log::LoggerSingletone;
#endif

// ---------------------------------------------------------------------------
// Cross-platform logging helper
// ---------------------------------------------------------------------------
namespace
{
void LogMsg(std::string_view msg)
{
#ifndef HSBA_GAME_CONSOLE
    LoggerSingletone::LogInfo(msg);
#else
    (void)msg;
#endif
}

void OnProgress(int percent, const char* stage, void* /*user_data*/)
{
#ifndef HSBA_GAME_CONSOLE
    LoggerSingletone::LogInfo(std::format("[{}%] {}", percent, stage));
#else
    (void)percent;
    (void)stage;
#endif
}
}  // namespace

// ---------------------------------------------------------------------------
// Example 1: Basic usage - run 3DP pipeline with minimal config
// ---------------------------------------------------------------------------
static int RunBasicTdpPipeline()
{
    LogMsg("=== Example 1: 3DP Basic Pipeline ===");

    HsBaTdpPipelineConfig_t cfg = HsBaCreateDefaultTdpConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    cfg.export_lua_script = "scripts/my_tdp_export.lua";
    cfg.export_lua_func = "export_tdp";
    cfg.output_path = "output/3dp_basic_output.zip";

    HsBaTdpPipelineResult_t result = HsBaRunTdpPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("3DP slicing OK! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("3DP slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeTdpPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 2: Custom binder / head parameters
// ---------------------------------------------------------------------------
static int RunCustomTdpPipeline()
{
    LogMsg("=== Example 2: 3DP Custom Binder Parameters ===");

    HsBaTdpPipelineConfig_t cfg = HsBaCreateDefaultTdpConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    cfg.layer_height = 0.09f;
    cfg.first_layer_height = 0.11f;

    // Binder jetting parameters
    cfg.head_count = 256;
    cfg.drop_spacing = 0.04f;
    cfg.binder_saturation = 0.75f;
    cfg.ink_curing_time = 1.5f;
    cfg.bed_temperature = 45.0f;
    cfg.binder_mode = HSBA_TDP_FULL_COLOR;

    cfg.export_lua_script = "scripts/my_tdp_export.lua";
    cfg.export_lua_func = "export_tdp";
    cfg.output_path = "output/3dp_custom_output.zip";

    HsBaTdpPipelineResult_t result = HsBaRunTdpPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("3DP custom slicing done! Layers: {}, Time: {:.2f}s", result.total_layers,
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("3DP slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeTdpPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 3: Async pipeline
// ---------------------------------------------------------------------------
static void OnTdpPipelineComplete(HsBaTdpPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[Async] 3DP done! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[Async] 3DP FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
        *flag = -1;
    }

    HsBaFreeTdpPipelineResult(&result);
}

static int RunAsyncTdpPipeline()
{
    LogMsg("=== Example 3: 3DP Async Pipeline ===");

    HsBaTdpPipelineConfig_t cfg = HsBaCreateDefaultTdpConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.export_lua_script = "scripts/my_tdp_export.lua";
    cfg.export_lua_func = "export_tdp";
    cfg.output_path = "output/3dp_async_output.zip";

    int done_flag = 0;
    HsBaRunTdpPipelineAsync(&cfg, OnProgress, nullptr, OnTdpPipelineComplete, &done_flag);

    while (done_flag == 0)
    {
        // Waiting...
    }

    LogMsg(std::format("3DP async pipeline finished, status: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer 3DP Pipeline Examples");
    LogMsg("================================");
    LogMsg("Export format: Lua-driven (zip + database registration)");
    LogMsg("");

    RunBasicTdpPipeline();
    RunCustomTdpPipeline();
    RunAsyncTdpPipeline();

    LogMsg("All examples finished.");
    return 0;
}
