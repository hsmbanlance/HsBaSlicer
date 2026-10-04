/**
 * @file main.cpp
 * @brief HsBaSlicer LOM Pipeline usage examples
 *
 * Demonstrates how to configure and run the LOM slicing pipeline (sync & async),
 * with Lua-driven export (zip archive + database registration).
 *
 * LOM (Laminated Object Manufacturing) is a sheet-bonding process:
 *   - Each "layer" is a physical sheet thickness; contours are cut then bonded
 *   - Cut / bond parameters are handed to the Lua export via the config JSON
 *   - Output format is entirely controlled by the Lua export script
 *
 * Platforms: Windows / Linux / macOS / Android / iOS
 */

#include <format>
#include <string>
#include <string_view>

#include "lom_pipeline.h"

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
// Example 1: Basic usage - run LOM pipeline with minimal config
// ---------------------------------------------------------------------------
static int RunBasicLomPipeline()
{
    LogMsg("=== Example 1: LOM Basic Pipeline ===");

    HsBaLomPipelineConfig_t cfg = HsBaCreateDefaultLomConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    cfg.export_lua_script = "scripts/my_lom_export.lua";
    cfg.export_lua_func = "export_lom";
    cfg.output_path = "output/lom_basic_output.zip";

    HsBaLomPipelineResult_t result = HsBaRunLomPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("LOM slicing OK! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("LOM slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeLomPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 2: Custom cut / bond parameters
// ---------------------------------------------------------------------------
static int RunCustomLomPipeline()
{
    LogMsg("=== Example 2: LOM Custom Cut/Bond Parameters ===");

    HsBaLomPipelineConfig_t cfg = HsBaCreateDefaultLomConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // Sheet thickness
    cfg.layer_height = 0.25f;
    cfg.first_layer_height = 0.25f;

    // Cutting parameters
    cfg.cut_speed = 250.0f;
    cfg.cut_margin = 0.8f;
    cfg.cut_power = 0.9f;
    cfg.seal_contour = 1;
    cfg.cut_mode = HSBA_LOM_CUT_CONTOUR;

    // Bonding parameters
    cfg.bond_temperature = 160.0f;
    cfg.bond_pressure = 1.5f;
    cfg.bond_time = 6.0f;

    cfg.export_lua_script = "scripts/my_lom_export.lua";
    cfg.export_lua_func = "export_lom";
    cfg.output_path = "output/lom_custom_output.zip";

    HsBaLomPipelineResult_t result = HsBaRunLomPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        LogMsg(std::format("LOM custom slicing done! Layers: {}, Time: {:.2f}s", result.total_layers,
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("LOM slicing FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
    }

    HsBaFreeLomPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Example 3: Async pipeline
// ---------------------------------------------------------------------------
static void OnLomPipelineComplete(HsBaLomPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[Async] LOM done! Layers: {}, Export: {}, Time: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[Async] LOM FAILED: {}", result.error_message ? result.error_message : "Unknown error"));
        *flag = -1;
    }

    HsBaFreeLomPipelineResult(&result);
}

static int RunAsyncLomPipeline()
{
    LogMsg("=== Example 3: LOM Async Pipeline ===");

    HsBaLomPipelineConfig_t cfg = HsBaCreateDefaultLomConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.export_lua_script = "scripts/my_lom_export.lua";
    cfg.export_lua_func = "export_lom";
    cfg.output_path = "output/lom_async_output.zip";

    int done_flag = 0;
    HsBaRunLomPipelineAsync(&cfg, OnProgress, nullptr, OnLomPipelineComplete, &done_flag);

    while (done_flag == 0)
    {
        // Waiting...
    }

    LogMsg(std::format("LOM async pipeline finished, status: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer LOM Pipeline Examples");
    LogMsg("================================");
    LogMsg("Export format: Lua-driven (zip + database registration)");
    LogMsg("");

    RunBasicLomPipeline();
    RunCustomLomPipeline();
    RunAsyncLomPipeline();

    LogMsg("All examples finished.");
    return 0;
}
