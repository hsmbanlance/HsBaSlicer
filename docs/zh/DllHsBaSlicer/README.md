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
| `slm_pipeline.h` | SLM 金属粉末床全流程接口 |
| `lom_pipeline.h` | LOM 叠层实体全流程接口 |
| `tdp_pipeline.h` | 3DP 粘结剂喷射全流程接口 |
| `waam_pipeline.h` | WAAM 电弧增材（机器人）全流程接口 |
| `file_transfer_pipeline.h` | 文件传输流水线接口（同步/异步） |
| `param_store_pipeline.h` | 工艺参数存储流水线接口（写入/读取） |
| `custom_pipeline.h` | 自定义 Lua 流水线接口（同步/异步） |
| `pipeline_convert.h` | Proto 序列化字节 ↔ C 结构体转换 |
| `lua_register.h` | Lua 扩展函数注册接口（2D/3D/File/事件回调） |
| `event_source_register.h` | C++ 事件源注册接口（Zipper 进度 / DB 事件） |
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
| `spiral_mode` | 0 | 外壁螺旋连续化（花瓶模式，0=false, 1=true）；启用后外轮廓输出为单条挤出连续的爬升螺旋线，并跳过逐层填充与支撑 |

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

### SLM 流水线

金属粉末床熔融（激光 / 电子束选区熔化）。流程与 SLS 完全一致（预处理 → 切片 → Lua 脚本导出 zip + 数据库登记，无地板/支撑），额外携带金属专属参数（材料、能量源、保护气），随配置一并交给导出脚本。

```c
HsBaSlmPipelineConfig_t HsBaCreateDefaultSlmConfig(void);

HsBaSlmPipelineResult_t HsBaRunSlmPipeline(const HsBaSlmPipelineConfig_t* config,
                                           HsBaSlmProgressCallback callback, void* user_data);

void HsBaRunSlmPipelineAsync(const HsBaSlmPipelineConfig_t* config,
                             HsBaSlmProgressCallback callback, void* user_data,
                             HsBaSlmResultCallback result_callback, void* result_user_data);

void HsBaFreeSlmPipelineResult(HsBaSlmPipelineResult_t* result);
```

#### 配置字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.06 / 0.08 | 层高 / 首层高 (mm) |
| `laser_power` | 200.0 | 激光功率 (W) |
| `scan_speed` | 1000.0 | 扫描速度 (mm/s) |
| `hatch_spacing` / `hatch_rotation` | 0.1 / 67.0 | 填充线距 (mm) / 层间旋转角 (°) |
| `bed_temperature` | 100.0 | 粉末床温度 (°C) |
| `material` | `HSBA_SLM_MATERIAL_TITANIUM` | 金属粉末：IRON / ALUMINUM / TITANIUM / UNKNOWN |
| `light_source` | `HSBA_SLM_LIGHT_LASER` | 能量源：LASER / EBEAM / UNKNOWN |
| `protect_gas` | `HSBA_METAL_GAS_ARGON` | 保护气：ARGON / HELIUM / N2 / CO2 / UNKNOWN |
| `export_lua_script` | NULL | 导出 Lua 脚本路径（**不可为 NULL**） |
| `export_lua_func` | NULL | 导出函数名，NULL 时为 `export_slm` |
| `output_path` | NULL | 输出路径 |

### LOM 流水线

叠层实体制造（薄片逐层粘结 + 激光切割）。按片厚逐层切片，把每层轮廓及切割 / 粘结参数交给 Lua 导出脚本。

```c
HsBaLomPipelineConfig_t HsBaCreateDefaultLomConfig(void);

HsBaLomPipelineResult_t HsBaRunLomPipeline(const HsBaLomPipelineConfig_t* config,
                                           HsBaLomProgressCallback callback, void* user_data);

void HsBaRunLomPipelineAsync(const HsBaLomPipelineConfig_t* config,
                             HsBaLomProgressCallback callback, void* user_data,
                             HsBaLomResultCallback result_callback, void* result_user_data);

void HsBaFreeLomPipelineResult(HsBaLomPipelineResult_t* result);
```

#### 配置字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.2 / 0.2 | 片厚 / 首片厚 (mm) |
| `cut_speed` / `cut_power` / `cut_margin` | 300.0 / 0.8 / 0.5 | 激光切割速度 (mm/s) / 功率 [0,1] / 轮廓偏移 (mm) |
| `bond_temperature` / `bond_pressure` / `bond_time` | 150.0 / 1.0 / 5.0 | 粘结温度 (°C) / 压力 (MPa) / 每层时间 (s) |
| `seal_contour` | 1 | 零件边缘封边 (0=false, 1=true) |
| `cut_mode` | `HSBA_LOM_CUT_CONTOUR` | 切割模式：CONTOUR（轮廓切割）/ HALFTONE（半调切割） |
| `export_lua_script` | NULL | 导出 Lua 脚本路径（**不可为 NULL**） |
| `export_lua_func` | NULL | 导出函数名，NULL 时为 `export_lom` |
| `output_path` | NULL | 输出路径 |

### 3DP 流水线

粘结剂喷射（粉末床 + 液体粘结剂）。逐层切片，把每层喷头点阵轮廓及喷头 / 固化参数交给 Lua 导出脚本。

```c
HsBaTdpPipelineConfig_t HsBaCreateDefaultTdpConfig(void);

HsBaTdpPipelineResult_t HsBaRunTdpPipeline(const HsBaTdpPipelineConfig_t* config,
                                           HsBaTdpProgressCallback callback, void* user_data);

void HsBaRunTdpPipelineAsync(const HsBaTdpPipelineConfig_t* config,
                             HsBaTdpProgressCallback callback, void* user_data,
                             HsBaTdpResultCallback result_callback, void* result_user_data);

void HsBaFreeTdpPipelineResult(HsBaTdpPipelineResult_t* result);
```

#### 配置字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.1 / 0.12 | 层高 / 首层高 (mm) |
| `head_count` | 128 | 打印喷头喷嘴数 |
| `drop_spacing` | 0.05 | 粘结剂墨点间距 (mm) |
| `binder_saturation` | 0.6 | 粘结剂饱和度 [0,1] |
| `ink_curing_time` | 1.0 | 每层固化时间 (s) |
| `bed_temperature` | 40.0 | 粉末床温度 (°C) |
| `binder_mode` | `HSBA_TDP_SINGLE` | 模式：FULL_COLOR（全彩）/ SINGLE（单色）/ SINTERING（烧结） |
| `spiral_mode` | 0 | 外轮廓螺旋连续化 (0=false, 1=true) |
| `export_lua_script` | NULL | 导出 Lua 脚本路径（**不可为 NULL**） |
| `export_lua_func` | NULL | 导出函数名，NULL 时为 `export_tdp` |
| `output_path` | NULL | 输出路径 |

### WAAM 流水线

电弧增材制造（机器人逐道金属熔敷）。与粉床工艺根本不同：输出为**机器人语言程序**（ABB / KUKA / FANUC），而非层图压缩包。`UNKNOWN` 机器人型号需提供 Lua 路径脚本自定义代码生成。

```c
HsBaWaamPipelineConfig_t HsBaCreateDefaultWaamConfig(void);

HsBaWaamPipelineResult_t HsBaRunWaamPipeline(const HsBaWaamPipelineConfig_t* config,
                                             HsBaWaamProgressCallback callback, void* user_data);

void HsBaRunWaamPipelineAsync(const HsBaWaamPipelineConfig_t* config,
                              HsBaWaamProgressCallback callback, void* user_data,
                              HsBaWaamResultCallback result_callback, void* result_user_data);

void HsBaFreeWaamPipelineResult(HsBaWaamPipelineResult_t* result);
```

#### 配置字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.8 / 1.0 | 道高/层高 / 首层高 (mm) |
| `bead_width` | 1.2 | 熔敷焊道宽度 (mm) |
| `travel_speed` / `wire_feed_speed` | 8.0 / 5.0 | 焊枪行走速度 (mm/s) / 送丝速度 (m/min) |
| `arc_current` / `arc_voltage` | 180.0 / 22.0 | 焊接电流 (A) / 电弧电压 (V) |
| `gas_flow_rate` | 15.0 | 保护气流量 (L/min) |
| `material` | `HSBA_WAAM_MATERIAL_STEEL` | 材料：STEEL / ALUMINUM / TITANIUM / COPPER / UNKNOWN |
| `welding_process` | `HSBA_WAAM_WELD_ARC` | 工艺：ARC（电弧）/ LASER（激光）/ UNKNOWN |
| `protection` / `protect_gas` | `SHIELD_GAS` / `ARGON` | 保护方式（屏蔽气/真空）/ 保护气体 |
| `interpass_temperature` | 100.0 | 道间温度 (°C) |
| `robot_type` | `HSBA_WAAM_ROBOT_ABB` | 机器人：ABB / KUKA / FANUC / UNKNOWN（需 Lua 路径脚本） |
| `path_lua_script` / `path_lua_func` | NULL | 自定义机器人代码生成脚本与函数名（可选，NULL 时内置，默认 `export_waam`） |
| `spiral_mode` | 0 | 外壁单道连续爬升熔敷 (0=false, 1=true) |
| `output_path` | NULL | 输出机器人程序路径 |

> WAAM 结果结构体的输出字段为 `output_path`（机器人程序路径），而非其余流水线的 `export_path`。

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

### 工艺参数存储流水线

把各流水线的工艺参数（`HsBa*PipelineConfig_t` 结构体）持久化到数据库以便**复用**：按业务唯一键 upsert 写入（Save）、按键回填读取（Load）。底层复用 fileoperator 的 ParamStore 反射/类型收敛机制，按工艺类型分宽表存储，无需为每种工艺单独建表。同步接口，单次调用内部完成「建连 → 建表 → 执行 → 断开」。

```c
HsBaParamStoreResult_t HsBaSavePipelineParams(const HsBaParamStoreConn_t* conn, HsBaPipelineKind kind,
                                              const char* table, const char* key, const void* config);

HsBaParamStoreResult_t HsBaLoadPipelineParams(const HsBaParamStoreConn_t* conn, HsBaPipelineKind kind,
                                              const char* table, const char* key, void* out_config);

void HsBaFreeLoadedPipelineConfig(HsBaPipelineKind kind, void* config);
void HsBaFreeParamStoreResult(HsBaParamStoreResult_t* result);
```

#### 连接结构 `HsBaParamStoreConn_t`

| 字段 | 说明 |
| --- | --- |
| `backend` | 存储后端：`HSBA_PARAM_BACKEND_SQLITE` / `_MYSQL` / `_POSTGRESQL`（Android/iOS 仅 SQLite） |
| `sqlite_path` | SQLite 数据库文件路径（SQLite 后端使用） |
| `host` / `port` / `user` / `password` / `database` | MySQL/PostgreSQL 连接参数；`port` 传 `0` 表示使用适配器默认端口 |

#### 参数说明

| 参数 | 说明 |
| --- | --- |
| `kind` | 配置类型 `HsBaPipelineKind`（FDM/SLA/SLS/SLM/LOM/TDP/WAAM/CUSTOM/FILETRANSFER），须与 `config` 指向的结构体一致 |
| `table` | 目标表名；传 `NULL` 或 `""` 时按 `kind` 派生默认表名（如 `hsba_param_fdm`） |
| `key` | 业务唯一键（如模板名），不可为 `NULL` |
| `config` / `out_config` | 指向对应的 `HsBa*PipelineConfig_t` 结构体；读取前建议先用 `HsBa*ConfigDefault()` 初始化 |

#### 结果结构 `HsBaParamStoreResult_t`

| 字段 | 说明 |
| --- | --- |
| `success` | `0`/`1`；读取未命中或后端不可用时为 `0`（不吞错误码） |
| `param_id` | Save 落库行 id；Load 命中回填原 id |
| `error_message` | UTF-8 错误信息，用 `HsBaFreeParamStoreResult` 释放 |
| `elapsed_seconds` | 本次调用耗时（秒） |

> **字符串所有权**：`HsBaLoadPipelineParams` 回填到 `out_config` 内的 `const char*` 字段由库 `malloc` 分配，`NULL` 字段保持 `NULL`；在复用或丢弃该结构体前必须调用 `HsBaFreeLoadedPipelineConfig(kind, &cfg)` 释放。`error_message` 独立由 `HsBaFreeParamStoreResult` 释放。

> **后端可用性**：MySQL/PostgreSQL 分支在编译时受 `HSBA_USE_MYSQL` / `HSBA_USE_PGSQL` 控制；未编译进时对应后端调用返回 `success=0` 并给出错误信息，不会崩溃。

#### 调用示例

```c
HsBaParamStoreConn_t conn = {0};
conn.backend = HSBA_PARAM_BACKEND_SQLITE;
conn.sqlite_path = "params.db";

HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault();
cfg.layer_height = 0.2f;
cfg.model_name = "tough_template";

// 写入（按 key upsert）
HsBaParamStoreResult_t rw = HsBaSavePipelineParams(&conn, HSBA_PIPELINE_FDM, NULL, "tough_template", &cfg);
HsBaFreeParamStoreResult(&rw);

// 读取（按 key 回填）
HsBaFdmPipelineConfig_t out = HsBaFdmConfigDefault();
HsBaParamStoreResult_t rd = HsBaLoadPipelineParams(&conn, HSBA_PIPELINE_FDM, NULL, "tough_template", &out);
if (rd.success) {
    // 使用 out.layer_height / out.model_name ...
}
HsBaFreeLoadedPipelineConfig(HSBA_PIPELINE_FDM, &out);  // 释放库 malloc 的字符串
HsBaFreeParamStoreResult(&rd);
```

### 自定义 Lua 流水线

与 FDM/SLA/SLS（阶段顺序在 C++ 中固定，Lua 只能替换个别阶段）不同，Custom 流水线的**整条工作流由 Lua 脚本决定**：C++ 侧只负责构造 Lua 环境、把全部流水线算子挂在全局表 `HsBa` 上，然后调用脚本里的入口函数。需要新增工艺时只改脚本，不必重新编译库。

```c
HsBaCustomPipelineConfig_t HsBaCreateDefaultCustomConfig(void);

HsBaCustomPipelineResult_t HsBaRunCustomPipeline(const HsBaCustomPipelineConfig_t* config,
                                                 HsBaCustomProgressCallback callback, void* user_data);

void HsBaRunCustomPipelineAsync(const HsBaCustomPipelineConfig_t* config,
                                HsBaCustomProgressCallback callback, void* user_data,
                                HsBaCustomResultCallback result_callback, void* result_user_data);

void HsBaFreeCustomPipelineResult(HsBaCustomPipelineResult_t* result);
```

#### 配置字段

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `pipeline_lua_script` | NULL | 流水线 Lua 脚本路径 |
| `pipeline_lua_source` | NULL | 内联 Lua 源码，**先于**脚本文件执行（可作为参数预置） |
| `entry_func` | NULL | 入口函数名，NULL 时为 `run_pipeline` |
| `config_json` | NULL | 任意 JSON 字符串，脚本中以 `pipeline_config` 读取 |
| `model_name` / `model_path` | NULL | 模型名与路径，脚本中以 `model_name` / `model_path` 读取 |
| `output_path` | NULL | 默认输出路径，脚本中以 `output_path` 读取 |

> `pipeline_lua_script` 与 `pipeline_lua_source` 至少提供一个。上述字段都可以通过 Proto 字节流下发，见下方 [Proto 序列化转换](#proto-序列化转换)。

#### 脚本环境

注入的全局变量：`HsBa`（算子表）、`model_name`、`model_path`、`output_path`、`pipeline_config`、`pipeline_entry`；同时可用项目注册池中的 `PolygonOperations`、`Support`、`PolygonFill`、`PathOptimize`、`Zipper`、`Cipher`、`SQLiteAdapter` 等库。

`HsBa` 算子（坐标单位为 mm）：

| 分组 | 算子 |
| --- | --- |
| 回报 | `progress(pct[, stage])`、`setLayers(n)`、`setOutputPath(path)` |
| 文件 | `readFile(path)`、`writeFile(path, content)` |
| 模型 | `loadModel(n, path)`、`modelInfo(n)`、`translateModel`、`rotateModel`、`scaleModel`、`removeModel`、`modelNames` |
| 切片 | `layerCount(n, lh, flh)`、`layerZ(i, lh, flh)`、`slice(n, z)`、`sliceUnsafe(n, z)`、`toInt`、`toDouble` |
| 工艺 | `fill(polys[, cfg])`、`fdmSupport(layers, cfg)`、`slaSupport(layers, cfg)`、`floor(bottom, cfg)` |
| 输出 | `toGcode(layers, cfg)`、`saveSlaPackage(tbl)`、`saveSlsPackage(tbl)`、`renderImage(polys, w, h, path)` |

入口函数返回任意真值表示成功（字符串会经 `result_string` 回传），返回 `false`/`nil` 或抛出 Lua 错误表示失败；`total_layers`、`output_path` 由脚本通过 `HsBa.setLayers()` / `HsBa.setOutputPath()` 回报。

#### Proto 方式调用

跨进程 / 跨语言场景下，不必在边界上逐个传递字符串字段：把请求序列化成 `custom_pipe_config` 的 wire 字节，接收端用 C 接口还原成配置结构后照常执行，再把结果转成 `custom_pipe_result` 字节回传。

```c
#include "pipeline_convert.h"

// 1. 收到对端发来的 custom_pipe_config 字节
HsBaCustomPipelineConfig_t cfg = HsBaCustomConfigDefault();
if (!HsBaCustomConfigFromProtoBytes(buf, size, &cfg)) { /* 解析失败 */ }

// 2. 与直接赋值字段的调用方式完全一致
HsBaCustomPipelineResult_t r = HsBaRunCustomPipeline(&cfg, OnProgress, NULL);

// 3. 释放反序列化出的字符串，并把结果回传
HsBaFreeCustomConfigStrings(&cfg);
void* out_buf = NULL; int out_size = 0;
HsBaCustomResultToProtoBytes(&r, &out_buf, &out_size);  /* 发送 out_buf[0, out_size) */
free(out_buf);
HsBaFreeCustomPipelineResult(&r);
```

流水线定义本身（阶段顺序、算子组合）仍留在 Lua 脚本里，Proto 只负责运送脚本路径 / 内联源码与模型、输出等入参；因此 `custom_pipe_config` 字段比 FDM/SLA/SLS 少得多。内联源码字段 `pipeline_lua_source` 可以携带整条流水线，实现“无文件部署”。

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

// SLM
int HsBaSlmConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlmPipelineConfig_t* config);
int HsBaSlmConfigToProtoBytes(const HsBaSlmPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaSlmResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlmPipelineResult_t* result);
int HsBaSlmResultToProtoBytes(const HsBaSlmPipelineResult_t* result, void** out_data, int* out_size);

// LOM
int HsBaLomConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaLomPipelineConfig_t* config);
int HsBaLomConfigToProtoBytes(const HsBaLomPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaLomResultFromProtoBytes(const void* proto_data, int proto_size, HsBaLomPipelineResult_t* result);
int HsBaLomResultToProtoBytes(const HsBaLomPipelineResult_t* result, void** out_data, int* out_size);

// 3DP
int HsBaTdpConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaTdpPipelineConfig_t* config);
int HsBaTdpConfigToProtoBytes(const HsBaTdpPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaTdpResultFromProtoBytes(const void* proto_data, int proto_size, HsBaTdpPipelineResult_t* result);
int HsBaTdpResultToProtoBytes(const HsBaTdpPipelineResult_t* result, void** out_data, int* out_size);

// WAAM
int HsBaWaamConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaWaamPipelineConfig_t* config);
int HsBaWaamConfigToProtoBytes(const HsBaWaamPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaWaamResultFromProtoBytes(const void* proto_data, int proto_size, HsBaWaamPipelineResult_t* result);
int HsBaWaamResultToProtoBytes(const HsBaWaamPipelineResult_t* result, void** out_data, int* out_size);

// File Transfer
int HsBaFileTransferConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineConfig_t* config);
int HsBaFileTransferConfigToProtoBytes(const HsBaFileTransferPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaFileTransferResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineResult_t* result);
int HsBaFileTransferResultToProtoBytes(const HsBaFileTransferPipelineResult_t* result, void** out_data, int* out_size);

// Custom Lua 流水线
int HsBaCustomConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaCustomPipelineConfig_t* config);
int HsBaCustomConfigToProtoBytes(const HsBaCustomPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaCustomResultFromProtoBytes(const void* proto_data, int proto_size, HsBaCustomPipelineResult_t* result);
int HsBaCustomResultToProtoBytes(const HsBaCustomPipelineResult_t* result, void** out_data, int* out_size);

// 内存释放
void HsBaFreeFdmConfigStrings(HsBaFdmPipelineConfig_t* config);
void HsBaFreeSlaConfigStrings(HsBaSlaPipelineConfig_t* config);
void HsBaFreeSlsConfigStrings(HsBaSlsPipelineConfig_t* config);
void HsBaFreeSlmConfigStrings(HsBaSlmPipelineConfig_t* config);
void HsBaFreeLomConfigStrings(HsBaLomPipelineConfig_t* config);
void HsBaFreeTdpConfigStrings(HsBaTdpPipelineConfig_t* config);
void HsBaFreeWaamConfigStrings(HsBaWaamPipelineConfig_t* config);
void HsBaFreeFileTransferConfigStrings(HsBaFileTransferPipelineConfig_t* config);
void HsBaFreeCustomConfigStrings(HsBaCustomPipelineConfig_t* config);
```

> Proto 消息定义位于 `proto/` 目录（`fdm_pipeline.proto`、`sla_pipeline.proto`、`sls_pipeline.proto`、`slm_pipeline.proto`、`lom_pipeline.proto`、`tdp_pipeline.proto`、`waam_pipeline.proto`、`file_transfer_pipeline.proto`、`custom_pipeline.proto`），支持 C++/C#/Java/Python/PHP 多语言输出。
>
> `HsBaCustomResultFromProtoBytes` 得到的结果字符串同样由 `malloc` 分配，请使用 `HsBaFreeCustomPipelineResult()` 释放（Custom 没有单独的 ResultStrings 释放函数）。
>
> **C++ 调用方注意**：`DllHsBaSlicer` 内部已经链了一份 `HsBaSlicerProto`，不要把生成的 `.pb.cc` 再链进同一个进程，否则 protobuf 会因同名 proto 文件重复注册（`File already exists in database`）在启动时终止进程。纯 C++ 集成请直接使用 C 结构体，或改走 `LibHsBaSlicer` / `ModuleHsBaSlicer` 层；跨语言调用方（C# / Python / Java 用自己的 protobuf 运行库）不受影响，`samples/Custom/` 的示例 4 就是按这种方式只手拼 wire 字节。

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

### C++ 事件源注册

注册 C 风格事件回调，用于监听 Zipper 压缩进度和数据库操作事件（非 Lua 环境，纯 C 回调）：

```c
// Zipper 事件回调（进度百分比 + 阶段描述）
void HsBaAddZipperEventCallback(const char* event_name, void (*func)(double, const char*));

// 数据库事件回调（键 + 值）
void HsBaAddDBEventCallback(const char* event_name, void (*func)(const char*, const char*));
```

示例：

```c
#include "initialize.h"
#include "event_source_register.h"

static void on_zip_progress(double percent, const char* stage) {
    // 处理压缩进度
}

static void on_db_event(const char* key, const char* value) {
    // 处理数据库事件
}

int main(void) {
    initialize();
    HsBaAddZipperEventCallback("zipper.on_progress", on_zip_progress);
    HsBaAddDBEventCallback("db.on_query", on_db_event);
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
2. 结果结构体中的 `gcode_content` / `export_path`（WAAM 为 `output_path`）/ `error_message` 由库内部分配，**必须**调用对应的 `HsBaFree*PipelineResult()` 释放；
3. 版本字符串必须用 `HsBaFreeVersionString()` 释放；
4. 模型句柄（`HsBaLoadModel` / `HsBaGetModel` / `HsBaBoolean*` / `HsBaThickSolidModel` 返回的 `void*`）必须用 `HsBaReleaseModelHandle()` 释放引用；
5. `pipeline_types.h` 还提供无 DLL 依赖的内联初始化器 `HsBaFdmConfigDefault()` / `HsBaSlaConfigDefault()` / `HsBaSlsConfigDefault()` / `HsBaSlmConfigDefault()` / `HsBaLomConfigDefault()` / `HsBaTdpConfigDefault()` / `HsBaWaamConfigDefault()` / `HsBaFileTransferConfigDefault()`，便于纯头文件场景（如 P/Invoke 结构体对照）使用；
6. Proto 反序列化（`*FromProtoBytes`）产生的字符串字段由 `malloc` 分配，必须调用对应的 `HsBaFree*ConfigStrings()` 释放（Custom 结果例外，用 `HsBaFreeCustomPipelineResult()`）；`*ToProtoBytes` 产生的 `out_data` 缓冲区由调用方 `free`。

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
- `samples/SLM/` —— SLM 金属粉末床流水线与 Lua 导出示例（基础/自定义金属参数/异步）
- `samples/LOM/` —— LOM 叠层实体流水线与 Lua 导出示例
- `samples/TDP/` —— 3DP 粘结剂喷射流水线与 Lua 导出示例
- `samples/WAAM/` —— WAAM 电弧增材机器人路径导出示例
- `samples/Custom/` —— 整条流水线完全由 Lua 脚本定义的示例（FDM/SLA/内联脚本/异步/Protobuf 字节流）
- `android/` —— Android JNI 调用示例工程
- `ios/HsBaSlicerExample/` —— iOS Swift 桥接调用示例
