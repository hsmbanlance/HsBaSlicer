/**
 * @file main.cpp
 * @brief HsBaSlicer SLM Pipeline usage examples
 *
 * Demonstrates how to configure and run the SLM slicing pipeline (sync & async),
 * with Lua-driven export (zip archive + database registration).
 *
 * SLM (Selective Laser Melting) is a metal powder-bed process:
 *   - Mirrors the SLS flow (no floor/raft, no support structures)
 *   - Adds metal-specific parameters: powder material, energy source, gas
 *   - Output format is entirely controlled by the Lua export script
 *
 * Platforms: Windows / Linux / macOS / Android / iOS
 */

#include <format>
#include <string>
#include <string_view>

#include "slm_pipeline.h"

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

// Progress callback
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
// Example 1: Basic usage - run SLM pipeline with minimal config
// ---------------------------------------------------------------------------
static int RunBasicSlmPipeline()
{
    LogMsg("=== Example 1: SLM Basic Pipeline ===");

    HsBaSlmPipelineConfig_t cfg = HsBaCreateDefaultSlmConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Metal defaults from HsBaSlmConfigDefault(): titanium, laser, argon.
    cfg.export_lua_script = "scripts/my_slm_export.lua";
    cfg.export_lua_func = "export_slm";
    cfg.output_path = "output/slm_basic_output.zip";

    HsBaSlmPipelineResult_t result = HsBaRunSlmPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("SLM slicing OK! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLM slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeSlmPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 2: Custom process parameters - laser, material, shielding gas
// ---------------------------------------------------------------------------
static int RunCustomSlmPipeline()
{
    LogMsg("=== Example 2: SLM Custom Metal Parameters ===");

    HsBaSlmPipelineConfig_t cfg = HsBaCreateDefaultSlmConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Slice parameters
    cfg.layer_height = 0.05f;
    cfg.first_layer_height = 0.06f;

    // Laser / scan parameters
    cfg.laser_power = 280.0f;
    cfg.scan_speed = 1200.0f;
    cfg.hatch_spacing = 0.09f;
    cfg.hatch_rotation = 67.0f;
    cfg.bed_temperature = 150.0f;

    // Metal-specific parameters
    cfg.material = HSBA_SLM_MATERIAL_ALUMINUM;
    cfg.light_source = HSBA_SLM_LIGHT_LASER;
    cfg.protect_gas = HSBA_METAL_GAS_ARGON;

    cfg.export_lua_script = "scripts/my_slm_export.lua";
    cfg.export_lua_func = "export_slm";
    cfg.output_path = "output/slm_custom_output.zip";

    HsBaSlmPipelineResult_t result = HsBaRunSlmPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("SLM custom slicing done! Layers: {}, Time: {:.2f}s", result.total_layers,
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLM slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeSlmPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 3: Async pipeline
// ---------------------------------------------------------------------------
static void OnSlmPipelineComplete(HsBaSlmPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[Async] SLM done! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[Async] SLM FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
        *flag = -1;
    }

    HsBaFreeSlmPipelineResult(&result);
}

static int RunAsyncSlmPipeline()
{
    LogMsg("=== Example 3: SLM Async Pipeline ===");

    HsBaSlmPipelineConfig_t cfg = HsBaCreateDefaultSlmConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.export_lua_script = "scripts/my_slm_export.lua";
    cfg.export_lua_func = "export_slm";
    cfg.output_path = "output/slm_async_output.zip";

    int done_flag = 0;
    HsBaRunSlmPipelineAsync(&cfg, OnProgress, nullptr, OnSlmPipelineComplete, &done_flag);

    while (done_flag == 0)
    {
        // Waiting...
    }

    LogMsg(std::format("SLM async pipeline finished, status: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer SLM Pipeline Examples");
    LogMsg("================================");
    LogMsg("Export format: Lua-driven (zip + database registration)");
    LogMsg("");

    RunBasicSlmPipeline();
    RunCustomSlmPipeline();
    RunAsyncSlmPipeline();

    LogMsg("All examples finished.");
    return 0;
}
