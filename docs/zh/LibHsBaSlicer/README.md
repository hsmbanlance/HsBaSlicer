# LibHsBaSlicer 模块

LibHsBaSlicer 是 HsBaSlicer 的核心 C++ 库，提供完整的切片流水线能力：预处理、切片、支撑、填充、路径生成、SLA 地板/渲染/打包、SLS 导出、文件传输与 Lua 扩展。

## 子模块列表

- [Preprocess（模型预处理）](./model_preprocess.md) - 模型加载、变换、信息查询、布尔运算与抽壳、文本 CSG（OCCT DRAW）与三角网格导出
- [Slice（网格切片）](./mesh_slice.md) - Z 轴平面切片，生成层轮廓
- [Support（支撑生成）](./fdm_support.md) - FDM/SLA/Lua 支撑截面生成
- [Fill（多边形填充）](./polygon_fill.md) - 多种模式的多边形填充
- [Path（路径生成）](./path_generator.md) - 从层数据生成 G-code 路径，与路径输出前置区域路径优化
- **Floor（SLA 地板/渲染/打包）** - SLA 底座生成、层图渲染与 zip 打包导出
- **Path/SLS 导出** - SLS Lua 脚本驱动导出（无标准格式）
- **Transfer（文件传输）** - 远程执行器文件传输（连接池化 TCP 传输）
- **ParamStore（工艺参数存储）** - 将各流水线工艺参数结构体写入/读取数据库以供复用，同时提供反射驱动的 PipelineConfig ↔ JSON 编解码（`config_json_ops.hpp`）
- **Pipeline（工艺编排内核）** - DllHsBaSlicer（C ABI）与 ModuleHsBaSlicer（C++20 模块）共同调用的阶段逻辑内核，两个表面行为一致
- **Extends（扩展注册）** - 外部 Lua 函数注册池、事件回调与 C++ 事件源

## 架构

```
LibHsBaSlicer
├── Preprocess/    模型加载、变换、布尔运算、抽壳、文本 CSG（OCCT DRAW）与三角导出
├── Slice/         指定高度的网格切片
├── Support/       FDM/SLA/Lua 支撑生成
├── Fill/          多边形填充模式
├── Path/          G-code 路径生成 + 区域路径优化 + SLS Lua 导出
├── Floor/         SLA 地板/筏生成、层图渲染、zip 打包
├── Transfer/      远程文件传输（连接池化）
├── Pipeline/      供 Dll（C ABI）与 Module（C++20 模块）共享的工艺编排内核
├── ParamStore/    工艺参数写入/读取 + PipelineConfig ↔ JSON 反射编解码（封装 fileoperator ParamStore）
└── Extends/       外部 Lua 函数注册 + C++ 事件源（Zipper/DB）
```

## 使用方法

使用 LibHsBaSlicer 需要包含相应的头文件：

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/path_optimizer.hpp"        // 路径输出前置优化
#include "LibHsBaSlicer/Path/sls_export.hpp"            // SLS Lua 导出
#include "LibHsBaSlicer/Floor/sla_floor.hpp"            // SLA 地板/渲染/打包
#include "LibHsBaSlicer/Transfer/file_transfer.hpp"     // 文件传输
#include "LibHsBaSlicer/ParamStore/param_store_ops.hpp"  // 工艺参数存储
#include "LibHsBaSlicer/ParamStore/config_json_ops.hpp"  // PipelineConfig ↔ JSON 反射编解码
#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"       // Kernel::Hooks / Kernel::GeometryCapture
#include "LibHsBaSlicer/Pipeline/fdm_pipeline.hpp"       // Kernel::RunFdm
#include "LibHsBaSlicer/Pipeline/sla_pipeline.hpp"       // Kernel::RunSla
#include "LibHsBaSlicer/Pipeline/sls_pipeline.hpp"       // Kernel::RunSls
#include "LibHsBaSlicer/Pipeline/slm_pipeline.hpp"       // Kernel::RunSlm
#include "LibHsBaSlicer/Pipeline/lom_pipeline.hpp"       // Kernel::RunLom
#include "LibHsBaSlicer/Pipeline/tdp_pipeline.hpp"       // Kernel::RunTdp
#include "LibHsBaSlicer/Pipeline/waam_pipeline.hpp"      // Kernel::RunWaam
#include "LibHsBaSlicer/Pipeline/custom_pipeline.hpp"    // Kernel::RunCustom
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"     // Lua 扩展注册
#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp" // C++ 事件源
#include "LibHsBaSlicer/version_info.hpp"               // 版本信息
```

链接 `LibHsBaSlicer` 及其依赖项。

详细的 CMake 集成步骤请参考 [C++ 使用指南（CMake 集成）](../cpp_cmake_usage.md)。

## Lua 扩展函数注册

通过 `Extends/LuaAddFunction.hpp` 可注册外部 Lua 函数，在流水线各阶段创建 Lua 环境时自动注入：

```cpp
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
using namespace HsBa::Slicer;

// 注册自定义 2D 函数（可用于 Support、Fill、SLA Output 阶段）
Add2DFunctions([](lua_State* L) {
    lua_register(L, "my_2d_func", my_2d_func_impl);
});

// 注册自定义 3D 函数（可用于 Slice、Support 阶段）
Add3DFunctions([](lua_State* L) {
    lua_register(L, "my_3d_func", my_3d_func_impl);
});

// 注册自定义 File 函数（可用于 SLS Output、SLA Output 阶段）
AddFileFunctions([](lua_State* L) {
    lua_register(L, "my_file_func", my_file_func_impl);
});

// 注册事件回调（如 Zipper 事件）
AddEventCallback("zipper.on_add", [](lua_State* L) {
    lua_register(L, "on_zip_add", on_zip_add_impl);
});
```

### 各阶段可用函数类型

| 流水线阶段 | 可用函数类型 |
| --- | --- |
| Slice（切片） | 3D |
| Support（支撑） | 2D + 3D |
| Fill（填充） | 2D |
| SLS Output（SLS 输出） | File |
| SLA Output（SLA 输出） | 2D + File |

## C++ 事件源注册

通过 `Extends/EventSourceFunction.hpp` 可注册 C++ 原生事件回调（非 Lua），用于 Zipper 进度和数据库事件通知：

```cpp
#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp"
using namespace HsBa::Slicer;

// 注册 Zipper 事件回调（进度百分比 + 阶段描述）
AddZipperEventCallback([](double percent, std::string_view stage) {
    // 处理压缩进度
});

// 注册数据库事件回调（键 + 值）
AddDBEventCallback([](std::string_view key, std::string_view value) {
    // 处理数据库事件
});
```

## 文件传输

通过 `Transfer/file_transfer.hpp` 提供远程文件传输能力，内部使用连接池化 TCP 传输：

```cpp
#include "LibHsBaSlicer/Transfer/file_transfer.hpp"
using namespace HsBa::Slicer;

FileTransferConfig config;
config.host = "192.168.1.100";
config.port = "9000";
config.pool_size = 4;
config.files = {"part_a.stl", "part_b.stl"};

FileTransferResult result = TransferFiles(config, [](int percent, std::string_view stage) {
    // progress callback
});

if (result.success) {
    // result.files_transferred == result.total_files
}
```

## 工艺参数存储

通过 `ParamStore/param_store_ops.hpp` 把各流水线的工艺参数（`HsBa*PipelineConfig_t` 结构体）写入/读取数据库以供复用。Dll 层的 `param_store_pipeline.h` 即封装本组函数；Lib 层以 `std::string`/枚举返回 `ParamStoreOutcome`，不暴露 C ABI 所有权细节。

```cpp
#include "LibHsBaSlicer/ParamStore/param_store_ops.hpp"
using namespace HsBa::Slicer;

ParamStoreConn conn;
conn.backend    = ParamBackend::Sqlite;
conn.sqlitePath = "params.db";
// conn.cryptoKey = "secret";  // 可选：非空时整份配置加密存入 param_blob 列

HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
cfg.model_name   = "tough_template";
cfg.layer_height = 0.2f;

// 写入（按 key upsert），table 传 "" 使用类型默认表名
ParamStoreOutcome saved = SavePipelineParams(conn, ParamPipelineKind::Fdm, "", "tough_template", &cfg);
if (saved.success) { /* saved.paramId */ }

// 读取（回填到调用方结构体）
HsBaFdmPipelineConfig_t out = HsBaFdmConfigDefault();
ParamStoreOutcome loaded = LoadPipelineParams(conn, ParamPipelineKind::Fdm, "", "tough_template", &out);
if (loaded.success) {
    // 使用 out.model_name（由库 malloc 持有）...
    FreeLoadedConfigStrings(ParamPipelineKind::Fdm, &out);  // 释放读取到的堆字符串
}
```

- `SavePipelineParams` / `LoadPipelineParams` 依 `conn.backend` 构造 SQLite/MySQL/PostgreSQL 适配器，连接后 `EnsureTable` → `Save`/`Load`；异常收敛进 `ParamStoreOutcome::error`（`success=false`），不向外抛出。
- MySQL/PostgreSQL 分支受 `HSBA_USE_MYSQL` / `HSBA_USE_PGSQL` 编译宏守卫，未启用时返回 `success=false` 而非崩溃（移动端仅 SQLite）。
- 读取回填的 `const char*` 字段由 `std::malloc` 持有，须用 `FreeLoadedConfigStrings` 释放。
- 底层存储/反射/CRUD（含 `List`/`Update`/`Delete`/Lua）能力位于 fileoperator 的 `ParamStore`，本层仅面向 C 配置结构体暴露写入/读取。
- 可选静态加密：`conn.cryptoKey` 非空时，配置会被序列化并经 AES-256-CBC（口令由 `HsBaCipher` 派生）加密存入 `param_blob` 列；留空则维持原明文行为。读取加密行时口令缺失或错误会使 `success=false`。详见 [fileoperator/param_store.md](../fileoperator/param_store.md)。

## 流水线配置 JSON 编解码

`ParamStore/config_json_ops.hpp` 是反射驱动的 PipelineConfig ↔ JSON 编解码器，REST 服务（`restapi/config_bridge`）同样依赖本机制。本层把它加到共享核心后，直接链接的 GUI 宿主与 Dll C ABI（`config_json_pipeline.h`）可以不经过 HTTP 往返就搭建配置编辑器。编码器走与 `param_reflect` 相同的 `TypeInfo::fields` 表，逐字段交由 `param_convert`（`FieldToAny` / `AnyToField`）收敛；仅三类反射类型（`const char*`、double/float、int/int64_t/枚举）进入 JSON，其他字段被静默跳过以保留结构体原值。

```cpp
#include "LibHsBaSlicer/ParamStore/config_json_ops.hpp"
using namespace HsBa::Slicer;

// (1) 默认配置 JSON，与刚刚 HsBa*ConfigDefault() 初始化出的结构体一致
std::string defaults = DefaultConfigJson(ParamPipelineKind::Fdm);

// (2) 序列化调用方持有的配置结构体
HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
cfg.layer_height = 0.2f;
std::string json = ConfigToJson(ParamPipelineKind::Fdm, &cfg);

// (3) 解码到现有结构体；需预先初始化以保证所有 const char* 字段为 NULL
HsBaFdmPipelineConfig_t out = HsBaFdmConfigDefault();
if (ConfigFromJson(ParamPipelineKind::Fdm, json, &out)) {
    // out.model_name 等字段出现于 JSON 时由编码器 malloc 分配
    FreeLoadedConfigStrings(ParamPipelineKind::Fdm, &out);
}
```

Dll 层以 `HsBaGetDefaultConfigJson` / `HsBaConfigToJson` / `HsBaConfigFromJson` 提供同一组函数（见
[DllHsBaSlicer README](../DllHsBaSlicer/README.md)）。所有权规则：解码出的字符串由 `std::malloc` 持有，
需以 `FreeLoadedConfigStrings` / `HsBaFreeLoadedPipelineConfig` 释放，与 `LoadPipelineParams` 一致。

## 工艺编排内核（`Pipeline/`）

`Pipeline/*_pipeline.hpp` 携带了原本住在 Dll 协程里的逐工艺阶段逻辑（FDM：切片→支撑→填充→路径；SLA：预处理→切片→地板→支撑→导出；SLS/SLM/LOM/TDP/WAAM：Lua 脚本驱动导出；Custom：Lua 环境入口）。`DllHsBaSlicer/*_pipeline.cpp` 与 `ModuleHsBaSlicer` 都只调用本内核一份实现，阶段顺序、进度百分比、螺旋/花瓶处理、顶底实体分层、密度驱动间距、接缝随机化、合作取消与层几何捕获都不会出现两代码路径不一致。

### 共享类型

```cpp
#include "LibHsBaSlicer/Pipeline/kernel_types.hpp"
namespace HsBa::Slicer::Kernel {
    struct GeometryCapture {                 // 仅在 cfg.capture_geometry 启用时填充
        bool valid;
        std::vector<PolygonsD> outlines;     // 每层零件轮廓
        std::vector<PolygonsD> fills;        // 每层 FDM 填充 / SLA 地板环
        std::vector<PolygonsD> supports;     // 每层支撑截面
        std::vector<double>  zs;             // 每层平台高度（mm）
        std::vector<double>  path_xyz;       // 连续（螺旋）路径的 x,y,z 三元组手序
    };
    struct Hooks {                           // 内核运行期调用的原生回调
        std::function<void(int, std::string_view)> progress; // [0,100]，阶段文本
        const std::atomic<bool>* cancel_flag = nullptr;      // 阶段边界与层循环轮询
    };
}
```

### 逐工艺入口

| 头文件 | 入口 | 输入 | 输出结构 |
| --- | --- | --- | --- |
| `fdm_pipeline.hpp` | `Kernel::RunFdm` | `HsBaFdmPipelineConfig_t` + `Hooks` | `Kernel::FdmOutput`（`success`、`total_layers`、`gcode`、`error`、`elapsed_seconds`、`cancelled`、`geometry`） |
| `sla_pipeline.hpp` | `Kernel::RunSla` | `HsBaSlaPipelineConfig_t` + `Hooks` | `Kernel::SlaOutput` |
| `sls_pipeline.hpp` | `Kernel::RunSls` | `HsBaSlsPipelineConfig_t` + `Hooks` | `Kernel::SlsOutput` |
| `slm_pipeline.hpp` | `Kernel::RunSlm` | `HsBaSlmPipelineConfig_t` + `Hooks` | `Kernel::SlmOutput` |
| `lom_pipeline.hpp` | `Kernel::RunLom` | `HsBaLomPipelineConfig_t` + `Hooks` | `Kernel::LomOutput` |
| `tdp_pipeline.hpp` | `Kernel::RunTdp` | `HsBaTdpPipelineConfig_t` + `Hooks` | `Kernel::TdpOutput` |
| `waam_pipeline.hpp` | `Kernel::RunWaam` | `HsBaWaamPipelineConfig_t` + `Hooks` | `Kernel::WaamOutput` |
| `custom_pipeline.hpp` | `Kernel::RunCustom` | `HsBaCustomPipelineConfig_t` + `Hooks` | `Kernel::CustomOutput` |

每个内核入口接受 C ABI 配置结构体（两个调用者都拿得出的共同货币），返回原生输出结构，里面是
`std::string` 的错误/G-code/导出路径加上可选的 `GeometryCapture`。Dll 拿到后转换为
`HsBa*PipelineResult_t`（ malloc 字符串、把几何放进快照池）；Module 直接映射成对应的 `*Result`，保留原生生命周期。

```cpp
#include "LibHsBaSlicer/Pipeline/fdm_pipeline.hpp"
#include "pipelinetypes/pipeline_types.h"
#include <atomic>

HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
cfg.model_path = "model.stl";
cfg.output_path = "out.gcode";

std::atomic<bool> cancel{false};
HsBa::Slicer::Kernel::Hooks hooks{
    [](int pct, std::string_view stage) { /* UI 钩子 */ },
    &cancel,
};
auto out = HsBa::Slicer::Kernel::RunFdm(cfg, hooks);
if (out.success) {
    // out.gcode / out.total_layers / out.geometry
}
```

> 内核是**同步**的：`base/parallel.hpp` 中的 `ParallelForLayers` 在驱动为阻塞时与协程包裹逐字节一致，这
> 正是 Dll 同步入口（`task.get_result()`）与 Module 的 `run()` 使用它的方式。异步语义属于调用方：Dll
> 在内核外面罩了一层协程任务 / `HsBaSubmitJob`；Module 则把内核组合进自身的 `Task`/`Job`。

## SLA 地板/渲染/打包

通过 `Floor/sla_floor.hpp` 提供 SLA 底座（筏）生成、层图渲染和 zip 打包导出：

```cpp
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
using namespace HsBa::Slicer;

// 配置地板参数
SlaFloorConfig floor_cfg;
floor_cfg.raft_offset = 2.0;
floor_cfg.border_width = 1.0;
floor_cfg.fill_spacing = 0.5;
floor_cfg.use_convex_hull = false;

// 生成完整地板（边框 + 填充）
Polygons floor = GenerateFloorRaft(bottom_layer, floor_cfg);

// 渲染多边形到层图
RenderPolygonsToImage(layer_polys, 1920, 1080, "output/layer_0.png");

// 打包为 zip
SlaPackage pkg;
pkg.layer_outlines = layers;
pkg.image_width = 1920;
pkg.image_height = 1080;
SaveSlaPackage(pkg, "output/result.zip");

// 或使用 Lua 自定义导出
SaveSlaPackageLua(pkg, "output/result.zip", lua_script, "export_sla");
```

### 主要函数

| 函数 | 说明 |
| --- | --- |
| `GenerateFloorContact()` | 计算底板接触区域 |
| `GenerateFloorRaft()` | 生成完整筏（边框 + 填充） |
| `GenerateFloorBorder()` | 仅生成边框环 |
| `GenerateFloorFill()` | 仅生成内部填充 |
| `LuaCustomFloorByFile()` | Lua 脚本文件自定义地板 |
| `LuaCustomFloorByString()` | 内联 Lua 脚本自定义地板 |
| `RenderPolygonsToImage()` | 渲染多边形为图片（PNG/JPG/SVG） |
| `SaveSlaPackage()` | 打包为 zip 归档 |
| `SaveSlaPackageLua()` | Lua 自定义导出逻辑打包 |

## SLS Lua 导出

通过 `Path/sls_export.hpp` 提供 SLS 导出能力。SLS 无标准输出格式，完全由 Lua 脚本决定：

```cpp
#include "LibHsBaSlicer/Path/sls_export.hpp"
using namespace HsBa::Slicer;

SlsPackage pkg;
pkg.layer_outlines = layers;      // 各层轮廓
pkg.layer_z_heights = z_heights;  // 各层 Z 高度

// Lua 脚本接收 config/images/output_path 全局变量
SaveSlsPackageLua(pkg, "output/result.zip", "scripts/export_sls.lua", "export_sls");
```

## 路径优化（路径输出前置）

通过 `Path/path_optimizer.hpp` 提供路径输出前置优化：把独立的多边形区域作为图顶点（AreaGraph），
求解使空走代价最小的区域访问顺序，减少输出路径中的空走距离。按执行时机分两种模式（同一优化器内不可混用）：

| 模式 | 执行时机 | 区域输入 | 门禁（候选出入门点） | 输出 |
| --- | --- | --- | --- | --- |
| 多边形模式 | 填充前 | 多边形本身（轮廓） | 全部多边形顶点 | 优化顺序的多边形集合（供后续填充） |
| 填充结果模式 | 填充后 | 填充路径（支持多点折线） | 每条折线的首/尾端点 | 完整填充路径 |

### C++ 接口

```cpp
#include "LibHsBaSlicer/Path/path_optimizer.hpp"
using namespace HsBa::Slicer;

// 多边形模式（填充前执行）：输出优化顺序的多边形，供后续填充使用
RegionPathOptimizer opt;
opt.addPolygonRegion(1, region1_polys);
opt.addPolygonRegion(2, region2_polys);
opt.optimizeOrder();                       // 区域数 >= 3 用遗传 TSP 求解
PolygonsD ordered_polys = opt.buildPolygons();
// 区域内多边形次序贪心编排，每个多边形仅旋转起点至入门禁顶点，不改变环绕方向

// 填充结果模式（填充后执行）：输入支持多点折线，输出完整填充路径
RegionPathOptimizer opt2;
opt2.addRegion(1, region1_paths);
opt2.addRegion(2, region2_paths);
opt2.addRoute(1, 2, 42.0);                 // 可选：手动覆盖区域间空走代价（对称）
opt2.optimizeOrder();
PolygonsD full_paths = opt2.buildPaths();  // 区域内路径方向/次序贪心编排，减少跳变

// 全组合图：由用户算法决定区域访问顺序
RegionPathOptimizer opt3;
opt3.addRegion(1, region1_paths);
opt3.addRegion(2, region2_paths);
opt3.addRegion(3, region3_paths);
auto fg = opt3.getFullGraph();             // N x N 两两最短路径代价矩阵 + 扁平边表
auto eg = opt3.getExpandedGraph();         // 门级展开图（所有入口/出口顶点与边）
opt3.setCustomTour({3, 1, 2});             // 反馈用户计算的顺序（跳过内置遗传 TSP）
PolygonsD custom_paths = opt3.buildPaths();
```

### Lua 脚本嵌入方案（唯一交付入口）

脚本环境内注册全局表 `PathOptimize`：

| Lua 接口 | 说明 |
| --- | --- |
| `PathOptimize.new()` | 创建优化器对象（`addPolygons`/`addRegion`/`addRoute`/`optimizeOrder`/`buildPolygons`/`buildPaths`/`getFullGraph`/`getExpandedGraph`/`setCustomTour`） |
| `PathOptimize.optimizePolygons(regions)` | 多边形模式一键优化，返回优化顺序的多边形表 |
| `PathOptimize.optimizeRegions(regions)` | 填充结果模式一键优化，返回完整填充路径表（支持多点折线） |

优化器对象额外暴露**全组合图**，供用户用 Lua 设计任意路径优化算法：

| Lua 接口 | 说明 |
| --- | --- |
| `opt:getFullGraph()` | 区域级全组合图：`{vertices, distMatrix（不可达为 false）, edges, areaCount}`，即所有区域两两的门禁感知最短路径代价 |
| `opt:getExpandedGraph()` | 门级展开图：`{vertices（areaId/gateId/isExit）, edges（from/to/weight，1 基下标）, vertexCount, edgeCount}` |
| `opt:setCustomTour(tour[, entryGates])` | 反馈用户计算的顺序（区域 id 数组）与可选 `{[areaId]=门禁下标}` 映射，随后 `buildPaths()`/`buildPolygons()` 复用该顺序、跳过内置遗传 TSP |

其中 `regions` = 区域数组，每个区域 = 折线/多边形数组，每条折线/多边形 = `{x=.., y=..}` 点数组。

脚本契约：函数接收 `regions`，返回同格式的优化结果表；脚本内还可使用多边形操作与填充函数。
示例脚本见 `tests/PolygonFill/optimize_paths.lua`（内置策略）与 `tests/PolygonFill/custom_tsp.lua`
（在全组合图上运行用户自定义算法）：

```lua
-- 填充结果模式：输出完整填充路径（支持多点折线）
function optimize_paths(regions)
    return PathOptimize.optimizeRegions(regions)
end

-- 多边形模式（填充前执行）：输出优化顺序的多边形集合，供后续填充使用
function optimize_polygons(regions)
    return PathOptimize.optimizePolygons(regions)
end

-- 在全组合图上运行用户自定义算法（最近邻示例）
function optimize_paths_custom(regions)
    local opt = PathOptimize.new()
    for i, paths in ipairs(regions) do
        opt:addRegion(i, paths)
    end
    local graph = opt:getFullGraph()          -- N x N 两两代价矩阵
    local tour = { graph.vertices[1] }         -- 从首个区域出发
    -- ...每步选择代价最小的未访问区域，再把顺序反馈给优化器...
    opt:setCustomTour(tour)
    return opt:buildPaths()
end
```

C++ 嵌入入口（脚本文件 / 内联字符串两个版本）：

| 函数 | 说明 |
| --- | --- |
| `LuaOptimizeRegionPolygons()` / `LuaOptimizeRegionPolygonsString()` | 多边形模式（默认函数名 `optimize_polygons`） |
| `LuaOptimizeRegionPaths()` / `LuaOptimizeRegionPathsString()` | 填充结果模式（默认函数名 `optimize_paths`） |

```cpp
// 脚本环境中已注册多边形操作、填充与 PathOptimize 函数；可经 lua_reg 追加自定义注册
PolygonsD optimized = LuaOptimizeRegionPaths(regions, "scripts/optimize_paths.lua");
```

### 流程接入

按分层契约，`RegisterLuaPathOptimizeFunctions` 已接入 **Fill（填充）阶段**：`LuaCustomFillByFile()`
的 Lua 环境在外部 2D 函数池之外自动注册 `PathOptimize`，填充脚本可直接在路径输出前优化区域顺序。
Slice/Support/SLS Output/SLA Output 阶段不涉及路径空走语义，不接入。


## 版本信息

```cpp
#include "LibHsBaSlicer/version_info.hpp"
using namespace HsBa::Slicer;

std::string json = GetVersionJson();  // JSON 格式版本信息
std::string xml = GetVersionXml();    // XML 格式版本信息
```

## 典型工作流

1. **预处理**：通过 `LoadModel()` 加载模型，施加变换，可选执行布尔运算/抽壳
2. **切片**：在每个层高位置通过 `Slice()` 生成层轮廓
3. **支撑**：通过 `GenerateAllFdmSupport()` / `GenerateAllSlaSupport()` 生成支撑结构
4. **填充**：通过 `FillPolygon()` 或 `FillWithBorder()` 填充层多边形；可在填充前（多边形模式）或填充后（填充结果模式）通过 `RegionPathOptimizer` / Lua 脚本优化区域路径顺序
5. **路径**：通过 `GenerateGCodePathV2()` 生成支持多固件格式的 G-code 路径
6. **SLA 导出**：通过 `GenerateFloorRaft()` + `RenderPolygonsToImage()` + `SaveSlaPackage()` 完成 SLA 输出
7. **SLS 导出**：通过 `SaveSlsPackageLua()` 由 Lua 脚本决定输出格式

## 模型预处理高级操作（CGAL/OCCT）

当编译时启用 `USE_CGAL` 宏，预处理模块提供以下高级几何操作：

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
using namespace HsBa::Slicer;

// Load models
LoadModel("body", "models/body.step");    // BRep via OCCT
LoadModel("cavity", "models/cavity.stl"); // Mesh via IGL

// 插入外部构造的模型到池中
auto custom_model = std::make_shared<FullTopoModel>(/*...*/);
InsertModel("custom", custom_model);

// ThickSolid (shell) - requires OCCT BRep source
ThickSolidModel("body", "shelled", 2.0f);

// ThickSolid with face exclusion (open faces specified as vertex loops)
std::vector<std::vector<Eigen::Vector3f>> open_faces = {{{0,0,0},{1,0,0},{1,1,0}}};
ThickSolidModel("body", "shelled_open", open_faces, 2.0f);

// Boolean operations - OCCT for BRep-BRep, IGL/CGAL fallback for mesh
BooleanUnion("body", "cavity", "merged");
BooleanDifference("body", "cavity", "cut");
BooleanIntersection("body", "cavity", "common");
BooleanXor("body", "cavity", "xor_result");

// Pool management
ContainsModel("merged");   // true
ModelCount();              // number of models
GetModelNames();           // all names
CleanupModels();           // remove unreferenced models
```

### 三角网格导出

`GetModelTriangles` 以 igl 风格的 `Eigen` 顶点/面片对返回模型。网格模型直接返回存储数据；BRep 模型（STEP / IGES / BREP，文本 CSG）需要依赖内核默认偏转量现场细分，三角面数随质量设置变化。

```cpp
auto [vertices, faces] = GetModelTriangles("merged");   // vertices: N x 3 (mm)，faces: M x 3 (0 基下标)
```

Dll C ABI 以 `HsBaGetModelTriangles` / `HsBaFreeModelTriangles` 暴露同一组数据，两个数组住在同一个 `malloc` 块里（与 `HsBaLayerGeometry_t` 同样的所有权契约）。

### 跨线程发布（`PublishModel` / `UnpublishModel`）

模型池是**按线程隔离**的，流水线协程的各阶段在 executor 线程上运行；调用线程上以名字加载的模型在
`RunFdm` / `RunSla` / ... 里不会命中。`PublishModel(name)` 会在进程范围内保留一份同名的引用，本文件
里的所有查询都会回退到它，变换也随实例可见；直到调用 `UnpublishModel` 或 `RemoveModel` 才撤销。同名并发
发布会覆盖：后发布者的实例被共享。

```cpp
LoadModel("body", "models/body.step");     // 调用线程本地池
PublishModel("body");                       // 跨线程按名可见
Kernel::RunFdm(cfg, hooks);                 // cfg.model_name = "body" 在 worker 线程解析成功
UnpublishModel("body");                     // 仅撤进程级副本，调用线程本地项仍在
```

`RemoveModel` 同时撤本线程池项与发布副本。

### 文本 CSG（`BuildCsgModel`，OCCT DRAW）

OpenCASCADE 带 Draw 模块（vcpkg 特性 `opencascade[tcl]`）编译时，可以以**文本** CSG 脚本定义 B-Rep
模型而不依赖文件。`HasTextCsgSupport()` 返回当前二进制是否可以求值这类脚本；`BuildCsgModel(name,
script, shapeName)` 返回的模型与从文件加载的完全同位，可变换、可查询、可切片。

脚本使用 OCCT DRAW Test Harness 命令，一行一条：

| 类别 | 命令 |
| --- | --- |
| 图元 | `box`、`wedge`、`pcylinder`、`pcone`、`psphere`、`ptorus` |
| 变换 | `ttranslate`、`trotate`、`tscale`、`tmirror`、`tcopy`、`reset` |
| 布尔 | `bcut`、`bfuse`、`bcommon`、`bsection` 及 `bop*` 系列 |
| 仓库 | `compound`、`explode`、`orientation`、`nbshapes`、`checkshape`、`vprops` |
| Tcl | `set`、`expr` 用于参数代入 |

```cpp
if (HasTextCsgSupport()) {
    static const char* bracket =
        "set t 3\n"
        "box base 60 40 $t\n"
        "pcylinder boss 8 12\n"
        "ttranslate boss 30 20 $t\n"
        "bfuse result base boss\n";
    auto model = BuildCsgModel("bracket", bracket);   // shapeName 默认 "result"
}
```

失败以项目异常报出：`InvalidArgumentError`（名字冲突/脚本不合法）、`NotFoundError`（脚本未在
`shapeName` 下留下形状）、`NotSupportedError`（本构建未带 DRAW Test Harness）。解释器是完整的 Tcl，
**未受沙箱限制**（`exec`、`open`、`source`、`file`、`readbrep`、`binsave` 等都可达），只能执行可信来源
的脚本。

### 内核路由策略

| 操作 | BRep 模型 (OCCT) | 网格模型 (IGL/CGAL) |
| --- | --- | --- |
| 布尔运算 | OCCT 优先 | IGL/CGAL 回退 |
| 抽壳 (ThickSolid) | 支持 | 不支持（抛出异常） |
| 加载 | STEP/IGES/VRML/BREP | STL/OBJ/PLY/OFF |

## GCode 多固件输出（V2）

`GenerateGCodePathV2()` 返回 `GCodePath` 对象（继承自 `LayersPath`），支持按目标固件生成标准 3D 打印机 GCode：

```cpp
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "paths/gcodepath.hpp"
using namespace HsBa::Slicer;

// 配置打印机参数
GCodePrinterConfig printer_cfg;
printer_cfg.nozzle_temp = 210.0f;
printer_cfg.bed_temp = 60.0f;
printer_cfg.filament_diameter = 1.75f;
printer_cfg.retract_length = 1.2f;

// 生成 GCode 路径
auto gcode_path = GenerateGCodePathV2(layer_data, path_cfg, printer_cfg);

// 按固件格式输出
std::string marlin_gcode = gcode_path->ToGCode(GCodeFirmware::Marlin);
std::string klipper_gcode = gcode_path->ToGCode(GCodeFirmware::Klipper);

// 保存到文件
gcode_path->SaveGCode("output/model.gcode", GCodeFirmware::Marlin);
```

### 支持的固件格式

| 固件 | 枚举值 | 特性 |
| --- | --- | --- |
| Marlin | `GCodeFirmware::Marlin` | M104/M109 温度等待、G92 E0 复位、M82/M83 挤出模式 |
| RepRap/RRF | `GCodeFirmware::RepRap` | 额外 M106 风扇控制、M82/M83 显式切换 |
| Klipper | `GCodeFirmware::Klipper` | SET_PRESSURE_ADVANCE、M220/M221 速度/流量百分比、SET_FAN_SPEED |

### GCodePrinterConfig 字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `nozzle_diameter` | 0.4 | 喷嘴直径 (mm) |
| `filament_diameter` | 1.75 | 耗材直径 (mm) |
| `nozzle_temp` | 200.0 | 喷嘴温度 (°C) |
| `bed_temp` | 60.0 | 热床温度 (°C) |
| `retract_length` | 1.0 | 回抽长度 (mm) |
| `retract_speed` | 40.0 | 回抽速度 (mm/s) |
| `first_layer_speed` | 20.0 | 首层速度 (mm/s) |
| `relative_extrusion` | false | 相对挤出模式 (M83) |
| `enable_retraction` | true | 启用回抽 |

> 原 `GenerateGCodePath()` 仍然可用（返回 `PointsPath`），保持向后兼容。

## 命名空间

所有 API 均位于 `HsBa::Slicer` 命名空间中。

## 示例

- `samples/LibHsBaSlicer/main_header.cpp` — 完整 FDM 切片流程（传统 #include）
- `samples/LibHsBaSlicer/main_module.cpp` — C++20 模块版（`import hsba.slicer;`，类风格 API）
- `samples/CsgModel/main.cpp` — 文本 CSG（OCCT DRAW）C ABI 示例；图元、Tcl 参数、fuse/cut、交给 FDM 流水线以及三类失败原因输出

## C++20 模块版（ModuleHsBaSlicer）

若编译器支持 C++20 模块，可使用 `ModuleHsBaSlicer` 包装层，提供类风格 API：

```cpp
#include <iostream>          // MSVC: #include 必须在 import 之前
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"  // GMF 类型不随模块导出
#include "pipelinetypes/pipeline_types.h"

import hsba.slicer;
using namespace HsBa::Slicer;

Model model("bunny", "model.stl");   // RAII
HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
cfg.output_path = "out.gcode";
FdmPipeline pipeline(cfg);
FdmResult result = pipeline.run(model);  // 全流程
```

**注意**：
- MSVC 下 `#include` 必须位于 `import` 之前（否则 C2572 重定义错误）
- GMF 中的项目类型（`ModelInfo` 等）不随模块导出，消费者需自行 `#include`
- `Point2`/`Polygon`/`Polygons`/`Point2D`/`PolygonD`/`PolygonsD`/`LuaRegFunc` 这些别名仅在 MSVC 下随模块导出：GCC（≥ 15）/Clang 会将在模块内重新声明 GMF 同名实体判为 conflicting declaration，需要命名它们时请自行 `#include "2D/IntPolygon.hpp"`、`#include "2D/FloatPolygons.hpp"`、`#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"`
- 配置结构体复用 `pipelinetypes/pipeline_types.h`，禁止重复定义
- 模块包装层仅能在 MSVC、Clang 或 GCC ≥ 16 上构建：GMF 头文件（Eigen/libstdc++）暴露内部链接实体，GCC 15 视为硬错误，配置阶段探测到后会自动关闭 `HSBA_SLICER_MODULE`，此时请改用传统 `#include` 版 API（见 `samples/LibHsBaSlicer/main_header.cpp`）或 DllHsBaSlicer C ABI

详见 [C++ 使用指南](../cpp_cmake_usage.md#使用-modulehsbaslicerc20-模块版)。
