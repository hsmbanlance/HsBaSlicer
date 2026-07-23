/**
 * @file main.cpp
 * @brief HsBaSlicer SLS Pipeline usage examples
 *
 * Demonstrates how to configure and run the SLS slicing pipeline (sync & async),
 * with Lua-driven export (zip archive + database registration).
 *
 * SLS (Selective Laser Sintering) is a powder bed process:
 *   - No floor/raft needed
 *   - No support structures needed (powder bed provides support)
 *   - Output format is entirely controlled by Lua script
 *
 * Platforms: Windows / Linux / macOS / Android / iOS
 */

#include <format>
#include <string>
#include <string_view>

#include "sls_pipeline.h"

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
// Example 1: Basic usage - run SLS pipeline with minimal config
// ---------------------------------------------------------------------------
static int RunBasicSlsPipeline()
{
    LogMsg("=== Example 1: SLS Basic Pipeline ===");

    // 1. Get default SLS config
    HsBaSlsPipelineConfig_t cfg = HsBaCreateDefaultSlsConfig();

    // 2. Set model (required)
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. Set Lua export script (required - SLS has no standard output format)
    cfg.export_lua_script = "scripts/my_sls_export.lua";
    cfg.export_lua_func = "export_sls";

    // 4. Set output path
    cfg.output_path = "output/sls_basic_output.zip";

    // 5. Run pipeline synchronously
    HsBaSlsPipelineResult_t result =
        HsBaRunSlsPipeline(&cfg, OnProgress, nullptr);

    // 6. Check result
    if (result.success)
    {
        LogMsg(std::format("SLS slicing OK! Layers: {}, Export: {}, Time: {:.2f}s",
                           result.total_layers,
                           result.export_path ? result.export_path : "N/A",
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLS slicing FAILED: {}",
                           result.error_message ? result.error_message : "Unknown error"));
    }

    // 7. Free result memory (required)
    HsBaFreeSlsPipelineResult(&result);

    return result.success;
}

// ---------------------------------------------------------------------------
// Example 2: Custom process parameters - laser power, scan speed, hatch
// ---------------------------------------------------------------------------
static int RunCustomSlsPipeline()
{
    LogMsg("=== Example 2: SLS Custom Laser Parameters ===");

    HsBaSlsPipelineConfig_t cfg = HsBaCreateDefaultSlsConfig();

    // Model
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Slice parameters
    cfg.layer_height = 0.08f;         // 0.08mm layer thickness
    cfg.first_layer_height = 0.12f;   // 0.12mm first layer

    // Laser parameters
    cfg.laser_power = 45.0f;          // 45W laser power
    cfg.scan_speed = 3500.0f;         // 3500 mm/s scan speed
    cfg.hatch_spacing = 0.12f;        // 0.12mm hatch spacing
    cfg.hatch_rotation = 67.0f;       // 67 degrees rotation between layers
    cfg.bed_temperature = 175.0f;     // 175°C powder bed temperature

    // Lua export
    cfg.export_lua_script = "scripts/my_sls_export.lua";
    cfg.export_lua_func = "export_sls";

    // Output
    cfg.output_path = "output/sls_custom_output.zip";

    // Run
    HsBaSlsPipelineResult_t result =
        HsBaRunSlsPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("SLS custom slicing done! Layers: {}, Time: {:.2f}s",
                           result.total_layers, result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLS slicing FAILED: {}",
                           result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeSlsPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 3: Async pipeline
// ---------------------------------------------------------------------------

// Async completion callback
static void OnSlsPipelineComplete(HsBaSlsPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[Async] SLS done! Layers: {}, Export: {}, Time: {:.2f}s",
                           result.total_layers,
                           result.export_path ? result.export_path : "N/A",
                           result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[Async] SLS FAILED: {}",
                           result.error_message ? result.error_message : "Unknown error"));
        *flag = -1;
    }

    HsBaFreeSlsPipelineResult(&result);
}

static int RunAsyncSlsPipeline()
{
    LogMsg("=== Example 3: SLS Async Pipeline ===");

    HsBaSlsPipelineConfig_t cfg = HsBaCreateDefaultSlsConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.export_lua_script = "scripts/my_sls_export.lua";
    cfg.export_lua_func = "export_sls";
    cfg.output_path = "output/sls_async_output.zip";

    int done_flag = 0;

    // Start async (non-blocking)
    HsBaRunSlsPipelineAsync(&cfg, OnProgress, nullptr,
                            OnSlsPipelineComplete, &done_flag);

    // In real applications, do other work here or wait for done_flag
    while (done_flag == 0)
    {
        // Waiting...
    }

    LogMsg(std::format("SLS async pipeline finished, status: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer SLS Pipeline Examples");
    LogMsg("================================");
    LogMsg("Export format: Lua-driven (zip + database registration)");
    LogMsg("");

    RunBasicSlsPipeline();
    RunCustomSlsPipeline();
    RunAsyncSlsPipeline();

    LogMsg("All examples finished.");
    return 0;
}
