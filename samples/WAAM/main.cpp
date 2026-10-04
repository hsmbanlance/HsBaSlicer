/**
 * @file main.cpp
 * @brief HsBaSlicer WAAM (wire arc additive manufacturing) Pipeline usage examples
 *
 * Demonstrates how to configure and run the WAAM slicing pipeline (sync & async).
 *
 * WAAM is a robot-based deposition process:
 *   - Metal is deposited bead-by-bead along the per-layer contours
 *   - The output is a robot language program (ABB / KUKA / FANUC), not a zip
 *   - An optional Lua path script can replace the built-in code generator
 *
 * Platforms: Windows / Linux / macOS / Android / iOS
 */

#include <format>
#include <string>
#include <string_view>

#include "waam_pipeline.h"

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
// Example 1: Basic usage - built-in ABB robot program generation
// ---------------------------------------------------------------------------
static int RunBasicWaamPipeline()
{
    LogMsg("=== Example 1: WAAM Basic Pipeline (ABB) ===");

    HsBaWaamPipelineConfig_t cfg = HsBaCreateDefaultWaamConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // No path_lua_script => built-in ABB code generator is used.
    cfg.robot_type = HSBA_WAAM_ROBOT_ABB;
    cfg.output_path = "output/waam_basic_output.txt";

    HsBaWaamPipelineResult_t result = HsBaRunWaamPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("WAAM slicing OK! Layers: {}, Robot program: {}, Time: {:.2f}s", result.total_layers,
                           result.output_path ? result.output_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("WAAM slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeWaamPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 2: Custom welding parameters + KUKA robot
// ---------------------------------------------------------------------------
static int RunCustomWaamPipeline()
{
    LogMsg("=== Example 2: WAAM Custom Welding Parameters (KUKA) ===");

    HsBaWaamPipelineConfig_t cfg = HsBaCreateDefaultWaamConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Process / geometry parameters
    cfg.layer_height = 0.6f;
    cfg.first_layer_height = 0.8f;
    cfg.bead_width = 1.0f;
    cfg.travel_speed = 6.0f;

    // Welding parameters
    cfg.material = HSBA_WAAM_MATERIAL_ALUMINUM;
    cfg.welding_process = HSBA_WAAM_WELD_ARC;
    cfg.protection = HSBA_WAAM_PROTECTION_SHIELD_GAS;
    cfg.protect_gas = HSBA_METAL_GAS_ARGON;
    cfg.arc_current = 220.0f;
    cfg.arc_voltage = 26.0f;
    cfg.wire_feed_speed = 8.0f;
    cfg.gas_flow_rate = 18.0f;
    cfg.interpass_temperature = 120.0f;

    // KUKA built-in code generator.
    cfg.robot_type = HSBA_WAAM_ROBOT_KUKA;
    cfg.output_path = "output/waam_custom_output.txt";

    HsBaWaamPipelineResult_t result = HsBaRunWaamPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("WAAM custom slicing done! Layers: {}, Robot program: {}, Time: {:.2f}s",
                           result.total_layers, result.output_path ? result.output_path : "N/A",
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("WAAM slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeWaamPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 3: Async pipeline with a custom Lua robot-code generator
// ---------------------------------------------------------------------------
static void OnWaamPipelineComplete(HsBaWaamPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[Async] WAAM done! Layers: {}, Robot program: {}, Time: {:.2f}s", result.total_layers,
                           result.output_path ? result.output_path : "N/A", result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[Async] WAAM FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
        *flag = -1;
    }

    HsBaFreeWaamPipelineResult(&result);
}

static int RunAsyncWaamPipeline()
{
    LogMsg("=== Example 3: WAAM Async Pipeline (custom Lua robot code) ===");

    HsBaWaamPipelineConfig_t cfg = HsBaCreateDefaultWaamConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Provide a custom Lua generator: replaces the built-in brand-specific code.
    cfg.path_lua_script = "scripts/my_waam_path.lua";
    cfg.path_lua_func = "export_waam";
    cfg.output_path = "output/waam_async_output.txt";

    int done_flag = 0;
    HsBaRunWaamPipelineAsync(&cfg, OnProgress, nullptr, OnWaamPipelineComplete, &done_flag);

    while (done_flag == 0)
    {
        // Waiting...
    }

    LogMsg(std::format("WAAM async pipeline finished, status: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Example 4: Spiral / vase mode - one continuous, Z-rising weld bead
// ---------------------------------------------------------------------------
static int RunSpiralWaamPipeline()
{
    LogMsg("=== Example 4: WAAM Spiral Mode (continuous rising bead) ===");

    HsBaWaamPipelineConfig_t cfg = HsBaCreateDefaultWaamConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    cfg.layer_height = 0.6f;
    cfg.first_layer_height = 0.8f;
    cfg.bead_width = 1.0f;
    cfg.robot_type = HSBA_WAAM_ROBOT_ABB;

    // Key: merge the outer walls of all layers into ONE continuous bead that
    // rises in Z - no per-layer travel moves or arc restarts between layers.
    cfg.spiral_mode = 1;
    cfg.output_path = "output/waam_spiral_output.txt";

    HsBaWaamPipelineResult_t result = HsBaRunWaamPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("WAAM spiral slicing OK! Layers: {}, Robot program: {}, Time: {:.2f}s", result.total_layers,
                           result.output_path ? result.output_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("WAAM spiral slicing FAILED: {}",
                           result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeWaamPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer WAAM Pipeline Examples");
    LogMsg("=================================");
    LogMsg("Output format: robot language program (ABB / KUKA / FANUC or custom Lua)");
    LogMsg("");

    RunBasicWaamPipeline();
    RunCustomWaamPipeline();
    RunAsyncWaamPipeline();
    RunSpiralWaamPipeline();

    LogMsg("All examples finished.");
    return 0;
}
