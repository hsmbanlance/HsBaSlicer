/**
 * @file main.cpp
 * @brief HsBaSlicer 自定义 Lua 流水线（Custom Lua Pipeline）使用示例
 *
 * 内置的 FDM / SLA / SLS 流水线的阶段顺序在 C++ 中固定，Lua 只能替换个别阶段
 * （支撑、填充、地板、导出）。Custom 流水线则把整条工作流交给 Lua：C++ 侧只
 * 提供一个封装了全部流水线算子的 Lua 环境（全局表 `HsBa`）与一个入口函数名，
 * 做什么、按什么顺序做，完全由脚本决定。
 *
 * `HsBa` 算子表（参数与返回值均为 Lua 表 / 字符串，坐标单位 mm）:
 *   progress(p[, stage])          向 C++ 进度回调上报百分比
 *   setLayers(n) / setOutputPath(p)  把层数 / 输出路径回报给 C++ 结果结构
 *   readFile(p) / writeFile(p, s)    文本读写
 *   loadModel(n, p) / modelInfo(n) / translateModel / rotateModel / scaleModel
 *   removeModel(n) / modelNames()
 *   layerCount(n, lh, flh) / layerZ(i, lh, flh)
 *   slice(n, z) / sliceUnsafe(n, z) / toInt(polys) / toDouble(polys)
 *   fill(polys[, {spacing,mode,angle,borderCount}])
 *   fdmSupport(layers, cfg) / slaSupport(layers, cfg) / floor(bottom, cfg)
 *   toGcode(layers, cfg) -> 字符串
 *   saveSlaPackage({...}) / saveSlsPackage({...}) / renderImage(polys,w,h,p)
 *
 * 同时脚本还能使用项目注册池中的 PolygonOperations / Support / PolygonFill /
 * PathOptimize / Zipper / Cipher / SQLiteAdapter 等库。
 *
 * 支持平台: Windows / Linux / macOS / Android / iOS
 */

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <thread>

#include "custom_pipeline.h"

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

// 进度回调：由 Lua 脚本中的 HsBa.progress() 驱动
void OnProgress(int percent, const char* stage, void* /*user_data*/)
{
#ifndef HSBA_GAME_CONSOLE
    LoggerSingletone::LogInfo(std::format("[{}%] {}", percent, stage ? stage : ""));
#else
    (void)percent;
    (void)stage;
#endif
}

// 打印结果（Lua 脚本回报的层数 / 输出路径 / 返回值字符串）
void LogResult(const HsBaCustomPipelineResult_t& result)
{
    if (result.success)
    {
        LogMsg(std::format("流水线成功! 层数: {}, 输出: {}, 耗时: {:.2f} 秒", result.total_layers,
                           result.output_path ? result.output_path : "N/A", result.elapsed_seconds));
        if (result.result_string && *result.result_string)
            LogMsg(std::format("Lua 返回值: {}", result.result_string));
    }
    else
    {
        LogMsg(std::format("流水线失败: {}", result.error_message ? result.error_message : "未知错误"));
    }
}
}  // namespace

// ---------------------------------------------------------------------------
// 示例 1: 整条 FDM 工作流写在 Lua 脚本文件里（同步执行）
// ---------------------------------------------------------------------------
static int RunLuaDefinedFdmPipeline()
{
    LogMsg("=== 示例 1: 纯 Lua FDM 流水线（脚本文件） ===");

    std::filesystem::create_directories("output");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();

    // 1. 流水线定义：脚本文件里的 run_pipeline() 决定所有阶段
    cfg.pipeline_lua_script = "scripts/my_fdm_pipeline.lua";
    cfg.entry_func = "run_pipeline";  // 可省略，默认即 "run_pipeline"

    // 2. 模型与输出路径：以全局变量的形式暴露给 Lua
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_fdm_pipeline.gcode";

    // 3. 机台参数通过内联 Lua 源码预置：内联源码先于脚本文件执行，
    //    因此脚本可以直接读取这里定义的 machine 表
    cfg.pipeline_lua_source = "machine = { layer_height = 0.2, first_layer_height = 0.25, "
                              "fill_spacing = 0.45, wall_count = 3, enable_support = false, "
                              "print_speed = 80, firmware = 'marlin' }";

    // 4. 任意 JSON 字符串，Lua 中通过全局 pipeline_config 读取
    cfg.config_json = "{ \"machine\": \"HsBa-X1\", \"material\": \"PLA\" }";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    const int ok = result.success;

    HsBaFreeCustomPipelineResult(&result);  // 必须释放
    return ok;
}

// ---------------------------------------------------------------------------
// 示例 2: 不落地脚本文件 —— 流水线定义全部来自内联 Lua 源码
// ---------------------------------------------------------------------------
static int RunInlineLuaPipeline()
{
    LogMsg("=== 示例 2: 内联 Lua 源码流水线（无脚本文件） ===");

    static const char* kInlinePipeline =
        "function run_pipeline()\n"
        "    HsBa.loadModel(model_name, model_path)\n"
        "    local lh, flh = 0.2, 0.25\n"
        "    local n = HsBa.layerCount(model_name, lh, flh)\n"
        "    HsBa.setLayers(n)\n"
        "    local zb = HsBa.modelInfo(model_name).bbox_min.z\n"
        "    local report = { 'layer,zHeight,contours,area' }\n"
        "    local total_area = 0\n"
        "    for i = 0, n - 1 do\n"
        "        local z = HsBa.layerZ(i, lh, flh)\n"
        "        local polys = HsBa.slice(model_name, zb + z)\n"
        "        local area = 0\n"
        "        for _, p in ipairs(polys) do area = area + math.abs(PolygonOperations.area(p)) end\n"
        "        total_area = total_area + area\n"
        "        report[#report + 1] = string.format('%d,%.3f,%d,%.3f', i, z, #polys, area)\n"
        "        if i % 25 == 0 then HsBa.progress(math.floor(90 * i / math.max(n, 1)), '统计层面积') end\n"
        "    end\n"
        "    HsBa.writeFile(output_path, table.concat(report, '\\n') .. '\\n')\n"
        "    HsBa.setOutputPath(output_path)\n"
        "    HsBa.removeModel(model_name)\n"
        "    HsBa.progress(100, '完成')\n"
        "    return string.format('共 %d 层, 累计截面积 %.2f mm^2', n, total_area)\n"
        "end\n";

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();
    cfg.pipeline_lua_source = kInlinePipeline;  // 只给内联源码，不给脚本文件
    cfg.entry_func = "run_pipeline";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_layer_report.csv";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    if (result.success)
        LogMsg(std::format("层面积报表已写入: {}", result.output_path ? result.output_path : cfg.output_path));

    const int ok = result.success;
    HsBaFreeCustomPipelineResult(&result);
    return ok;
}

// ---------------------------------------------------------------------------
// 示例 3: 切换到另一条 Lua 工作流（SLA 打包），并异步执行
// ---------------------------------------------------------------------------
namespace
{
void OnCustomPipelineComplete(HsBaCustomPipelineResult_t result, void* user_data)
{
    int* flag = static_cast<int*>(user_data);
    LogMsg("[异步] Custom 流水线回调:");
    LogResult(result);
    *flag = result.success ? 1 : -1;
    HsBaFreeCustomPipelineResult(&result);
}
}  // namespace

static int RunAsyncLuaPipeline()
{
    LogMsg("=== 示例 3: 纯 Lua SLA 打包流水线（异步） ===");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();
    cfg.pipeline_lua_script = "scripts/my_sla_pipeline.lua";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_sla_pipeline.zip";
    // 同一个入口函数名、同一套算子，换一条脚本就是完全不同的流水线；
    // 这里用内联源码覆盖脚本内的默认工艺参数，并把层数压到示例可接受的规模
    cfg.pipeline_lua_source = "machine = { layer_height = 0.3, first_layer_height = 0.4, "
                              "support_density = 0.1, image_width = 800, image_height = 800 }";

    int done_flag = 0;
    HsBaRunCustomPipelineAsync(&cfg, OnProgress, nullptr, OnCustomPipelineComplete, &done_flag);

    // 异步调用立即返回，这里等待完成回调（实际应用可在回调中刷新 UI）
    while (done_flag == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LogMsg(std::format("Custom 异步流水线结束, 状态: {}", done_flag));
    return done_flag > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer 自定义 Lua 流水线示例");
    LogMsg("================================");
    LogMsg("整条工作流（阶段与顺序）由 Lua 脚本决定，C++ 只提供算子环境");
    LogMsg("");

    std::filesystem::create_directories("output");

    RunLuaDefinedFdmPipeline();
    RunInlineLuaPipeline();
    RunAsyncLuaPipeline();

    LogMsg("全部示例执行完毕。");
    return 0;
}
