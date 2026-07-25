# LibHsBaSlicer 模块

LibHsBaSlicer 是 HsBaSlicer 的核心 C++ 静态库，提供五大切片接口：预处理、切片、支撑、填充和路径生成。

## 子模块列表

- [Preprocess（模型预处理）](./model_preprocess.md) - 模型加载、变换、信息查询、布尔运算与抽壳
- [Slice（网格切片）](./mesh_slice.md) - Z 轴平面切片，生成层轮廓
- [Support（FDM 支撑生成）](./fdm_support.md) - FDM 支撑截面生成
- [Fill（多边形填充）](./polygon_fill.md) - 多种模式的多边形填充
- [Path（路径生成）](./path_generator.md) - 从层数据生成 G-code 路径
- **Extends（Lua 扩展注册）** - 外部 Lua 函数注册池与事件回调

## 架构

```
LibHsBaSlicer
├── Preprocess/    模型加载、变换、布尔运算与抽壳
├── Slice/         指定高度的网格切片
├── Support/       FDM 支撑生成
├── Fill/          多边形填充模式
├── Path/          G-code 路径生成
└── Extends/       外部 Lua 函数注册（2D/3D/File/事件回调）
```

## 使用方法

使用 LibHsBaSlicer 需要包含相应的头文件：

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"  // Lua 扩展注册
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

## 典型工作流

1. **预处理**：通过 `LoadModel()` 加载模型，施加变换，可选执行布尔运算/抽壳
2. **切片**：在每个层高位置通过 `Slice()` 生成层轮廓
3. **支撑**：通过 `GenerateAllFdmSupport()` 生成支撑结构
4. **填充**：通过 `FillPolygon()` 或 `FillWithBorder()` 填充层多边形
5. **路径**：通过 `GenerateGCodePathV2()` 生成支持多固件格式的 G-code 路径

## 模型预处理高级操作（CGAL/OCCT）

当编译时启用 `USE_CGAL` 宏，预处理模块提供以下高级几何操作：

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
using namespace HsBa::Slicer;

// Load models
LoadModel("body", "models/body.step");    // BRep via OCCT
LoadModel("cavity", "models/cavity.stl"); // Mesh via IGL

// ThickSolid (shell) - requires OCCT BRep source
ThickSolidModel("body", "shelled", 2.0f);

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
