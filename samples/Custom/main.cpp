/**
 * @file main.cpp
 * @brief HsBaSlicer 自定义 Lua 流水线（Custom Lua Pipeline）使用示例
 *
 * 内置的 FDM / SLA / SLS 流水线的阶段顺序在 C++ 中固定，Lua 只能替换个别阶段
 * （支撑、填充、地板、导出）。Custom 流水线则把整条工作流交给 Lua：C++ 侧只
 * 提供一个封装了全部流水线算子的 Lua 环境（全局表 `HsBa`）与一个入口函数名，
 * 做什么、按什么顺序做，完全由脚本决定。示例 1（FDM）、示例 2（SLS）与示例 4
 * （SLA）分别用三条脚本演示如何在 Custom 流水线里重建三种 3D 打印模式。
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
 *   saveSlaPackage({...}) / saveSlsPackage({...}) / saveWaamPackage({...}) / renderImage(polys,w,h,p)
 *
 * 同时脚本还能使用项目注册池中的 PolygonOperations / Support / PolygonFill /
 * PathOptimize / Zipper / Cipher / SQLiteAdapter 等库。
 *
 * 调用方式共四种：脚本文件（示例 1/2 的 FDM、SLS）、内联源码（示例 3），异步执行
 * （示例 4），以及示例 5 演示的 Protobuf
 * 字节流（跨进程 / 跨语言：请求与结果都以 `proto/custom_pipeline.proto` 定义
 * 的 wire 格式传递，C++ / C# / Java / Python 只要能序列化该 schema 即可调用）。
 *
 * 支持平台: Windows / Linux / macOS / Android / iOS
 */

#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "custom_pipeline.h"
#include "pipeline_convert.h"

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
// 示例 2: 纯 Lua SLS（粉末床）打包流水线（脚本文件，同步）
//
// SLS 无需支撑与地板，输出格式完全由导出脚本决定：这里用 saveSlsPackage
// 把逐层轮廓打包为 zip，并交给内嵌的 my_sls_export.lua 完成归档与数据库注册
// ---------------------------------------------------------------------------
static int RunLuaDefinedSlsPipeline()
{
    LogMsg("=== 示例 2: 纯 Lua SLS 打包流水线（脚本文件） ===");

    std::filesystem::create_directories("output");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();

    cfg.pipeline_lua_script = "scripts/my_sls_pipeline.lua";
    cfg.entry_func = "run_pipeline";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_sls_pipeline.zip";

    // 激光 / 扫描 / 粉末床工艺参数通过内联源码预置，脚本内的 param() 会优先读取
    cfg.pipeline_lua_source = "machine = { layer_height = 0.1, first_layer_height = 0.15, "
                              "laser_power = 45.0, scan_speed = 3500.0, hatch_spacing = 0.12, "
                              "hatch_rotation = 67.0, bed_temperature = 175.0 }";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    const int ok = result.success;

    HsBaFreeCustomPipelineResult(&result);  // 必须释放
    return ok;
}

// ---------------------------------------------------------------------------
// 示例 2b: 纯 Lua SLM / LOM / 3DP / WAAM 流水线
//
// 这四条脚本演示 Custom 流水线如何覆盖其余 3D 打印模式：SLM / LOM / 3DP 与 SLS
// 同属“切片 + zip 打包”族，复用 saveSlsPackage；WAAM 则输出机器人程序，调用
// saveWaamPackage。换一条脚本就是一条完全不同的流水线。
// ---------------------------------------------------------------------------
static int RunLuaDefinedSlmPipeline()
{
    LogMsg("=== 示例 2b-1: 纯 Lua SLM 打包流水线（脚本文件） ===");

    std::filesystem::create_directories("output");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();
    cfg.pipeline_lua_script = "scripts/my_slm_pipeline.lua";
    cfg.entry_func = "run_pipeline";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_slm_pipeline.zip";
    cfg.pipeline_lua_source = "machine = { layer_height = 0.06, first_layer_height = 0.08, "
                              "laser_power = 220.0, scan_speed = 1100.0, hatch_spacing = 0.1, "
                              "hatch_rotation = 67.0, material = 'TITANIUM', bed_temperature = 100.0 }";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    const int ok = result.success;
    HsBaFreeCustomPipelineResult(&result);
    return ok;
}

static int RunLuaDefinedLomPipeline()
{
    LogMsg("=== 示例 2b-2: 纯 Lua LOM 打包流水线（脚本文件） ===");

    std::filesystem::create_directories("output");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();
    cfg.pipeline_lua_script = "scripts/my_lom_pipeline.lua";
    cfg.entry_func = "run_pipeline";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_lom_pipeline.zip";
    cfg.pipeline_lua_source = "machine = { layer_height = 0.2, cut_speed = 320.0, cut_margin = 0.5, "
                              "laser_power = 0.85, bond_temperature = 150.0, cut_mode = 'CONTOUR' }";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    const int ok = result.success;
    HsBaFreeCustomPipelineResult(&result);
    return ok;
}

static int RunLuaDefinedTdpPipeline()
{
    LogMsg("=== 示例 2b-3: 纯 Lua 3DP 打包流水线（脚本文件） ===");

    std::filesystem::create_directories("output");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();
    cfg.pipeline_lua_script = "scripts/my_tdp_pipeline.lua";
    cfg.entry_func = "run_pipeline";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_tdp_pipeline.zip";
    cfg.pipeline_lua_source = "machine = { layer_height = 0.1, first_layer_height = 0.12, "
                              "head_count = 256, drop_spacing = 0.04, binder_saturation = 0.7, "
                              "curing_time = 1.5, bed_temperature = 45.0, binder_mode = 'SINGLE' }";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    const int ok = result.success;
    HsBaFreeCustomPipelineResult(&result);
    return ok;
}

static int RunLuaDefinedWaamPipeline()
{
    LogMsg("=== 示例 2b-4: 纯 Lua WAAM 机器人路径流水线（脚本文件） ===");

    std::filesystem::create_directories("output");

    HsBaCustomPipelineConfig_t cfg = HsBaCreateDefaultCustomConfig();
    cfg.pipeline_lua_script = "scripts/my_waam_pipeline.lua";
    cfg.entry_func = "run_pipeline";
    cfg.model_name = "stanford_bunny";
    cfg.model_path = "models/stanford_bunny.stl";
    cfg.output_path = "output/custom_waam_pipeline.txt";
    // robot_type 0=ABB, 1=KUKA, 2=FANUC（内置代码生成，无需导出脚本）
    cfg.pipeline_lua_source = "machine = { layer_height = 0.8, first_layer_height = 1.0, "
                              "bead_width = 1.2, travel_speed = 8.0, arc_current = 200.0, "
                              "arc_voltage = 24.0, wire_feed_speed = 6.0, gas_flow_rate = 16.0, "
                              "welding_process = 0, robot_type = 0 }";

    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);
    const int ok = result.success;
    HsBaFreeCustomPipelineResult(&result);
    return ok;
}

// ---------------------------------------------------------------------------
// 示例 3: 不落地脚本文件 —— 流水线定义全部来自内联 Lua 源码
// ---------------------------------------------------------------------------
static int RunInlineLuaPipeline()
{
    LogMsg("=== 示例 3: 内联 Lua 源码流水线（无脚本文件） ===");

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
// 示例 4: 切换到另一条 Lua 工作流（SLA 打包），并异步执行
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
    LogMsg("=== 示例 4: 纯 Lua SLA 打包流水线（异步） ===");

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
// 极简 Protobuf wire 编解码
//
// 只覆盖示例 5 用到的 varint / length-delimited / fixed64 三种类型，字段编号与
// proto/custom_pipeline.proto 一致。示例因此不必链接 protobuf 运行库 —— 这也正是
// C# / Python / Java 调用方的处境：它们用自己的运行库产生字节。注意：C++ 调用方
// 不要把 HsBaSlicerProto 里的 .pb.cc 与 DllHsBaSlicer 链进同一个进程，protobuf
// 会因同名 proto 文件重复注册（"File already exists in database"）直接终止进程。
// ---------------------------------------------------------------------------
namespace
{
constexpr int kWireVarint = 0;
constexpr int kWireFixed64 = 1;
constexpr int kWireDelimited = 2;
constexpr int kWireFixed32 = 5;

void AppendVarint(std::string& out, uint64_t value)
{
    do
    {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value)
            byte |= 0x80;
        out.push_back(static_cast<char>(byte));
    } while (value);
}

/// @brief 追加一个 string 字段（proto3 语义：空串属于默认值，不写入字节流）
void AppendStringField(std::string& out, int field_number, std::string_view value)
{
    if (value.empty())
        return;
    AppendVarint(out, (static_cast<uint64_t>(field_number) << 3) | kWireDelimited);
    AppendVarint(out, value.size());
    out.append(value);
}

struct ProtoField
{
    int field_number = 0;
    int wire_type = 0;
    std::string_view text;  ///< wire_type 2
    uint64_t varint = 0;    ///< wire_type 0
    uint64_t fixed64 = 0;   ///< wire_type 1
};

bool ReadVarint(std::string_view& buf, uint64_t& value)
{
    value = 0;
    for (int shift = 0; shift < 64; shift += 7)
    {
        if (buf.empty())
            return false;
        const uint8_t byte = static_cast<uint8_t>(buf.front());
        buf.remove_prefix(1);
        value |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            return true;
    }
    return false;
}

/// @brief 从字节流头部读一个字段，buf 同步前移；buf 耗尽时返回 false
bool ReadField(std::string_view& buf, ProtoField& field)
{
    field = {};
    if (buf.empty())
        return false;
    uint64_t tag = 0;
    if (!ReadVarint(buf, tag))
        return false;
    field.field_number = static_cast<int>(tag >> 3);
    field.wire_type = static_cast<int>(tag & 0x7);
    switch (field.wire_type)
    {
    case kWireVarint:
        return ReadVarint(buf, field.varint);
    case kWireFixed64:
        if (buf.size() < 8)
            return false;
        std::memcpy(&field.fixed64, buf.data(), 8);
        buf.remove_prefix(8);
        return true;
    case kWireDelimited:
    {
        uint64_t len = 0;
        if (!ReadVarint(buf, len) || len > buf.size())
            return false;
        field.text = buf.substr(0, static_cast<size_t>(len));
        buf.remove_prefix(static_cast<size_t>(len));
        return true;
    }
    case kWireFixed32:
        if (buf.size() < 4)
            return false;
        buf.remove_prefix(4);
        return true;
    default:
        return false;  // group 等不在示例范围内
    }
}

/// @brief 解析整个消息的字段列表（未设置的字段不会出现）
std::vector<ProtoField> ParseProtoFields(std::string_view buf)
{
    std::vector<ProtoField> fields;
    ProtoField field;
    while (ReadField(buf, field))
        fields.push_back(field);
    return fields;
}

std::string_view TextField(const std::vector<ProtoField>& fields, int number)
{
    for (const auto& f : fields)
        if (f.field_number == number && f.wire_type == kWireDelimited)
            return f.text;
    return {};
}

uint64_t NumberField(const std::vector<ProtoField>& fields, int number)
{
    for (const auto& f : fields)
        if (f.field_number == number && f.wire_type == kWireVarint)
            return f.varint;
    return 0;
}

// custom_pipe_config 的字段编号（与 .proto 一致）
enum CustomConfigField : int
{
    kCfgScript = 1,
    kCfgSource = 2,
    kCfgEntryFunc = 3,
    kCfgConfigJson = 4,
    kCfgModelName = 5,
    kCfgModelPath = 6,
    kCfgOutputPath = 7,
};

// custom_pipe_result 的字段编号
enum CustomResultField : int
{
    kResSuccess = 1,
    kResTotalLayers = 2,
    kResOutputPath = 3,
    kResResultString = 4,
    kResErrorMessage = 5,
    kResElapsedSeconds = 6,
};
}  // namespace

// ---------------------------------------------------------------------------
// 示例 5: 以 Protobuf 字节流的方式调用 Custom 流水线
//
// 真实部署中，下面的请求字节通常由其它语言（C# / Python / Java）按
// proto/custom_pipeline.proto 序列化后经 socket / 消息队列送来，接收端只需要
// 这对 From/ToProtoBytes 的 C 接口就能把 wire 数据变成 C 配置结构。
// ---------------------------------------------------------------------------
static int RunPipelineFromProtoBytes()
{
    LogMsg("=== 示例 5: Protobuf 字节流驱动的 Custom 流水线 ===");

    // 1. 发送端：按 wire 格式组装 custom_pipe_config 请求
    std::string payload;
    AppendStringField(payload, CustomConfigField::kCfgScript, "scripts/my_fdm_pipeline.lua");
    AppendStringField(payload, CustomConfigField::kCfgSource,
                      "machine = { layer_height = 0.25, first_layer_height = 0.3, fill_spacing = 0.5, "
                      "wall_count = 2, enable_support = false, print_speed = 70, firmware = 'marlin' }");
    AppendStringField(payload, CustomConfigField::kCfgEntryFunc, "run_pipeline");
    AppendStringField(payload, CustomConfigField::kCfgConfigJson,
                      "{ \"machine\": \"HsBa-X1\", \"material\": \"PETG\" }");
    AppendStringField(payload, CustomConfigField::kCfgModelName, "stanford_bunny");
    AppendStringField(payload, CustomConfigField::kCfgModelPath, "models/stanford_bunny.stl");
    AppendStringField(payload, CustomConfigField::kCfgOutputPath, "output/custom_proto_pipeline.gcode");
    LogMsg(std::format("请求序列化完成: {} 字节 wire 数据", payload.size()));

    // 2. 接收端：proto 字节 -> C 配置结构（字符串字段由 malloc 分配）
    HsBaCustomPipelineConfig_t cfg = HsBaCustomConfigDefault();
    if (!HsBaCustomConfigFromProtoBytes(payload.data(), static_cast<int>(payload.size()), &cfg))
    {
        LogMsg("Proto 请求解析失败");
        return 0;
    }

    // 3. 配置回转：C 结构 -> proto 字节，再解一次关键字段，验证双向映射一致
    void* roundtrip_data = nullptr;
    int roundtrip_size = 0;
    if (HsBaCustomConfigToProtoBytes(&cfg, &roundtrip_data, &roundtrip_size))
    {
        const auto fields = ParseProtoFields(
            std::string_view(static_cast<const char*>(roundtrip_data), static_cast<size_t>(roundtrip_size)));
        LogMsg(std::format("配置回转 {} 字节, 脚本字段解回: {}", roundtrip_size,
                           TextField(fields, CustomConfigField::kCfgScript)));
        std::free(roundtrip_data);
    }

    // 4. 执行流水线（与示例 1 完全相同的入口，只是配置来源换成了 proto）
    HsBaCustomPipelineResult_t result = HsBaRunCustomPipeline(&cfg, OnProgress, nullptr);
    LogResult(result);

    HsBaFreeCustomConfigStrings(&cfg);  // 释放 FromProtoBytes 分配的字符串字段

    // 5. 回传结果：C 结果结构 -> proto 字节 -> 对端（这里自己解一次）反序列化
    void* response_data = nullptr;
    int response_size = 0;
    if (HsBaCustomResultToProtoBytes(&result, &response_data, &response_size))
    {
        const auto fields = ParseProtoFields(
            std::string_view(static_cast<const char*>(response_data), static_cast<size_t>(response_size)));
        const double elapsed = std::bit_cast<double>(
            [&]
            {
                for (const auto& f : fields)
                    if (f.field_number == CustomResultField::kResElapsedSeconds)
                        return f.fixed64;
                return uint64_t{0};
            }());
        LogMsg(std::format("结果回传包 {} 字节: success={}, layers={}, output={}, elapsed={:.2f}", response_size,
                           NumberField(fields, CustomResultField::kResSuccess) != 0,
                           NumberField(fields, CustomResultField::kResTotalLayers),
                           TextField(fields, CustomResultField::kResOutputPath), elapsed));
        std::free(response_data);
    }
    else
    {
        LogMsg("结果序列化失败");
    }

    const int success = result.success;
    HsBaFreeCustomPipelineResult(&result);
    return success;
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
    RunLuaDefinedSlsPipeline();
    RunLuaDefinedSlmPipeline();
    RunLuaDefinedLomPipeline();
    RunLuaDefinedTdpPipeline();
    RunLuaDefinedWaamPipeline();
    RunInlineLuaPipeline();
    RunAsyncLuaPipeline();
    RunPipelineFromProtoBytes();

    LogMsg("全部示例执行完毕。");
    return 0;
}
