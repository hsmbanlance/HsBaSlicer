// HsBaSlicer.cpp: 定义应用程序的入口点。
//
// 本文件同时作为桌面端可执行程序和 Android 共享库的入口，
// 包含 FDM / SLA / SLS 三种工艺流水线的使用示例（非实际生产入口，仅供示例和测试）。
//

#include "HsBaSlicer.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

#include "DllHsBaSlicer/initialize.h"
#include "DllHsBaSlicer/fdm_pipeline.h"
#include "DllHsBaSlicer/sla_pipeline.h"
#include "DllHsBaSlicer/sls_pipeline.h"
#include "logger/logger.hpp"

using HsBa::Slicer::Log::LoggerSingletone;

// ---------------------------------------------------------------------------
// 日志辅助
// ---------------------------------------------------------------------------
namespace
{
void LogMsg(std::string_view msg)
{
    LoggerSingletone::LogInfo(msg);
}

// 进度回调（三种流水线共用）
void OnProgress(int percent, const char* stage, void* /*user_data*/)
{
    LoggerSingletone::LogInfo(std::format("[{}%] {}", percent, stage));
}
}  // namespace

// ---------------------------------------------------------------------------
// FDM 流水线示例
// ---------------------------------------------------------------------------
static int RunFdmExample()
{
    LogMsg("=== FDM 流水线示例 ===");

    std::filesystem::create_directories("output");

    // 1. 获取默认配置
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();

    // 2. 设置模型（必须）
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. 可选：自定义工艺参数
    cfg.layer_height       = 0.2f;
    cfg.first_layer_height = 0.25f;
    cfg.wall_count         = 3;
    cfg.top_layer_count    = 4;
    cfg.bottom_layer_count = 3;
    cfg.infill_density     = 0.2;
    cfg.fill_mode          = HSBA_FILL_ZIGZAG;
    cfg.enable_support     = 1;
    cfg.overhang_angle     = 45.0f;
    cfg.support_pattern    = HSBA_SUPPORT_PLANE;

    // 4. 输出路径
    cfg.output_path = "output/fdm_example.gcode";

    // 5. 同步运行流水线
    HsBaFdmPipelineResult_t result =
        HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);

    // 6. 处理结果
    if (result.success)
    {
        std::filesystem::path out_path = "output/fdm_example.gcode";
        std::ofstream ofs(out_path, std::ios::binary);
        if (ofs)
        {
            ofs << (result.gcode_content ? result.gcode_content : "");
            ofs.close();
            LogMsg(std::format("FDM 切片成功! 层数: {}, 耗时: {:.2f}s, G-code -> {}",
                               result.total_layers, result.elapsed_seconds, out_path.string()));
        }
        else
        {
            LogMsg(std::format("FDM 切片成功! 层数: {}, 耗时: {:.2f}s (写入文件失败)",
                               result.total_layers, result.elapsed_seconds));
        }
    }
    else
    {
        LogMsg(std::format("FDM 切片失败: {}",
                           result.error_message ? result.error_message : "未知错误"));
    }

    // 7. 释放内存（必须）
    HsBaFreePipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// SLA 流水线示例
// ---------------------------------------------------------------------------
static int RunSlaExample()
{
    LogMsg("=== SLA 流水线示例 ===");

    std::filesystem::create_directories("output");

    // 1. 获取默认 SLA 配置
    HsBaSlaPipelineConfig_t cfg = HsBaCreateDefaultSlaConfig();

    // 2. 设置模型
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. 可选：自定义参数
    cfg.layer_height        = 0.05f;
    cfg.first_layer_height  = 0.1f;
    cfg.bottom_exposure_time = 60.0f;
    cfg.normal_exposure_time = 2.5f;
    cfg.floor_raft_offset   = 2.0f;
    cfg.floor_border_count  = 2;
    cfg.enable_support      = 1;
    cfg.overhang_angle      = 45.0f;
    cfg.support_pattern     = HSBA_SLA_SUPPORT_SACRIFICIAL;

    // 4. 输出路径
    cfg.output_path = "output/sla_example.zip";

    // 5. 同步运行
    HsBaSlaPipelineResult_t result =
        HsBaRunSlaPipeline(&cfg, OnProgress, nullptr);

    // 6. 处理结果
    if (result.success)
    {
        LogMsg(std::format("SLA 切片成功! 层数: {}, 导出: {}, 耗时: {:.2f}s",
                           result.total_layers,
                           result.export_path ? result.export_path : "N/A",
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLA 切片失败: {}",
                           result.error_message ? result.error_message : "未知错误"));
    }

    // 7. 释放内存
    HsBaFreeSlaPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// SLS 流水线示例
// ---------------------------------------------------------------------------
static int RunSlsExample()
{
    LogMsg("=== SLS 流水线示例 ===");

    std::filesystem::create_directories("output");

    // 1. 获取默认 SLS 配置
    HsBaSlsPipelineConfig_t cfg = HsBaCreateDefaultSlsConfig();

    // 2. 设置模型
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. 可选：自定义激光参数
    cfg.layer_height       = 0.1f;
    cfg.first_layer_height = 0.15f;
    cfg.laser_power        = 30.0f;
    cfg.scan_speed         = 2000.0f;
    cfg.hatch_spacing      = 0.15f;
    cfg.hatch_rotation     = 90.0f;
    cfg.bed_temperature    = 180.0f;

    // 4. Lua 导出脚本（SLS 必须指定，无标准输出格式）
    cfg.export_lua_script = "scripts/my_sls_export.lua";
    cfg.export_lua_func   = "export_sls";

    // 5. 输出路径
    cfg.output_path = "output/sls_example.zip";

    // 6. 同步运行
    HsBaSlsPipelineResult_t result =
        HsBaRunSlsPipeline(&cfg, OnProgress, nullptr);

    // 7. 处理结果
    if (result.success)
    {
        LogMsg(std::format("SLS 切片成功! 层数: {}, 导出: {}, 耗时: {:.2f}s",
                           result.total_layers,
                           result.export_path ? result.export_path : "N/A",
                           result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("SLS 切片失败: {}",
                           result.error_message ? result.error_message : "未知错误"));
    }

    // 8. 释放内存
    HsBaFreeSlsPipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// 运行全部示例（供桌面端 main 和 Android JNI 共用）
// ---------------------------------------------------------------------------
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
// 平台入口
// ---------------------------------------------------------------------------
#if defined(ANDROID)
// Android：导出 JNI 函数，供 Java 直接调用
// JNI 命名约定：Java_<包名>_<类名>_<方法名>（包名中的 '.' 替换为 '_'）
extern "C" void Java_com_hsmbanlance_hsbaslicer_example_MainActivity_runPipelineExamples(
    void* /*env*/, void* /*thiz*/)
{
    initialize();

    // 运行流水线示例
    RunAllPipelineExamples();
}

#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
// iOS：导出 C 函数，供 Swift / Objective-C 直接调用
extern "C" void HsBaRunPipelineExamples()
{
    initialize();

    // 运行流水线示例
    RunAllPipelineExamples();
}
#else
// macOS：标准 main 入口
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

    // 运行流水线示例
    RunAllPipelineExamples();

    return 0;
}
#endif  // TARGET_OS_IPHONE

#else
// 桌面端（Windows / Linux）：标准 main 入口
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

    // 运行流水线示例
    RunAllPipelineExamples();

    return 0;
}
#endif  // ANDROID / __APPLE__ / else
