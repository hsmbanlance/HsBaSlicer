/**
 * @file main.cpp
 * @brief HsBaSlicer FDM Pipeline 使用示例
 *
 * 演示如何配置并运行 FDM 切片流水线（同步 & 异步），
 * 包括顶层/底层实心层数、中间层填充率、Lua 自定义支撑/填充等全部功能。
 *
 * 支持平台: Windows / Linux / macOS / Android / iOS
 */

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

#include "fdm_pipeline.h"

#ifndef HSBA_GAME_CONSOLE
#include "logger/logger.hpp"
using HsBa::Slicer::Log::LoggerSingletone;
#endif

// ---------------------------------------------------------------------------
// 跨平台日志辅助
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

// 进度回调
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
// 示例 1: 基本用法 —— 使用默认配置运行流水线
// ---------------------------------------------------------------------------
static int RunBasicPipeline()
{
    LogMsg("=== 示例 1: 基本流水线 ===");

    std::filesystem::create_directories("output");

    // 1. 获取默认配置
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();

    // 2. 设置模型（必须）
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 3. 同步运行流水线
    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);

    // 4. 检查结果
    if (result.success)
    {
        std::filesystem::path out_path = "output/stanford_bunny_basic.gcode";
        std::ofstream ofs(out_path, std::ios::binary);
        if (ofs)
        {
            ofs << (result.gcode_content ? result.gcode_content : "");
            ofs.close();
            LogMsg(std::format("切片成功! 总层数: {}, 耗时: {:.2f} 秒，G-code 已写入 {}", result.total_layers,
                               result.elapsed_seconds, out_path.string()));
        }
        else
        {
            LogMsg(std::format("切片成功! 总层数: {}, 耗时: {:.2f} 秒，但写入 G-code 失败", result.total_layers,
                               result.elapsed_seconds));
        }
    }
    else
    {
        LogMsg(std::format("切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    // 5. 释放结果内存（必须）
    HsBaFreePipelineResult(&result);

    return result.success;
}

// ---------------------------------------------------------------------------
// 示例 2: 自定义工艺参数 —— 顶层/底层/填充率
// ---------------------------------------------------------------------------
static int RunCustomPipeline()
{
    LogMsg("=== 示例 2: 自定义工艺参数 ===");

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();

    // 模型
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 切片
    cfg.layer_height = 0.2f;         // 层高 0.2mm
    cfg.first_layer_height = 0.25f;  // 首层 0.25mm

    // 填充 —— 顶层/底层实心，中间层稀疏
    cfg.fill_spacing = 0.4;  // 填充间距
    cfg.fill_mode = HSBA_FILL_ZIGZAG;
    cfg.fill_angle = 45.0;
    cfg.wall_count = 3;          // 壁厚 3 圈
    cfg.top_layer_count = 5;     // 顶层 5 层实心
    cfg.bottom_layer_count = 4;  // 底层 4 层实心
    cfg.infill_density = 0.15;   // 中间层填充率 15%

    // 支撑
    cfg.enable_support = 1;
    cfg.overhang_angle = 50.0f;  // 悬垂阈值 50°
    cfg.support_pattern = HSBA_SUPPORT_TREE;

    // 路径
    cfg.line_width = 0.4f;
    cfg.print_speed = 60.0f;
    cfg.travel_speed = 120.0f;
    cfg.extrusion_multiplier = 1.0f;

    // 输出
    cfg.output_path = "output/stanford_bunny.gcode";

    // 运行
    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        std::filesystem::path out_path = "output/stanford_bunny_custom.gcode";
        std::ofstream ofs(out_path, std::ios::binary);
        if (ofs)
        {
            ofs << (result.gcode_content ? result.gcode_content : "");
            ofs.close();
            LogMsg(std::format("自定义切片完成! 层数: {}, 耗时: {:.2f} 秒，G-code 已写入 {}", result.total_layers,
                               result.elapsed_seconds, out_path.string()));
        }
        else
        {
            LogMsg(std::format("自定义切片完成! 层数: {}, 耗时: {:.2f} 秒，但写入 G-code 失败", result.total_layers,
                               result.elapsed_seconds));
        }
    }
    else
    {
        LogMsg(std::format("切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    HsBaFreePipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// 示例 3: Lua 自定义支撑与填充
// ---------------------------------------------------------------------------
static int RunLuaCustomPipeline()
{
    LogMsg("=== 示例 3: Lua 自定义支撑/填充 ===");

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();

    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";

    // 基本工艺
    cfg.layer_height = 0.2f;
    cfg.wall_count = 3;
    cfg.top_layer_count = 4;
    cfg.bottom_layer_count = 3;
    cfg.infill_density = 0.2;

    // Lua 自定义支撑: 指定脚本路径和函数名
    cfg.support_lua_script = "scripts/my_support.lua";
    cfg.support_lua_func = "generate_support";

    // Lua 自定义填充: 指定脚本路径和函数名
    cfg.infill_lua_script = "scripts/my_infill.lua";
    cfg.infill_lua_func = "generate_fill";

    // 运行
    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);

    if (result.success)
    {
        std::filesystem::path out_path = "output/stanford_bunny_lua.gcode";
        std::ofstream ofs(out_path, std::ios::binary);
        if (ofs)
        {
            ofs << (result.gcode_content ? result.gcode_content : "");
            ofs.close();
            LogMsg(
                std::format("Lua 自定义切片完成! 层数: {}，G-code 已写入 {}", result.total_layers, out_path.string()));
        }
        else
        {
            LogMsg(std::format("Lua 自定义切片完成! 层数: {}，但写入 G-code 失败", result.total_layers));
        }
    }
    else
    {
        LogMsg(std::format("切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    HsBaFreePipelineResult(&result);
    return result.success;
}

// ---------------------------------------------------------------------------
// 示例 4: 异步流水线
// ---------------------------------------------------------------------------

// 异步完成回调
static void OnPipelineComplete(HsBaFdmPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);

    if (result.success)
    {
        LogMsg(std::format("[异步] 切片完成! 层数: {}, 耗时: {:.2f} 秒", result.total_layers, result.elapsed_seconds));
        *flag = 1;
    }
    else
    {
        LogMsg(std::format("[异步] 切片失败: {}", result.error_message ? result.error_message : "未知错误"));
        *flag = -1;
    }

    HsBaFreePipelineResult(&result);
}

static int RunAsyncPipeline()
{
    LogMsg("=== 示例 4: 异步流水线 ===");

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.top_layer_count = 3;
    cfg.bottom_layer_count = 3;
    cfg.infill_density = 0.2;

    int done_flag = 0;

    // 异步启动（非阻塞）
    HsBaRunFdmPipelineAsync(&cfg, OnProgress, nullptr, OnPipelineComplete, &done_flag);

    // 实际应用中此处可执行其他工作，或等待 done_flag 变化
    while (done_flag == 0)
    {
        // 模拟等待...
    }

    LogMsg(std::format("异步流水线结束, 状态: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer FDM Pipeline 示例");
    LogMsg("==============================");

    RunBasicPipeline();
    RunCustomPipeline();
    RunLuaCustomPipeline();
    RunAsyncPipeline();

    LogMsg("全部示例执行完毕。");
    return 0;
}
