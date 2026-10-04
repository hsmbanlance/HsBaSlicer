/**
 * @file HsBaSlicer.cpp
 * @brief Application entry point.
 *
 * This file serves as both the desktop executable and the Android shared-library entry,
 * containing usage examples of the FDM / SLA / SLS pipelines (not a real production entry,
 * for examples and testing only).
 */

#include "HsBaSlicer.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

#include "DllHsBaSlicer/fdm_pipeline.h"
#include "DllHsBaSlicer/initialize.h"
#include "DllHsBaSlicer/sla_pipeline.h"
#include "DllHsBaSlicer/sls_pipeline.h"
#include "logger/logger.hpp"

using HsBa::Slicer::Log::LoggerSingletone;

// ---------------------------------------------------------------------------
// Logging helpers
// ---------------------------------------------------------------------------
namespace
{
/// @brief Write an informational message through the singleton logger.
void LogMsg(std::string_view msg)
{
    LoggerSingletone::LogInfo(msg);
}

// Progress callback (shared by the three pipelines)
/// @brief Log a pipeline progress update as an informational message.
void OnProgress(int percent, const char* stage, void* /*user_data*/)
{
    LoggerSingletone::LogInfo(std::format("[{}%] {}", percent, stage));
}
}  // namespace

// ---------------------------------------------------------------------------
// FDM pipeline example
// ---------------------------------------------------------------------------
/// @brief Run the FDM pipeline example and return a process exit code.
static int RunFdmExample()
{
    LogMsg("=== FDM 流水线示例 ===");

    std::filesystem::create_directories("output");

    // 1. Get the default configuration
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();

    // 2. Set the model (required)
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. Optional: customize process parameters
    cfg.layer_height = 0.2f;
    cfg.first_layer_height = 0.25f;
    cfg.wall_count = 3;
    cfg.top_layer_count = 4;
    cfg.bottom_layer_count = 3;
    cfg.infill_density = 0.2;
    cfg.fill_mode = HSBA_FILL_ZIGZAG;
    cfg.enable_support = 1;
    cfg.overhang_angle = 45.0f;
    cfg.support_pattern = HSBA_SUPPORT_PLANE;

    // 4. Output path
    cfg.output_path = "output/fdm_example.gcode";

    // 5. Run the pipeline synchronously
    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);

    // 6. Handle the result
    if (result.success)
    {
        std::filesystem::path out_path = "output/fdm_example.gcode";
        std::ofstream ofs(out_path, std::ios::binary);
        if (ofs)
        {
            ofs << (result.gcode_content ? result.gcode_content : "");
            ofs.close();
            LogMsg(std::format("FDM 切片成功! 层数: {}, 耗时: {:.2f}s, G-code -> {}", result.total_layers,
                               result.elapsed_seconds, out_path.string()));
        }
        else
        {
            LogMsg(std::format("FDM 切片成功! 层数: {}, 耗时: {:.2f}s (写入文件失败)", result.total_layers,
                               result.elapsed_seconds));
        }
    }
    else
    {
        LogMsg(std::format("FDM 切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    // 7. Free memory (required)
    HsBaFreePipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// SLA pipeline example
// ---------------------------------------------------------------------------
/// @brief Run the SLA pipeline example and return a process exit code.
static int RunSlaExample()
{
    LogMsg("=== SLA 流水线示例 ===");

    std::filesystem::create_directories("output");

    // 1. Get the default SLA configuration
    HsBaSlaPipelineConfig_t cfg = HsBaCreateDefaultSlaConfig();

    // 2. Set the model
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. Optional: customize parameters
    cfg.layer_height = 0.05f;
    cfg.first_layer_height = 0.1f;
    cfg.bottom_exposure_time = 60.0f;
    cfg.normal_exposure_time = 2.5f;
    cfg.floor_raft_offset = 2.0f;
    cfg.floor_border_count = 2;
    cfg.enable_support = 1;
    cfg.overhang_angle = 45.0f;
    cfg.support_pattern = HSBA_SLA_SUPPORT_SACRIFICIAL;

    // 4. Output path
    cfg.output_path = "output/sla_example.zip";

    // 5. Run synchronously
    HsBaSlaPipelineResult_t result = HsBaRunSlaPipeline(&cfg, OnProgress, nullptr);

    // 6. Handle the result
    if (result.success)
    {
        LogMsg(std::format("SLA 切片成功! 层数: {}, 导出: {}, 耗时: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLA 切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    // 7. Free memory
    HsBaFreeSlaPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// SLS pipeline example
// ---------------------------------------------------------------------------
/// @brief Run the SLS pipeline example and return a process exit code.
static int RunSlsExample()
{
    LogMsg("=== SLS 流水线示例 ===");

    std::filesystem::create_directories("output");

    // 1. Get the default SLS configuration
    HsBaSlsPipelineConfig_t cfg = HsBaCreateDefaultSlsConfig();

    // 2. Set the model
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. Optional: customize laser parameters
    cfg.layer_height = 0.1f;
    cfg.first_layer_height = 0.15f;
    cfg.laser_power = 30.0f;
    cfg.scan_speed = 2000.0f;
    cfg.hatch_spacing = 0.15f;
    cfg.hatch_rotation = 90.0f;
    cfg.bed_temperature = 180.0f;

    // 4. Lua export script (required for SLS, which has no standard output format)
    cfg.export_lua_script = "scripts/my_sls_export.lua";
    cfg.export_lua_func = "export_sls";

    // 5. Output path
    cfg.output_path = "output/sls_example.zip";

    // 6. Run synchronously
    HsBaSlsPipelineResult_t result = HsBaRunSlsPipeline(&cfg, OnProgress, nullptr);

    // 7. Handle the result
    if (result.success)
    {
        LogMsg(std::format("SLS 切片成功! 层数: {}, 导出: {}, 耗时: {:.2f}s", result.total_layers,
                           result.export_path ? result.export_path : "N/A", result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLS 切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    // 8. Free memory
    HsBaFreeSlsPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// Run all examples (shared by the desktop main and the Android JNI entry)
// ---------------------------------------------------------------------------
/// @brief Execute the FDM, SLA and SLS pipeline examples in sequence.
static void RunAllPipelineExamples()
{
    LogMsg("================================================");
    LogMsg("HsBaSlicer 流水线使用示例（非生产入口，仅供测试）");
    LogMsg("================================================");

    RunFdmExample();
    RunSlaExample();
    RunSlsExample();

    LogMsg("全部示例执行完毕。");
}

// ---------------------------------------------------------------------------
// Platform entry
// ---------------------------------------------------------------------------
#if defined(ANDROID)
// Android: export a JNI function for direct calls from Java
// JNI naming convention: Java_<package>_<Class>_<method> (dots in the package replaced by '_')
/// @brief JNI entry invoked from the Android example Activity to run the pipeline examples.
extern "C" void Java_com_hsmbanlance_hsbaslicer_example_MainActivity_runPipelineExamples(void* /*env*/, void* /*thiz*/)
{
    initialize();

    // Run the pipeline examples
    RunAllPipelineExamples();
}

#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// iOS: export a C function for direct calls from Swift / Objective-C
/// @brief C entry exported for Swift / Objective-C to initialize and run the pipeline examples.
extern "C" void HsBaRunPipelineExamples()
{
    initialize();

    // Run the pipeline examples
    RunAllPipelineExamples();
}
#else
// macOS: standard main entry
/// @brief macOS application entry point: initialize logging and run the pipeline examples.
int main()
{
    auto log = HsBa::Slicer::Log::LoggerSingletone::GetInstance();
    using namespace HsBa::Slicer::Log::LogLiterals;
    if (log->UseLogFile())
    {
        "use log file"_log_info();
    }
    else
    {
        "not use log file"_log_warning();
    }

    initialize();
    "initialize completed"_log_info();

    // Run the pipeline examples
    RunAllPipelineExamples();

    return 0;
}
#endif  // TARGET_OS_IPHONE

#else
// Desktop (Windows / Linux): standard main entry
/// @brief Desktop application entry point: initialize logging and run the pipeline examples.
int main()
{
    auto log = HsBa::Slicer::Log::LoggerSingletone::GetInstance();
    using namespace HsBa::Slicer::Log::LogLiterals;
    if (log->UseLogFile())
    {
        "use log file"_log_info();
    }
    else
    {
        "not use log file"_log_warning();
    }

    initialize();
    "initialize completed"_log_info();

    // Run the pipeline examples
    RunAllPipelineExamples();

    return 0;
}
#endif  // ANDROID / __APPLE__ / else
