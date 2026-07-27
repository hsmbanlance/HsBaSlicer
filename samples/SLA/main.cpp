/**
 * @file main.cpp
 * @brief HsBaSlicer SLA Pipeline usage examples
 *
 * Demonstrates how to configure and run the SLA slicing pipeline (sync & async),
 * including floor/raft generation, support generation, Lua customization,
 * and zip export (config JSON + layer contour SVGs).
 *
 * Platforms: Windows / Linux / macOS / Android / iOS
 */

#include <format>
#include <string>
#include <string_view>

#include "sla_pipeline.h"

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
// Example 1: Basic usage - run SLA pipeline with default config
// ---------------------------------------------------------------------------
static int RunBasicSlaPipeline()
{
    LogMsg("=== Example 1: SLA Basic Pipeline ===");

    // 1. Get default SLA config
    HsBaSlaPipelineConfig_t cfg = HsBaCreateDefaultSlaConfig();

    // 2. Set model (required)
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. Set output path
    cfg.output_path = "output/sla_basic_output.zip";

    // 4. Run pipeline synchronously
    HsBaSlaPipelineResult_t result = HsBaRunSlaPipeline(&cfg, OnProgress, nullptr);

    // 5. Check result
    if (result.success)
    {
        LogMsg(std::format("SLA slicing OK! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLA slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    // 6. Free result memory (required)
    HsBaFreeSlaPipelineResult(&result);

    return result.success;
}

// ---------------------------------------------------------------------------
// Example 2: Custom process parameters - layer height, exposure, floor
// ---------------------------------------------------------------------------
static int RunCustomSlaPipeline()
{
    LogMsg("=== Example 2: SLA Custom Parameters ===");

    HsBaSlaPipelineConfig_t cfg = HsBaCreateDefaultSlaConfig();

    // Model
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Slice parameters
    cfg.layer_height = 0.05f;       // 0.05mm (50 micron)
    cfg.first_layer_height = 0.1f;  // 0.1mm

    // Exposure parameters
    cfg.bottom_exposure_time = 60.0f;  // Bottom exposure 60s
    cfg.normal_exposure_time = 2.5f;   // Normal exposure 2.5s
    cfg.bottom_lift_distance = 5.0f;   // Bottom lift 5mm
    cfg.lift_distance = 3.0f;          // Normal lift 3mm
    cfg.lift_speed = 60.0f;            // Lift speed 60mm/s
    cfg.retract_speed = 150.0f;        // Retract speed 150mm/s

    // Floor/Raft parameters
    cfg.floor_raft_offset = 3.0f;   // Raft offset 3mm
    cfg.floor_border_width = 1.5f;  // Border width 1.5mm
    cfg.floor_fill_spacing = 0.5f;  // Fill spacing 0.5mm
    cfg.floor_border_count = 3;     // 3 border loops
    cfg.floor_use_convex_hull = 1;  // Use convex hull

    // Support parameters
    cfg.enable_support = 1;
    cfg.overhang_angle = 45.0f;   // Overhang threshold 45 deg
    cfg.support_gap = 0.5f;       // Support gap 0.5mm
    cfg.support_diameter = 2.0f;  // Support diameter 2mm
    cfg.support_density = 0.3f;   // Support density 30%
    cfg.support_pattern = HSBA_SLA_SUPPORT_SACRIFICIAL;

    // Output
    cfg.output_path = "output/sla_custom_output.zip";

    // Run
    HsBaSlaPipelineResult_t result = HsBaRunSlaPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("SLA custom slicing done! Layers: {}, Time: {:.2f}s", result.total_layers,
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLA slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeSlaPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 3: Lua custom floor, support, and export
// ---------------------------------------------------------------------------
static int RunLuaCustomSlaPipeline()
{
    LogMsg("=== Example 3: SLA Lua Custom Floor/Support/Export ===");

    HsBaSlaPipelineConfig_t cfg = HsBaCreateDefaultSlaConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Basic process
    cfg.layer_height = 0.05f;
    cfg.first_layer_height = 0.1f;

    // Lua custom floor: script path and function name
    cfg.floor_lua_script = "scripts/my_sla_floor.lua";
    cfg.floor_lua_func = "generate_floor";

    // Lua custom support: script path and function name
    cfg.support_lua_script = "scripts/my_sla_support.lua";
    cfg.support_lua_func = "generate_support";

    // Lua custom export (optional, demo only - built-in export is used by default)
    // cfg.export_lua_script = "scripts/my_sla_export.lua";
    // cfg.export_lua_func = "export_sla";

    // Output
    cfg.output_path = "output/sla_lua_custom_output.zip";

    // Run
    HsBaSlaPipelineResult_t result = HsBaRunSlaPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("SLA Lua custom slicing done! Layers: {}", result.total_layers));
    }
    else
    {
        LogMsg(std::format("SLA slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeSlaPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 4: Async pipeline
// ---------------------------------------------------------------------------

// Async completion callback
static void OnSlaPipelineComplete(HsBaSlaPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[Async] SLA done! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[Async] SLA FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
        *flag = -1;
    }

    HsBaFreeSlaPipelineResult(&result);
}

static int RunAsyncSlaPipeline()
{
    LogMsg("=== Example 4: SLA Async Pipeline ===");

    HsBaSlaPipelineConfig_t cfg = HsBaCreateDefaultSlaConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/sla_async_output.zip";

    int done_flag = 0;

    // Start async (non-blocking)
    HsBaRunSlaPipelineAsync(&cfg, OnProgress, nullptr, OnSlaPipelineComplete, &done_flag);

    // In real applications, do other work here or wait for done_flag
    while (done_flag == 0)
    {
        // Waiting...
    }

    LogMsg(std::format("SLA async pipeline finished, status: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer SLA Pipeline Examples");
    LogMsg("================================");
    LogMsg("Export format: zip (config.json + layer SVGs)");
    LogMsg("");

    RunBasicSlaPipeline();
    RunCustomSlaPipeline();
    RunLuaCustomSlaPipeline();
    RunAsyncSlaPipeline();

    LogMsg("All examples finished.");
    return 0;
}
