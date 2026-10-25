---
kind: logging_system
name: 跨平台单例日志系统（Boost.Log / Android logcat / iOS os_log）
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - logger/CMakeLists.txt
    - logger/export.h
    - docs/zh/logger/README.md
    - docs/zh/logger/logger_singletone.md
    - docs/zh/logger/log_state.md
---

## 1. 使用的框架与总体方案

项目自研了一个线程安全的跨平台单例日志器 `HsBa::Slicer::Log::LoggerSingletone`，根据编译目标平台选择底层实现：

- **桌面 / Linux**：基于 Boost.Log (`boost/log/trivial.hpp`)，同时输出到控制台和可选的文件 sink。
- **Android**：通过 `__android_log_print` 写入 kernel log (logcat)。
- **iOS**：通过 `os_log` / `os_log_with_type` 写入系统日志。

日志级别采用统一的整数枚举（0~5），在 logger.cpp 中分别映射为各平台的原生级别：

| 数值 | 含义 | Boost.Log | Android | iOS |
|------|------|-----------|---------|-----|
| 0 | trace/verbose | `trace` | `ANDROID_LOG_VERBOSE` | `OS_LOG_TYPE_DEBUG` |
| 1 | debug | `debug` | `ANDROID_LOG_DEBUG` | `OS_LOG_TYPE_DEBUG` |
| 2 | info | `info` | `ANDROID_LOG_INFO` | `OS_LOG_TYPE_INFO` |
| 3 | warning | `warning` | `ANDROID_LOG_WARN` | `OS_LOG_TYPE_DEFAULT` |
| 4 | error | `error` | `ANDROID_LOG_ERROR` | `OS_LOG_TYPE_ERROR` |
| 5 | fatal | `fatal` | `ANDROID_LOG_FATAL` | `OS_LOG_TYPE_FAULT` |

默认阈值：`_DEBUG` 下为 1（debug），Release 下为 3（warning）。

## 2. 关键文件

- `logger/logger.hpp` — 公共 API：`LoggerSingletone`、`LogState`、用户自定义字面量 `_log_debug/_log_info/_log_warning/_log_error`。
- `logger/logger.cpp` — 平台分支实现、Boost.Log 初始化、配置文件加载、sink 注册。
- `utils/logcfg.ini` — 默认日志配置样例。
- `docs/zh/logger/README.md`、`docs/zh/logger/logger_singletone.md`、`docs/zh/logger/log_state.md` — 文档。
- `logger/CMakeLists.txt`、`logger/export.h` — 导出宏 `HSBA_SLICER_LOG_API`。

调用方示例（`HsBaSlicer/HsBaSlicer.cpp`、`samples/*/main.cpp`）使用两种风格：

```cpp
LoggerSingletone::LogInfo("initialize completed");
"use log file"_log_info();          // 流式字面量语法
```

## 3. 架构与约定

### 3.1 单例与线程安全

- 单例通过 `std::shared_ptr<LoggerSingletone>` + `static std::once_flag instance_flag_` + `std::call_once` 实现延迟初始化；所有公开静态方法在实例未就绪时都会触发一次构造。
- 读操作（如 `UseLogFile()`）通过 `std::shared_mutex` + `std::shared_lock` 保护共享状态。

### 3.2 配置加载

构造函数从当前工作目录读取 `logcfg.ini`（路径拼接为 `current_path()/logcfg.ini`），使用 `fileoperator/rw_ptree.hpp` 的 `Config::from_ini` 解析。支持的键：

- `[log]` 段：`log_level`（Release）、`log_level_debug`（Debug）、`use_log_file`、`log_file`（相对路径拼接）。
- `[log_format]` 段：`log_datatime_format`（传给 Boost.Log 的 `%Y-%m-%d %H:%M:%S` 等 strftime 格式）。

若文件不存在或字段缺失，回退到默认值：`log_path = <cwd>/log/log.txt`，`use_log_file = false`，时间格式 `%Y-%m-%d %H:%M:%S`。

### 3.3 输出格式与 Sink

桌面平台下，Boost.Log 的 formatter 固定为：

```
[TimeStamp] <severity> [file:line] function_name: message\n
```

其中 `TimeStamp` 是全局属性 `local_clock()`，由 `add_common_attributes()` 注入；severity 来自 `boost::log::trivial::severity`。

Sink 包括：

- 控制台 sink（始终启用）。
- 文件 sink（仅当 `use_log_file=true` 时启用），按大小 50MB 轮转 + 每日午夜（`rotation_at_time_point(0,0,0)`）追加写入。

### 3.4 结构化字段

日志记录包含以下结构化上下文（非 JSON，而是格式化字符串中的字段）：

- `TimeStamp`：由 Boost.Log 全局属性注入的时间戳。
- `severity`：日志级别标签。
- 源码位置：由 `GetSourceLocation(std::source_location::current())` 生成，形如 `[path/to/file.cpp:123] func_name: `。

没有额外的结构化字段（如 job_id、pipeline 名等）——所有业务上下文需嵌入消息字符串本身。

## 4. 约定与约束

- **级别语义**：整数 0~5 是唯一内部表示；新增级别需要同时在 `GetLogLevel`、`GetAndroidLogPriority`、`GetiOSLogType` 三个映射函数中补齐，否则落入 default 分支（Debug 下降级为 debug/VERBOSE/DEBUG，Release 下降级为 warning/INFO/DEFAULT）。
- **平台分支**：`#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)` 包裹 Boost.Log 代码；Android/iOS 各自有独立分支，不可混用。
- **配置优先级**：`logcfg.ini` 存在且字段可解析时覆盖默认值；`ptree_bad_path` 异常被捕获后静默回退到默认值，不抛给调用方。
- **API 风格**：对外暴露两类接口 —— 静态方法 `LogDebug/LogInfo/LogWarning/LogError` 以及 `LogLiterals` 内联命名空间下的 `_log_*` 用户自定义字面量，二者最终都委托给 `LoggerSingletone::Log(message, level, source_location)`。
- **导出边界**：所有对外符号通过 `HSBA_SLICER_LOG_API`（定义于 `logger/export.h`）标记，便于 DLL 导出。
- **源位置**：所有日志调用均携带 `std::source_location::current()`，因此要求 C++20 编译器支持；这是强制性的，因为 `GetSourceLocation` 直接消费该类型。
- **无结构化日志字段**：当前实现没有提供 key-value 结构化字段扩展点；如需添加业务字段，需在 `GetSourceLocation` 之后手动拼接到消息字符串中。