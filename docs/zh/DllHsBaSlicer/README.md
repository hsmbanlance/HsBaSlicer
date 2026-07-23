# DllHsBaSlicer 模块

DllHsBaSlicer 是 HsBaSlicer 的**上层 C 导出动态库**，将 [LibHsBaSlicer](../LibHsBaSlicer/) 的 C++ 切片核心封装为一组纯 C ABI 接口，供 **Qt / wxWidgets / Unity / Unreal Engine / C# / Java / Python / Swift** 等任意宿主语言跨平台调用。

内部基于 C++20 协程实现流水线调度，同时提供**同步**与**异步**两套调用方式。

## 设计定位

```
宿主应用（Qt / wxWidgets / Unity / UE / Android / iOS ...）
        │  纯 C ABI（extern "C"）
        ▼
DllHsBaSlicer  ← 本模块：C 导出层（dll / so / dylib / 静态库）
        │  仅调用 LibHsBaSlicer 导出函数
        ▼
LibHsBaSlicer  ← C++ 静态库：预处理 / 切片 / 支撑 / 填充 / 路径生成
```

约束：DllHsBaSlicer 只允许调用 LibHsBaSlicer 的导出接口，不直接触碰更底层模块，保证 ABI 边界清晰。

## 构建产物

| 平台 | 产物 | 说明 |
| --- | --- | --- |
| Windows | `DllHsBaSlicer.dll` + `.lib` 导入库 | 输出至 `bin/<CONFIG>/` |
| Linux | `libDllHsBaSlicer.so` | 共享库 |
| macOS | `libDllHsBaSlicer.dylib` | 共享库 |
| Android | `libDllHsBaSlicer.so` | 共享库，经 JNI 调用 |
| iOS | `libDllHsBaSlicer.a` | **静态库**（iOS 禁止加载任意 dylib） |

所有对外头文件统一导出至安装目录的 `include/HsBaSlicer/` 下。

## 头文件

| 头文件 | 内容 |
| --- | --- |
| `dllexport.h` | `HSBA_SLICER_API` 导出宏 |
| `initialize.h` | `initialize()` 全局初始化（必须最先调用） |
| `fdm_pipeline.h` | FDM 全流程接口 |
| `sla_pipeline.h` | SLA 全流程接口 |
| `sls_pipeline.h` | SLS 全流程接口 |
| `version_info.h` | 版本信息（JSON / XML 字符串） |
| `pipelinetypes/pipeline_types.h` | 全部配置/结果结构体、枚举、回调类型与内联默认值初始化器（**无 DLL 依赖**，可独立包含） |

## API 总览

### 初始化

```c
void initialize(void);   // 进程启动后调用一次
```

### FDM 流水线

预处理 → 切片 → 支撑 → 填充 → 路径生成，输出 G-code。

```c
HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                           HsBaProgressCallback callback, void* user_data);

void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config,
                             HsBaProgressCallback callback, void* user_data,
                             HsBaResultCallback result_callback, void* result_user_data);

void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);
```

### SLA 流水线

预处理 → 切片 → 地板/筏 → 支撑 → 导出（zip 层图），输出 PNG/JPG/SVG 层图压缩包。

```c
HsBaSlaPipelineConfig_t HsBaCreateDefaultSlaConfig(void);

HsBaSlaPipelineResult_t HsBaRunSlaPipeline(const HsBaSlaPipelineConfig_t* config,
                                           HsBaSlaProgressCallback callback, void* user_data);

void HsBaRunSlaPipelineAsync(const HsBaSlaPipelineConfig_t* config,
                             HsBaSlaProgressCallback callback, void* user_data,
                             HsBaSlaResultCallback result_callback, void* result_user_data);

void HsBaFreeSlaPipelineResult(HsBaSlaPipelineResult_t* result);
```

### SLS 流水线

预处理 → 切片 → Lua 脚本导出（无标准输出格式，完全由导出脚本决定）。

```c
HsBaSlsPipelineConfig_t HsBaCreateDefaultSlsConfig(void);

HsBaSlsPipelineResult_t HsBaRunSlsPipeline(const HsBaSlsPipelineConfig_t* config,
                                           HsBaSlsProgressCallback callback, void* user_data);

void HsBaRunSlsPipelineAsync(const HsBaSlsPipelineConfig_t* config,
                             HsBaSlsProgressCallback callback, void* user_data,
                             HsBaSlsResultCallback result_callback, void* result_user_data);

void HsBaFreeSlsPipelineResult(HsBaSlsPipelineResult_t* result);
```

> SLS 的 `export_lua_script` 字段**不可为 NULL**。

### 版本信息

```c
char* HsBaGetVersionJson(void);   // 用 HsBaFreeVersionString 释放
char* HsBaGetVersionXml(void);
void  HsBaFreeVersionString(char* str);
```

## 回调与线程模型

```c
typedef void (*HsBaProgressCallback)(int percent, const char* stage, void* user_data);
typedef void (*HsBaResultCallback)(HsBaFdmPipelineResult_t result, void* user_data);
```

- 进度与结果回调均在**库内部工作线程**上触发，**不会**在调用方 UI 线程执行；
- 宿主（Qt/wxWidgets/Unity/UE）收到回调后，需自行调度回 UI/游戏线程再更新界面；
- `stage` 为 UTF-8 编码字符串，仅在回调期间有效，如需留存请自行拷贝；
- 异步接口的 `config` 指针仅在调用期间被读取，返回后即可释放或复用。

## 内存管理规则

1. `HsBaCreateDefault*Config()` 返回**值类型**结构体，无需释放；字符串字段指向的内存由调用方保证生命周期；
2. 结果结构体中的 `gcode_content` / `export_path` / `error_message` 由库内部分配，**必须**调用对应的 `HsBaFree*PipelineResult()` 释放；
3. 版本字符串必须用 `HsBaFreeVersionString()` 释放；
4. `pipeline_types.h` 还提供无 DLL 依赖的内联初始化器 `HsBaFdmConfigDefault()` / `HsBaSlaConfigDefault()` / `HsBaSlsConfigDefault()`，便于纯头文件场景（如 P/Invoke 结构体对照）使用。

## 最小示例（C/C++）

```c
#include "initialize.h"
#include "fdm_pipeline.h"

static void OnProgress(int percent, const char* stage, void* ud) { /* ... */ }

int main(void)
{
    initialize();

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name  = "stanford_bunny";
    cfg.model_path  = "models/stanford_bunny.stl";
    cfg.output_path = "output/bunny.gcode";

    HsBaFdmPipelineResult_t r = HsBaRunFdmPipeline(&cfg, OnProgress, NULL);
    if (r.success) { /* 使用 r.gcode_content ... */ }
    HsBaFreePipelineResult(&r);
    return 0;
}
```

## 集成指南

- **[Qt / wxWidgets 桌面框架集成](./qt_wxwidgets_integration.md)** —— CMake 链接、工作线程、进度条、信号槽 / CallAfter 调度
- **[Unity / Unreal Engine 游戏引擎集成](./game_engine_integration.md)** —— C# P/Invoke、UE ThirdParty 模块、Blueprint 封装、各平台打包

## 相关示例

- `samples/FDM/` —— FDM 同步/异步、Lua 自定义支撑与填充完整示例
- `samples/SLA/` —— SLA 流水线与 Lua 自定义地板/支撑/导出示例
- `samples/SLS/` —— SLS 流水线与 Lua 导出示例
- `android/` —— Android JNI 调用示例工程
- `ios/HsBaSlicerExample/` —— iOS Swift 桥接调用示例
