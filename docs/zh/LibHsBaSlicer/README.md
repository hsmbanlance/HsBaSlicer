# LibHsBaSlicer 模块

LibHsBaSlicer 是 HsBaSlicer 的核心 C++ 库，提供完整的切片流水线能力：预处理、切片、支撑、填充、路径生成、SLA 地板/渲染/打包、SLS 导出、文件传输与 Lua 扩展。

## 子模块列表

- [Preprocess（模型预处理）](./model_preprocess.md) - 模型加载、变换、信息查询、布尔运算与抽壳
- [Slice（网格切片）](./mesh_slice.md) - Z 轴平面切片，生成层轮廓
- [Support（支撑生成）](./fdm_support.md) - FDM/SLA/Lua 支撑截面生成
- [Fill（多边形填充）](./polygon_fill.md) - 多种模式的多边形填充
- [Path（路径生成）](./path_generator.md) - 从层数据生成 G-code 路径
- **Floor（SLA 地板/渲染/打包）** - SLA 底座生成、层图渲染与 zip 打包导出
- **Path/SLS 导出** - SLS Lua 脚本驱动导出（无标准格式）
- **Transfer（文件传输）** - 远程执行器文件传输（连接池化 TCP 传输）
- **Extends（扩展注册）** - 外部 Lua 函数注册池、事件回调与 C++ 事件源

## 架构

```
LibHsBaSlicer
├── Preprocess/    模型加载、变换、布尔运算与抽壳
├── Slice/         指定高度的网格切片
├── Support/       FDM/SLA/Lua 支撑生成
├── Fill/          多边形填充模式
├── Path/          G-code 路径生成 + SLS Lua 导出
├── Floor/         SLA 地板/筏生成、层图渲染、zip 打包
├── Transfer/      远程文件传输（连接池化）
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
#include "LibHsBaSlicer/Path/sls_export.hpp"            // SLS Lua 导出
#include "LibHsBaSlicer/Floor/sla_floor.hpp"            // SLA 地板/渲染/打包
#include "LibHsBaSlicer/Transfer/file_transfer.hpp"     // 文件传输
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
4. **填充**：通过 `FillPolygon()` 或 `FillWithBorder()` 填充层多边形
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
- 配置结构体复用 `pipelinetypes/pipeline_types.h`，禁止重复定义

详见 [C++ 使用指南](../cpp_cmake_usage.md#使用-modulehsbaslicerc20-模块版)。
