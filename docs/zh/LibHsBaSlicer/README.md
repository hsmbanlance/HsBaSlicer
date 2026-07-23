# LibHsBaSlicer 模块

LibHsBaSlicer 是 HsBaSlicer 的核心 C++ 静态库，提供五大切片接口：预处理、切片、支撑、填充和路径生成。

## 子模块列表

- [Preprocess（模型预处理）](./model_preprocess.md) - 模型加载、变换和信息查询
- [Slice（网格切片）](./mesh_slice.md) - Z 轴平面切片，生成层轮廓
- [Support（FDM 支撑生成）](./fdm_support.md) - FDM 支撑截面生成
- [Fill（多边形填充）](./polygon_fill.md) - 多种模式的多边形填充
- [Path（路径生成）](./path_generator.md) - 从层数据生成 G-code 路径

## 架构

```
LibHsBaSlicer
├── Preprocess/    模型加载与变换
├── Slice/         指定高度的网格切片
├── Support/       FDM 支撑生成
├── Fill/          多边形填充模式
└── Path/          G-code 路径生成
```

## 使用方法

使用 LibHsBaSlicer 需要包含相应的头文件：

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
```

链接 `LibHsBaSlicer` 及其依赖项。

详细的 CMake 集成步骤请参考 [C++ 使用指南（CMake 集成）](../cpp_cmake_usage.md)。

## 典型工作流

1. **预处理**：通过 `LoadModel()` 加载模型，施加变换
2. **切片**：在每个层高位置通过 `Slice()` 生成层轮廓
3. **支撑**：通过 `GenerateAllFdmSupport()` 生成支撑结构
4. **填充**：通过 `FillPolygon()` 或 `FillWithBorder()` 填充层多边形
5. **路径**：通过 `GenerateGCodePath()` 生成 G-code 路径

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
