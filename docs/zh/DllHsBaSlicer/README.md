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
| `model_preprocess.h` | 模型预处理接口（加载/变换/查询/布尔运算/抽壳） |
| `fdm_pipeline.h` | FDM 全流程接口 |
| `sla_pipeline.h` | SLA 全流程接口 |
| `sls_pipeline.h` | SLS 全流程接口 |
| `file_transfer_pipeline.h` | 文件传输流水线接口（同步/异步） |
| `pipeline_convert.h` | Proto 序列化字节 ↔ C 结构体转换 |
| `lua_register.h` | Lua 扩展函数注册接口（2D/3D/File/事件回调） |
| `version_info.h` | 版本信息（JSON / XML 字符串） |
| `pipelinetypes/pipeline_types.h` | 全部配置/结果结构体、枚举、回调类型与内联默认值初始化器（**无 DLL 依赖**，可独立包含） |

## API 总览

### 初始化

```c
void initialize(void);   // 进程启动后调用一次
```

### 模型预处理

独立的模型管理接口，支持在流水线运行前/外单独操作模型。模型通过不透明句柄（`void*`）引用，内部引用计数管理生命周期。

#### 基本操作

```c
void* HsBaLoadModel(const char* name, const char* file_path);  // 加载模型（IGL: STL/OBJ/PLY/OFF; OCCT: STEP/IGES/VRML/BREP）
void* HsBaGetModel(const char* name);                          // 获取已加载模型
void  HsBaRemoveModel(const char* name);                       // 从池中移除模型
int   HsBaContainsModel(const char* name);                     // 检查模型是否存在
int   HsBaModelCount(void);                                    // 池中模型数量
int   HsBaCleanupModels(void);                                 // 清理无外部引用的模型
```

#### 变换操作

```c
int HsBaTranslateModel(const char* name, float tx, float ty, float tz);       // 平移
int HsBaRotateModel(const char* name, float qx, float qy, float qz, float qw); // 旋转（四元数）
int HsBaScaleModelUniform(const char* name, float scale);                      // 等比缩放
int HsBaScaleModel(const char* name, float sx, float sy, float sz);            // 非等比缩放
```

#### 查询操作

```c
int HsBaGetModelInfo(const char* name, float out_bbox_min[3], float out_bbox_max[3], float* out_volume);
```

#### 高级操作（需要 CGAL/OCCT）

```c
void* HsBaThickSolidModel(const char* source_name, const char* result_name, float thickness);  // 抽壳（需 OCCT BRep 模型）
void* HsBaBooleanUnion(const char* left_name, const char* right_name, const char* result_name);        // 布尔并集
void* HsBaBooleanIntersection(const char* left_name, const char* right_name, const char* result_name); // 布尔交集
void* HsBaBooleanDifference(const char* left_name, const char* right_name, const char* result_name);   // 布尔差集
void* HsBaBooleanXor(const char* left_name, const char* right_name, const char* result_name);          // 布尔异或
```

#### 句柄管理

```c
void HsBaReleaseModelHandle(void* handle);  // 释放句柄引用（模型仍留在池中）
```

> **内核路由策略**：布尔运算优先使用 OCCT（BRep-BRep），若模型为网格则回退到 IGL/CGAL；抽壳仅支持 OCCT BRep 模型。高级操作在编译期由 `USE_CGAL` 宏控制，不可用时返回 NULL。

#### 模型预处理示例

```c
#include "initialize.h"
#include "model_preprocess.h"

int main(void)
{
    initialize();

    // Load mesh model (IGL)
    void* h1 = HsBaLoadModel("part_a", "models/part_a.stl");
    void* h2 = HsBaLoadModel("part_b", "models/part_b.stl");

    // Transform
    HsBaTranslateModel("part_b", 10.0f, 0.0f, 0.0f);

    // Boolean union (IGL/CGAL fallback for mesh)
    void* merged = HsBaBooleanUnion("part_a", "part_b", "merged");

    // Query
    float bmin[3], bmax[3], vol;
    HsBaGetModelInfo("merged", bmin, bmax, &vol);

    // Release handles
    HsBaReleaseModelHandle(h1);
    HsBaReleaseModelHandle(h2);
    HsBaReleaseModelHandle(merged);

    // Cleanup when done
    HsBaCleanupModels();
    return 0;
}
```

### FDM 流水线

预处理 → 切片 → 支撑 → 填充 → 路径生成，输出标准 3D 打印机 G-code（支持 Marlin / RepRap / Klipper 固件格式）。

```c
HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                           HsBaProgressCallback callback, void* user_data);

void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config,
                             HsBaProgressCallback callback, void* user_data,
                             HsBaResultCallback result_callback, void* result_user_data);

void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);
```

#### GCode 固件选择

通过 `gcode_firmware` 字段指定目标固件，输出对应规范的 GCode：

| 枚举值 | 固件 | 特性 |
| --- | --- | --- |
| `HSBA_GCODE_MARLIN` | Marlin（默认） | M104/M109 温度等待、G92 E0、M82/M83 |
| `HSBA_GCODE_REPRAP` | RepRap/RRF | 额外 M106 风扇控制 |
| `HSBA_GCODE_KLIPPER` | Klipper | SET_PRESSURE_ADVANCE、M220/M221、SET_FAN_SPEED |

#### 打印机配置字段（新增）

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `gcode_firmware` | `HSBA_GCODE_MARLIN` | 目标固件类型 |
| `nozzle_diameter` | 0.4 | 喷嘴直径 (mm) |
| `filament_diameter` | 1.75 | 耗材直径 (mm) |
| `nozzle_temp` | 200.0 | 喷嘴温度 (°C) |
| `bed_temp` | 60.0 | 热床温度 (°C) |
| `retract_length` | 1.0 | 回抽长度 (mm) |
| `retract_speed` | 40.0 | 回抽速度 (mm/s) |
| `first_layer_speed` | 20.0 | 首层速度 (mm/s) |

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

### 文件传输流水线

校验 → 连接池建立 → 逐文件传输，将本地文件发送至远程执行器服务。

```c
HsBaFileTransferPipelineConfig_t HsBaCreateDefaultFileTransferConfig(void);

HsBaFileTransferPipelineResult_t HsBaRunFileTransferPipeline(
    const HsBaFileTransferPipelineConfig_t* config,
    HsBaFileTransferProgressCallback callback, void* user_data);

void HsBaRunFileTransferPipelineAsync(
    const HsBaFileTransferPipelineConfig_t* config,
    HsBaFileTransferProgressCallback callback, void* user_data,
    HsBaFileTransferResultCallback result_callback, void* result_user_data);

void HsBaFreeFileTransferPipelineResult(HsBaFileTransferPipelineResult_t* result);
```

#### 配置字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `host` | NULL | 远程主机地址 |
| `port` | NULL | 远程服务端口 |
| `pool_size` | 4 | 连接池大小 [1, 16] |
| `file_paths` | NULL | 待传输文件路径数组 |
| `file_count` | 0 | 文件数量 |

### Proto 序列化转换

提供 C 结构体与 Protobuf 序列化字节之间的双向转换，适用于跨进程 / 跨语言通信场景。所有输出缓冲区由 `malloc` 分配，调用方负责 `free`。

```c
// FDM
int HsBaFdmConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFdmPipelineConfig_t* config);
int HsBaFdmConfigToProtoBytes(const HsBaFdmPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaFdmResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFdmPipelineResult_t* result);
int HsBaFdmResultToProtoBytes(const HsBaFdmPipelineResult_t* result, void** out_data, int* out_size);

// SLA
int HsBaSlaConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlaPipelineConfig_t* config);
int HsBaSlaConfigToProtoBytes(const HsBaSlaPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaSlaResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlaPipelineResult_t* result);
int HsBaSlaResultToProtoBytes(const HsBaSlaPipelineResult_t* result, void** out_data, int* out_size);

// SLS
int HsBaSlsConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlsPipelineConfig_t* config);
int HsBaSlsConfigToProtoBytes(const HsBaSlsPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaSlsResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlsPipelineResult_t* result);
int HsBaSlsResultToProtoBytes(const HsBaSlsPipelineResult_t* result, void** out_data, int* out_size);

// File Transfer
int HsBaFileTransferConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineConfig_t* config);
int HsBaFileTransferConfigToProtoBytes(const HsBaFileTransferPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaFileTransferResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineResult_t* result);
int HsBaFileTransferResultToProtoBytes(const HsBaFileTransferPipelineResult_t* result, void** out_data, int* out_size);

// 内存释放
void HsBaFreeFdmConfigStrings(HsBaFdmPipelineConfig_t* config);
void HsBaFreeSlaConfigStrings(HsBaSlaPipelineConfig_t* config);
void HsBaFreeSlsConfigStrings(HsBaSlsPipelineConfig_t* config);
void HsBaFreeFileTransferConfigStrings(HsBaFileTransferPipelineConfig_t* config);
```

> Proto 消息定义位于 `proto/` 目录（`fdm_pipeline.proto`、`sla_pipeline.proto`、`sls_pipeline.proto`、`file_transfer_pipeline.proto`），支持 C++/C#/Java/Python/PHP 多语言输出。

### 版本信息

```c
char* HsBaGetVersionJson(void);   // 用 HsBaFreeVersionString 释放
char* HsBaGetVersionXml(void);
void  HsBaFreeVersionString(char* str);
```

### Lua 扩展函数注册

在流水线运行前注册外部 Lua 函数，各阶段创建 Lua 环境时自动注入：

```c
typedef void (*HsBaLuaRegFn)(lua_State*);

void HsBaAdd2DFunction(HsBaLuaRegFn func);       // 2D（Support、Fill、SLA Output）
void HsBaAdd3DFunction(HsBaLuaRegFn func);       // 3D（Slice、Support）
void HsBaAddFileFunction(HsBaLuaRegFn func);     // File（SLS Output、SLA Output）
void HsBaAddEventCallback(const char* event_name, HsBaLuaRegFn func);  // 事件回调
```

示例：

```c
#include "initialize.h"
#include "lua_register.h"
#include <lua.hpp>

static int my_custom_func(lua_State* L) {
    // 自定义实现
    return 0;
}

static void register_my_functions(lua_State* L) {
    lua_register(L, "my_custom_func", my_custom_func);
}

int main(void) {
    initialize();
    HsBaAdd3DFunction(register_my_functions);  // 注册到切片/支撑阶段
    // ... 运行流水线
    return 0;
}
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
4. 模型句柄（`HsBaLoadModel` / `HsBaGetModel` / `HsBaBoolean*` / `HsBaThickSolidModel` 返回的 `void*`）必须用 `HsBaReleaseModelHandle()` 释放引用；
5. `pipeline_types.h` 还提供无 DLL 依赖的内联初始化器 `HsBaFdmConfigDefault()` / `HsBaSlaConfigDefault()` / `HsBaSlsConfigDefault()` / `HsBaFileTransferConfigDefault()`，便于纯头文件场景（如 P/Invoke 结构体对照）使用；
6. Proto 反序列化（`*FromProtoBytes`）产生的字符串字段由 `malloc` 分配，必须调用对应的 `HsBaFree*ConfigStrings()` 释放；`*ToProtoBytes` 产生的 `out_data` 缓冲区由调用方 `free`。

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
    cfg.gcode_firmware = HSBA_GCODE_MARLIN;  // 可选: HSBA_GCODE_REPRAP, HSBA_GCODE_KLIPPER
    cfg.nozzle_temp = 210.0f;
    cfg.bed_temp    = 60.0f;

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
