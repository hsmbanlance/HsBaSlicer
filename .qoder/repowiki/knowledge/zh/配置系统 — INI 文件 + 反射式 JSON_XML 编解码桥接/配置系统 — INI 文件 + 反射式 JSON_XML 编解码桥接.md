---
kind: configuration_system
name: 配置系统 — INI 文件 + 反射式 JSON/XML 编解码桥接
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/restapi.ini
    - restapi/main.cpp
    - restapi/server_config.cpp
    - restapi/config_bridge.cpp
    - utils/logcfg.ini
    - logger/logger.cpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - fileoperator/param_reflect.hpp
    - fileoperator/param_convert.hpp
---

## 1. 使用的系统与工具

- **INI 配置文件**：通过 `fileoperator/rw_ptree.hpp` 封装的 `Config::from_ini()`（基于 boost.property_tree）统一解析 `.ini` 文件。
- **日志配置**：`logger/logger.cpp` 在启动时从当前工作目录加载 `logcfg.ini`，按 `_DEBUG`/非 `_DEBUG` 分支选择 `log.log_level_debug` 或 `log.log_level`。
- **REST API 服务配置**：`restapi/restapi.ini` 由 `restapi/server_config.cpp::LoadServerConfig` 解析为 `ServerConfig` 结构体，支持 `[server]`、`[paramstore]`、`[auth]` 三个 section。
- **运行时参数配置（Pipeline Config）**：切片流水线参数以 protobuf 定义（`proto/slice_config.proto`、`proto/*_pipeline.proto`），经 `convert/Msg2PipelineConfig.cpp` 与 `convert/PipelineConfig2Msg.cpp` 在 C ABI 结构与消息之间转换；序列化/反序列化走 `fileoperator/param_reflect.*` + `fileoperator/param_convert.*` 的反射机制。
- **JSON/XML 双格式编解码**：`restapi/config_bridge.cpp` 基于 `Utils::TypeInfo` 反射表，用 rapidjson 和 tinyxml2 对任意 C ABI 配置结构做双向编解码，供 REST 端点使用。
- **应用级单例配置**：`utils/app_config.cpp` 提供线程安全的 `AppConfigSingletone`（目前仅暴露 `GetSevenZPath()`）。

## 2. 关键文件

- `restapi/restapi.ini` — 服务器默认配置（host/port/default_format/max_concurrent_jobs/max_jobs/result_ttl_seconds / paramstore.backend/sqlite_path/host/user/password/database/port/crypto_password / auth.enabled/token）。
- `restapi/main.cpp` — CLI 入口，默认 `--config restapi.ini`，支持 `--host`/`--port` 覆盖。
- `restapi/server_config.cpp` — `LoadServerConfig(path)` 实现，缺失文件返回内置默认值。
- `restapi/config_bridge.cpp` — 反射驱动的 `SerializeConfig`/`DeserializeConfig`（JSON/XML）。
- `utils/logcfg.ini` — 日志级别、是否写文件、路径与时间格式。
- `logger/logger.cpp` — 启动时读取 `logcfg.ini`，按平台（Windows/Android/iOS）初始化 Boost.Log 或系统日志。
- `utils/app_config.hpp/.cpp` — 进程内共享配置单例。
- `fileoperator/param_reflect.hpp/.cpp`、`fileoperator/param_convert.hpp/.cpp` — Pipeline 参数的反射描述与类型收敛（double/int64_t/string/nullptr）。
- `convert/Msg2PipelineConfig.cpp`、`convert/PipelineConfig2Msg.cpp` — protobuf ↔ C ABI 配置结构的双向转换。

## 3. 架构与约定

### 3.1 配置加载优先级

1. **程序内置默认值**：所有配置结构都有合理的默认构造（如 `port=8080`、`default_format=json`、`max_concurrent_jobs=2`、`max_jobs=128`、`result_ttl_seconds=3600`、`backend=sqlite`、`sqlite_path=hsba_params.db`）。
2. **INI 文件**：可选，缺失不报错。REST 服务默认在当前工作目录查找 `restapi.ini`；日志模块默认查找 `logcfg.ini`。
3. **命令行覆盖**：REST 服务的 `--host`/`--port` 在 INI 之后覆盖对应字段。

### 3.2 配置校验与规范化

`LoadServerConfig` 在解析后集中做防御性归一化：
- `port` 必须 ∈ (0, 65535]，否则回退到 8080。
- `default_format` 只接受 `xml`，其他一律归为 `json`。
- `max_concurrent_jobs`、`max_jobs` 至少为 1；`result_ttl_seconds` 至少为 0。
- `paramstore.backend` 为空串或 `none` 时禁用 ParamStore 后端；`mysql`/`postgresql`/`postgres` 映射到 `HSBA_PARAM_BACKEND_*` 枚举，其余默认 `HSBA_PARAM_BACKEND_SQLITE`。

### 3.3 反射式配置编解码

`config_bridge.cpp` 通过 `Utils::TypeInfo::fields`（名称→(child TypeInfo*, 字节偏移)）遍历任意 C ABI 配置结构：
- 支持的字段类型被收敛为四类：`CStr` (`const char*`)、`Double` (`float`/`double`)、`Int64` (`int`/`int64_t`/enum)、`Unsupported`（忽略）。
- JSON 输出使用 pretty-print；XML 根节点固定为 `<Config>`，null 字段在 XML 中省略（反序列化时保留基值）。
- 未知字段在反序列化中被静默忽略，保证前向兼容。

### 3.4 日志配置

`LoggerSingletone` 构造函数中：
- 若 `_DEBUG` 编译则优先读 `log.log_level_debug`，否则读 `log.log_level`。
- `use_log_file=false` 时仅输出控制台；true 时追加写入 `log.log_file`（相对 current_path 拼接）。
- 非 Windows 平台使用 Boost.Log，带时间戳、严重级别、source location 格式化。
- Android 走 `__android_log_print`，iOS 走 `os_log`。

### 3.5 进程内配置单例

`AppConfigSingletone` 使用 `std::shared_mutex` 保护 `sevenZ_path_`，提供 `GetInstance()`/`DeleteInstance()` 生命周期管理，用于跨模块共享外部工具路径等设置。

## 4. 约定与约束

- **INI 文件位置**：REST 服务默认在当前工作目录查找 `restapi.ini`；日志模块默认查找 `logcfg.ini`。可通过 `--config <file>` 覆盖 REST 配置路径（见 `restapi/main.cpp` 用法说明）。
- **缺失 INI 文件不是错误**：`LoadServerConfig` 显式检查 `std::filesystem::exists`，不存在时直接返回内置默认值，使“刚构建的服务无需额外文件即可启动”。
- **ParamStore 后端开关**：`paramstore.backend` 为空或 `none` 时 `paramstore_enabled=false`，对应 `/api/params` 端点返回 503（见 `restapi.ini` 注释）。
- **认证段预留**：`[auth]` section 当前未被读取，注释明确标注 “Reserved for a future release. Not read by the current implementation.”。
- **CLI 选项白名单**：main 中仅识别 `--help/-h`、`--config/-c`、`--host`、`--port`，未知选项打印帮助并返回退出码 2。
- **配置字段类型限制**：config_bridge 仅允许 `const char*`、`float/double`、`int/int64_t/enum` 三类字段进入 JSON/XML；其他类型被归类为 `Unsupported` 并在反序列化中跳过。
- **环境变量**：代码库中未发现 `.env`、`getenv` 或环境变量注入逻辑；运行期配置来源仅限于 INI 文件和命令行参数。
- **Feature flag**：REST API 服务整体受 `HSBA_BUILD_REST_API` 门控（顶层 CMake 聚合），但此门控属于构建系统范畴，不在本卡讨论范围。
