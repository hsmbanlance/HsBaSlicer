/**
 * @file main.cpp
 * @brief HsBaSlicer 文本 CSG 的 C ABI 使用示例（不依赖任何模型文件）
 *
 * 前面的工艺示例都从 models/stanford_bunny.stl 出发；本示例反过来：零件完全由
 * 一段文本描述，OCCT 的 DRAW Test Harness 解释它，直接得到 B-Rep 模型并进入模型池。
 * 用到的 C 接口都在 DllHsBaSlicer/model_preprocess.h：
 *   HsBaTextCsgAvailable()          本次构建能否求值 CSG 脚本
 *   HsBaBuildCsgModel(...)          文本 -> 模型池，失败时把原因写进调用方的缓冲区
 *   HsBaGetModelInfo / HsBaTranslateModel / ...  之后按名字继续操作，和文件加载的模型无异
 *   HsBaReleaseModelHandle          释放句柄（模型仍留在池中）
 *   HsBaRemoveModel                 从池中移除
 *
 * 脚本语言是 OCCT DRAW 命令（一行一条），可用的几何词表：
 *   图元   box / wedge / pcylinder / pcone / psphere / ptorus
 *   变换   ttranslate / trotate / tscale / tmirror / tcopy / reset
 *   仓库   compound / explode / orientation / nbshapes / checkshape / vprops
 *   布尔   bcut / bfuse / bcommon / bsection 以及 bop* 系列
 * Tcl 的 set / expr 可直接用于参数代入；布尔命令的产物是 compound，需要单一实体时在
 * 脚本里 explode。
 *
 * 注意：脚本不受沙箱限制。这里只筛掉了几何命令集，解释器本身仍是完整的 Tcl
 * （exec / open / source / file 与 readbrep / binsave / binrestore 都可达），一段脚本
 * 等价于一次本机 shell 调用，因此只能执行可信来源的脚本。
 *
 * 模型池是按线程隔离的：HsBaBuildCsgModel 把模型放进调用线程的池，示例 5 因此把
 * “建模型”和“跑流水线”放在同一个线程里，异步入口（HsBaRunFdmPipelineAsync）则需要
 * 让流水线自己加载模型。
 *
 * 支持平台: Windows / Linux / macOS（Android / iOS 与游戏主机无 OCCT，不构建本示例）
 */

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include "fdm_pipeline.h"
#include "model_preprocess.h"

#ifndef HSBA_GAME_CONSOLE
#include "logger/logger.hpp"
using HsBa::Slicer::Log::LoggerSingletone;
#endif

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

// 进度回调：CSG 之后跑流水线时由 C++ 各阶段驱动
void OnProgress(int percent, const char* stage, void* /*user_data*/)
{
#ifndef HSBA_GAME_CONSOLE
    LoggerSingletone::LogInfo(std::format("[{}%] {}", percent, stage ? stage : ""));
#else
    (void)percent;
    (void)stage;
#endif
}

/// @brief 取回模型的体积与包围盒，示例用它们证明脚本确实造出了所要的几何
struct ModelStats
{
    float volume = 0.0F;
    float min[3] = {0.0F, 0.0F, 0.0F};
    float max[3] = {0.0F, 0.0F, 0.0F};
};

ModelStats QueryModel(const char* name)
{
    ModelStats stats;
    if (!HsBaGetModelInfo(name, stats.min, stats.max, &stats.volume))
    {
        LogMsg(std::format("查询失败: 池中没有名为 {} 的模型", name));
    }
    return stats;
}

/// @brief 构建模型并在失败时打印原因；返回的句柄由调用方释放
void* BuildModel(const char* name, const char* script, const char* shape_name)
{
    char reason[512] = {};
    void* handle = HsBaBuildCsgModel(name, script, shape_name, reason, static_cast<int>(sizeof(reason)));
    if (!handle)
    {
        LogMsg(std::format("构建 {} 失败: {}", name, reason));
    }
    return handle;
}
}  // namespace

// ---------------------------------------------------------------------------
// 示例 1: 一条命令一个图元，体积与包围盒精确可核对
// ---------------------------------------------------------------------------
static void BuildOnePrimitive()
{
    LogMsg("=== 示例 1: 单条 DRAW 命令构建底板 ===");

    void* handle = BuildModel("plate", "box result 60 40 12", nullptr);
    if (!handle)
    {
        return;
    }

    const ModelStats stats = QueryModel("plate");
    LogMsg(std::format("底板体积 {:.3f} mm^3（期望 60*40*12 = 28800）", stats.volume));
    LogMsg(std::format("包围盒 ({:.1f}, {:.1f}, {:.1f}) - ({:.1f}, {:.1f}, {:.1f})，box 以一个角为原点",
                       stats.min[0], stats.min[1], stats.min[2], stats.max[0], stats.max[1], stats.max[2]));

    HsBaReleaseModelHandle(handle);
    HsBaRemoveModel("plate");
}

// ---------------------------------------------------------------------------
// 示例 2: 参数代入 + 指定读回的 DRAW 变量
//
// shape_name 给非空值时读的是那个变量，因此中间结果可以留在脚本里而不必合并；
// 这里 base 就是最终形状，boss 只是被建出来没有用上。
// ---------------------------------------------------------------------------
static void ParameterizedWithNamedVariable()
{
    LogMsg("=== 示例 2: Tcl 参数代入并指定读回变量 ===");

    static const char* kScript = "set t 18\nbox slab 30 30 $t\n";
    void* handle = BuildModel("slab", kScript, "slab");
    if (!handle)
    {
        return;
    }

    const ModelStats stats = QueryModel("slab");
    LogMsg(std::format("由 $t 代入得到的高度 {:.3f}，体积 {:.3f} mm^3（期望 30*30*18 = 16200）", stats.max[2],
                       stats.volume));

    HsBaReleaseModelHandle(handle);
    HsBaRemoveModel("slab");
}

// ---------------------------------------------------------------------------
// 示例 3: 用 bfuse 把图元拼成一个装配体
// ---------------------------------------------------------------------------
static void FusePrimitives()
{
    LogMsg("=== 示例 3: 底座 + 圆柱凸台 fuse 成一个模型 ===");

    static const char* kBracket =
        "set t 3\n"
        "box base 60 40 $t\n"
        "pcylinder boss 8 12\n"
        "ttranslate boss 30 20 $t\n"
        "bfuse result base boss\n";
    void* handle = BuildModel("bracket", kBracket, nullptr);
    if (!handle)
    {
        return;
    }

    // 凸台完整落在底座 footprint 内并向上生长，体积就是两者之和
    const ModelStats stats = QueryModel("bracket");
    const double boss = 3.14159265358979 * 8.0 * 8.0 * 12.0;
    LogMsg(std::format("支架体积 {:.3f} mm^3（期望 底座 7200 + 凸台 {:.3f} = {:.3f}）", stats.volume, boss,
                       7200.0 + boss));
    LogMsg(std::format("总高 {:.3f}（底座 3 + 凸台 12）", stats.max[2]));

    HsBaReleaseModelHandle(handle);
    HsBaRemoveModel("bracket");
}

// ---------------------------------------------------------------------------
// 示例 4: bcut 打孔，再把零件抬高到打印平台上
// ---------------------------------------------------------------------------
static void CutHoleAndMoveUp()
{
    LogMsg("=== 示例 4: 通孔 + 平移 ===");

    static const char* kPerforated =
        "box block 60 40 12\n"
        "pcylinder pin 5 20\n"
        "ttranslate pin 30 20 -4\n"
        "bcut result block pin\n";
    void* handle = BuildModel("pad", kPerforated, nullptr);
    if (!handle)
    {
        return;
    }

    const double hole = 3.14159265358979 * 5.0 * 5.0 * 12.0;
    const ModelStats before = QueryModel("pad");
    LogMsg(std::format("打孔后体积 {:.3f} mm^3（期望 28800 - 孔 {:.3f} = {:.3f}）", before.volume, hole,
                       28800.0 - hole));

    // 平移走的是与文件模型同一个 C 接口
    if (HsBaTranslateModel("pad", 100.0F, 0.0F, 5.0F))
    {
        const ModelStats moved = QueryModel("pad");
        LogMsg(std::format("平移后 X 范围 {:.1f} - {:.1f}, Z 范围 {:.1f} - {:.1f}", moved.min[0], moved.max[0],
                           moved.min[2], moved.max[2]));
    }

    HsBaReleaseModelHandle(handle);
    HsBaRemoveModel("pad");
}

// ---------------------------------------------------------------------------
// 示例 5: 文本造出来的模型直接进 FDM 流水线（同一个线程即可）
//
// 流水线先按 model_name 查池，命中就不再读文件：model_path 留空即可切片一个从未落盘的
// 零件。
// ---------------------------------------------------------------------------
static void SliceTheCsgModel()
{
    LogMsg("=== 示例 5: CSG 模型直接交给 FDM 流水线 ===");

    std::filesystem::create_directories("output");

    void* handle = BuildModel("csg_vase", "pcylinder body 25 20\n", nullptr);
    if (!handle)
    {
        return;
    }
    HsBaReleaseModelHandle(handle);  // 池里仍持有，流水线按名字取用

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name = "csg_vase";
    cfg.model_path = nullptr;  // 关键：没有文件
    cfg.output_path = "output/csg_vase.gcode";
    cfg.layer_height = 0.2F;
    cfg.first_layer_height = 0.2F;
    cfg.enable_support = 0;

    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);
    if (result.success)
    {
        LogMsg(std::format("切片成功: {} 层, G-code {} 字节, 耗时 {:.2f} 秒", result.total_layers,
                           result.gcode_content ? std::strlen(result.gcode_content) : 0, result.elapsed_seconds));
    }
    else
    {
        LogMsg(std::format("切片失败: {}", result.error_message ? result.error_message : "未知错误"));
    }

    HsBaFreePipelineResult(&result);
    HsBaRemoveModel("csg_vase");
}

// ---------------------------------------------------------------------------
// 示例 6: 失败原因写成文本，而不是只留一个 NULL
// ---------------------------------------------------------------------------
static void FailureReasons()
{
    LogMsg("=== 示例 6: 三类失败各自的文本原因 ===");

    char reason[256] = {};

    // 1) 命令报错：DRAW 的 box 需要 6 个数字
    if (HsBaBuildCsgModel("bad", "box result 60", nullptr, reason, static_cast<int>(sizeof(reason))))
    {
        LogMsg("意外: 参数不足的 box 竟然成功了");
    }
    else
    {
        LogMsg(std::format("脚本语法/参数错误 -> {}", reason));
    }

    // 2) 脚本没错，但结束时要找的变量里没有形状
    if (HsBaBuildCsgModel("ghost", "box built 1 1 1", "neverAssigned", reason,
                          static_cast<int>(sizeof(reason))))
    {
        LogMsg("意外: 不存在的变量竟然成功了");
    }
    else
    {
        LogMsg(std::format("结果变量缺失 -> {}", reason));
    }

    // 3) 名字已被占用：不静默覆盖，直接报出来
    void* first = HsBaBuildCsgModel("taken", "box t 1 1 1", nullptr, nullptr, 0);
    const bool second_failed =
        !HsBaBuildCsgModel("taken", "box t 2 2 2", nullptr, reason, static_cast<int>(sizeof(reason)));
    LogMsg(std::format("重名构建 -> {}", second_failed ? reason : "意外成功"));
    HsBaReleaseModelHandle(first);
    HsBaRemoveModel("taken");

    // 参数为 NULL 也是可说的错误，不会解引用
    if (!HsBaBuildCsgModel(nullptr, "box n 1 1 1", nullptr, reason, static_cast<int>(sizeof(reason))))
    {
        LogMsg(std::format("空参数 -> {}", reason));
    }
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
int main()
{
    LogMsg("HsBaSlicer 文本 CSG（C ABI）示例");
    LogMsg("================================");

    if (!HsBaTextCsgAvailable())
    {
        LogMsg("本次构建没有 OpenCASCADE 的 DRAW Test Harness（vcpkg 特性 opencascade[tcl]），");
        LogMsg("文本 CSG 不可用；保持 HSBA_REQUIRE_TEXT_CSG=ON 时配置阶段就会直接失败。");
        return EXIT_FAILURE;
    }

    BuildOnePrimitive();
    ParameterizedWithNamedVariable();
    FusePrimitives();
    CutHoleAndMoveUp();
    SliceTheCsgModel();
    FailureReasons();

    LogMsg("全部示例执行完毕。");
    return 0;
}
