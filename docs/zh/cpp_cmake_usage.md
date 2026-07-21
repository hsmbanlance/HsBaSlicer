# C++ 使用指南（CMake 集成）

本文档介绍如何在外部 C++ 项目中通过 CMake 使用 HsBaSlicer 的三个核心模块：

| 模块 | 类型 | 说明 |
| --- | --- | --- |
| **LibHsBaSlicer** | C++ 库（SHARED/STATIC） | 核心切片算法，支持传统头文件和 C++20 模块两种使用方式 |
| **ModuleHsBaSlicer** | C++20 模块包装层 | 类风格 API（Model/FdmPipeline/SlaPipeline），`import hsba.slicer;` |
| **DllHsBaSlicer** | C 导出库（SHARED/STATIC） | 纯 C ABI 流水线接口，适合跨语言调用 |
| **HsBaSlicer** | 可执行文件 / 平台库 | 应用程序入口（桌面为 exe，Android 为 .so，iOS 为 .a） |

---

## 前置条件

- **CMake** ≥ 3.28
- **C++20** 兼容编译器（MSVC 19.36+、GCC 13+、Clang 16+）
- 已安装 HsBaSlicer（`cmake --install`），或作为子目录引入

---

## 安装 HsBaSlicer

```bash
cmake --preset <preset-name>
cmake --build --preset <preset-name>
cmake --install build --prefix /path/to/install
```

安装后的目录布局：

```
<prefix>/
├── bin/                          # 可执行文件、DLL（Windows）
├── lib/                          # 静态库 / 导入库 / 共享库（Linux）
│   └── cmake/HsBaSlicer/        # CMake 包配置文件
│       ├── HsBaSlicerConfig.cmake
│       ├── HsBaSlicerConfigVersion.cmake
│       └── HsBaSlicerTargets.cmake
├── include/HsBaSlicer/           # 所有公开头文件
│   ├── LibHsBaSlicer/            # LibHsBaSlicer C++ API
│   ├── modules/                  # C++20 模块接口文件（.cppm）
│   ├── pipelinetypes/            # C 结构体定义
│   ├── 2D/                       # 多边形类型
│   ├── meshmodel/                # 网格模型
│   ├── paths/                    # 路径类型
│   ├── support/                  # 支撑配置
│   ├── convert/                  # Proto 转换
│   ├── proto/                    # Protobuf 生成头文件
│   ├── base/                     # 基础类型
│   └── logger/                   # 日志
└── share/HsBaSlicer/proto/       # Proto 多语言生成文件
```

---

## 方式一：find_package（推荐）

### 基本 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::LibHsBaSlicer)
```

配置时指定安装前缀：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/install
```

---

## 使用 LibHsBaSlicer（非模块版 — 传统头文件）

适用于所有支持 C++20 的编译器，无需模块支持。

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MySlicerApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::LibHsBaSlicer)
```

### 代码示例

```cpp
#include <LibHsBaSlicer/Preprocess/model_preprocess.hpp>
#include <LibHsBaSlicer/Slice/mesh_slice.hpp>
#include <LibHsBaSlicer/Support/fdm_support.hpp>
#include <LibHsBaSlicer/Fill/polygon_fill.hpp>
#include <LibHsBaSlicer/Path/path_generator.hpp>
#include <LibHsBaSlicer/Path/sls_export.hpp>
#include <LibHsBaSlicer/Floor/sla_floor.hpp>

using namespace HsBa::Slicer;

int main()
{
    // 1. 预处理：加载模型
    // auto model = LoadModel("model.stl");

    // 2. 切片：生成层轮廓
    // auto contours = Slice(model, layer_height);

    // 3. 支撑：生成 FDM 支撑
    // auto supports = GenerateAllFdmSupport(...);

    // 4. 填充：多边形填充
    // auto fill_paths = FillPolygon(...);

    // 5. 路径：生成 G-code
    // auto gcode = GenerateGCodePath(...);

    return 0;
}
```

### 可用目标

| CMake 目标 | 说明 |
| --- | --- |
| `HsBaSlicer::LibHsBaSlicer` | 核心 C++ 切片库 |
| `HsBaSlicer::ModuleHsBaSlicer` | C++20 模块包装层（类风格 API） |
| `HsBaSlicer::DllHsBaSlicer` | C ABI 导出层 |
| `HsBaSlicer::HsBaPipelineTypes` | 纯头文件类型定义（INTERFACE） |
| `HsBaSlicer::HsBaSlicerProto` | Protobuf 消息库 |
| `HsBaSlicer::HsBaSlicerConverter` | Proto ↔ 内部类型转换 |

---

## 使用 ModuleHsBaSlicer（C++20 模块版）

提供类风格的 C++ API（`Model`、`FdmPipeline`、`SlaPipeline` 等），支持异常、RAII。
需要编译器支持 C++20 模块（MSVC 19.34+、GCC 14+、Clang 16+）。

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyModuleApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::ModuleHsBaSlicer)
```

### 代码示例

```cpp
import hsba.slicer;

#include <iostream>
#include <format>

using namespace HsBa::Slicer;

int main()
{
    try
    {
        // RAII 模型管理（析构自动释放）
        Model model("bunny", "models/stanford_bunny.stl");
        auto info = model.info();
        std::cout << std::format("Volume: {:.2f} mm^3", info.volume) << std::endl;

        // FDM 全流程（使用 pipeline_types.h 配置）
        HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
        cfg.layer_height = 0.2f;
        cfg.fill_mode    = HSBA_FILL_ZIGZAG;
        cfg.output_path  = "output/result.gcode";

        FdmPipeline pipeline(cfg);
        FdmResult result = pipeline.run(model);

        std::cout << std::format("Layers: {}, G-code: {} bytes",
                                 result.total_layers, result.gcode.size()) << std::endl;
    }
    catch (const SlicerError& e)
    {
        std::cerr << "Slicer error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### 主要类

| 类 | 说明 |
| --- | --- |
| `Model` | RAII 模型句柄（加载/变换/切片/自动释放） |
| `FdmPipeline` | FDM 全流程（切片→支撑→填充→路径） |
| `SlaPipeline` | SLA 全流程（切片→支撑→底座→渲染→打包） |
| `SlsPipeline` | SLS Lua 导出 |
| `SlicerError` | 统一异常类型 |

### 平台注意事项

> **MSVC（Windows）**
>
> 1. **`#include` 必须在 `import` 之前**：MSVC 模块消费者中，标准库头文件的 `#include` 必须出现在 `import hsba.slicer;` 之前，否则 BMI 中的 std 声明与头文件冲突，产生 C2572 重定义错误。
> 2. **编译环境一致性（C5050）**：CGAL 通过 INTERFACE 属性传播 `/fp:strict` 和 `_SCL_SECURE_NO_WARNINGS`。`ModuleHsBaSlicer` 已将这些标志设为 PUBLIC，消费者链接时自动继承，无需手动设置。
> 3. **静态库**：`ModuleHsBaSlicer` 始终构建为 STATIC（避免 DLL 链接阶段对 `oleaut32.lib` 等系统库的路径依赖）。消费者通过 PUBLIC 传递获得完整依赖链。

> **Clang / GCC（Linux / Android NDK）**
>
> 1. **GMF 类型不随模块导出**（C++20 标准限制）：模块全局模块片段（GMF）中通过 `#include` 引入的类型（如 `ModelInfo`、`LayerPathData`、`PointsPath` 等）**不会**对消费者可见。消费者需自行包含相应头文件：
>    ```cpp
>    #include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"  // ModelInfo
>    #include "pipelinetypes/pipeline_types.h"                 // HsBaFdmPipelineConfig_t 等
>    ```
> 2. 标准库头文件无此限制，`#include` 和 `import` 的顺序在 Clang/GCC 下不敏感。

> **通用**
>
> - 流水线配置结构体（`HsBaFdmPipelineConfig_t`、`HsBaSlaPipelineConfig_t`、`HsBaSlsPipelineConfig_t`）统一定义在 `pipelinetypes/pipeline_types.h`，模块版与头文件版共用同一份定义，禁止重复定义。
> - 模块为单文件实现（`ModuleHsBaSlicer/hsba_slicer.cppm`），接口与实现合并以避免 MSVC 实现单元隐式导入引发的 C2572 错误。

---

## 使用 DllHsBaSlicer（C ABI 接口）

适合从 C/C++ 调用完整流水线，或作为跨语言桥接层（C#/Java/Python/Swift 等）。

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyPipelineApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::DllHsBaSlicer)
```

### 代码示例

```cpp
#include <initialize.h>
#include <fdm_pipeline.h>
#include <sla_pipeline.h>
#include <sls_pipeline.h>

static void OnProgress(int percent, const char* stage, void* /*ud*/)
{
    // 进度通知
}

int main()
{
    // 必须首先调用初始化
    initialize();

    // FDM 流水线
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name  = "my_model";
    cfg.model_path  = "models/my_model.stl";
    cfg.output_path = "output/my_model.gcode";

    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);
    if (result.success)
    {
        // 使用 result.gcode_content ...
    }
    HsBaFreePipelineResult(&result);

    return 0;
}
```

### 仅使用类型定义（无 DLL 依赖）

若只需结构体/枚举定义（如 P/Invoke 对照），可仅链接头文件目标：

```cmake
target_link_libraries(my_app PRIVATE HsBaSlicer::HsBaPipelineTypes)
```

```cpp
#include <pipelinetypes/pipeline_types.h>
// 使用 HsBaFdmPipelineConfig_t 等结构体，无需链接任何库
```

---

## 使用 HsBaSlicer 可执行目标

`HsBaSlicer` 是最终应用程序入口。在桌面平台构建为可执行文件，在 Android 上为共享库（.so），在 iOS 上为静态库（.a）。

通常不需要在外部项目中链接此目标。若需以子目录方式集成整个项目：

### 子目录集成（add_subdirectory）

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 HsBaSlicer 整个项目
add_subdirectory(third_party/HsBaSlicer)

# 链接所需目标
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE LibHsBaSlicer)        # 非模块版
# 或
target_link_libraries(my_app PRIVATE ModuleHsBaSlicer)     # C++20 模块版
# 或
target_link_libraries(my_app PRIVATE DllHsBaSlicer)        # C ABI 版
```

> **注意**：子目录方式下目标名称不带 `HsBaSlicer::` 命名空间前缀。

---

## 构建选项参考

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `ON` | 构建共享库（OFF 则为静态库） |
| `HSBA_SLICER_MODULE` | `ON` | 构建 ModuleHsBaSlicer C++20 模块包装层 |
| `HSBA_SLICER_BUILD_SAMPLES` | `ON` | 构建示例程序 |
| `HSBA_SLICER_USE_TESTS` | `ON` | 构建测试 |
| `HSBA_PROTOBUF_OUT` | `ON` | 输出 Proto 多语言生成文件 |

---

## 传递依赖

`find_package(HsBaSlicer)` 会自动查找以下公共依赖：

- Eigen3
- magic_enum
- Clipper2
- Lua
- Protobuf
- OpenSSL

请确保这些包在 `CMAKE_PREFIX_PATH` 中可被找到（通常由 vcpkg 工具链自动处理）。

---

## 完整示例项目结构

```
my_project/
├── CMakeLists.txt
├── main.cpp
└── vcpkg.json          # 若使用 vcpkg 管理依赖
```

**CMakeLists.txt**：

```cmake
cmake_minimum_required(VERSION 3.28)
project(MySlicerApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::LibHsBaSlicer)
```

---

## 相关示例

- `samples/LibHsBaSlicer/main_header.cpp` — 完整示例（传统 #include，全流程 FDM 切片）
- `samples/LibHsBaSlicer/main_module.cpp` — C++20 模块版示例（`import hsba.slicer;`，类风格 API）
- `samples/FDM/` — DllHsBaSlicer C ABI 同步/异步、Lua 自定义示例
- `samples/SLA/` — SLA 流水线示例
- `samples/SLS/` — SLS 流水线示例

## 相关文档

- [LibHsBaSlicer 模块](./LibHsBaSlicer/) — 核心 C++ 切片 API 详解
- [DllHsBaSlicer 模块](./DllHsBaSlicer/) — C ABI 流水线接口与跨平台集成
- [Qt / wxWidgets 集成](./DllHsBaSlicer/qt_wxwidgets_integration.md)
- [Unity / Unreal Engine 集成](./DllHsBaSlicer/game_engine_integration.md)
