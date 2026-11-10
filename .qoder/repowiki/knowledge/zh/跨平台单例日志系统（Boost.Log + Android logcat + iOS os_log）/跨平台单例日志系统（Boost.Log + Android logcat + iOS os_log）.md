---
kind: logging_system
name: 跨平台单例日志系统（Boost.Log + Android logcat + iOS os_log）
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - docs/zh/logger/README.md
    - docs/zh/logger/logger_singletone.md
    - docs/zh/logger/log_state.md
---

## 1. 使用的系统与框架

HsBaSlicer 在 `logger/` 目录下实现了一个自研的跨平台、线程安全的单例日志器 `HsBa::Slicer::Log::LoggerSingletone`，根据目标平台选择底层输出：

- **桌面 / Linux**：基于 `boost/log`（`trivial` API），同时输出到控制台与可旋转的文件。
- **Android**：通过 `<android/log.h>` 调用 `__android_log_print`，写入内核日志（logcat）。
- **iOS**：通过 `<os/log.h>` 的 `os_log_*` 系列函数写入系统日志。

日志级别采用统一的整型枚举（0–5）：trace/debug/info/warning/error/fatal，由 `GetLogLevel` / `GetAndroidLogPriority` / `GetiOSLogType` 三个平台适配函数映射到各自后端的级别常量。

## 2. 关键文件与包

- `logger/logger.hpp`：对外 API 声明，包含 `LoggerSingletone` 类与 `LogState` + `_log_debug/_log_info/_log_warning/_log_error` 用户自定义字面量。
- `logger/logger.cpp`：平台分支实现、Boost.Log 初始化、配置文件加载、日志轮转策略。
- `utils/logcfg.ini`：默认日志配置模板（`[log]` 与 `[log_format]` 两个 section）。
- `docs/zh/logger/README.md`、`logger_singletone.md`、`log_state.md`：中文文档说明。
- `logger/export.h`：导出宏 `HSBA_SLICER_LOG_API`，用于 DLL 导出。

## 3. 架构与设计约定

### 3.1 单例与线程安全

`LoggerSingletone` 使用 `std::shared_ptr` + `std::once_flag` + `std::call_once` 实现懒加载的单例；构造与访问路径上持有 `std::shared_mutex` 保护。所有公开静态方法（`Log`/`LogDebug`/`LogInfo`/`LogWarning`/`LogError`）在实例未创建时都会触发一次 `CreateInstance()`。

### 3.2 日志级别

| 整数 | 含义 | Boost.Log | Android | iOS |
|------|------|-----------|---------|-----|
| 0 | trace | `trace` | `VERBOSE` | `OS_LOG_TYPE_DEBUG` |
| 1 | debug | `debug` | `DEBUG` | `OS_LOG_TYPE_DEBUG` |
| 2 | info | `info` | `INFO` | `OS_LOG_TYPE_INFO` |
| 3 | warning | `warning` | `WARN` | `OS_LOG_TYPE_DEFAULT` |
| 4 | error | `error` | `ERROR` | `OS_LOG_TYPE_ERROR` |
| 5 | fatal | `fatal` | `FATAL` | `OS_LOG_TYPE_FAULT` |

### 3.3 配置加载

构造函数从当前工作目录读取 `logcfg.ini`，通过 `Config::from_ini`（`fileoperator/rw_ptree.hpp`）解析：

- `log.log_level`（release 模式）/ `log.log_level_debug`（debug 模式）：阈值，默认 release=3、debug=1。
- `log.use_log_file`：是否启用文件输出。
- `log.log_file`：文件路径（相对路径拼接至当前目录）。
- `log_format.log_datatime_format`：Boost.Log 时间戳格式，默认 `%Y-%m-%d %H:%M:%S`。

若配置文件不存在或字段缺失，回退为：禁用文件输出、日志路径 `/log/log.txt`、默认时间格式。

### 3.4 输出格式与轮转

桌面端 Boost.Log 格式化器输出形如：

```
[TimeStamp] <severity> [file:line] function_name: message
```

文件 sink 配置了：
- `rotation_size = 50MB`（`SIZE_50MB`）
- `time_based_rotation` 每天 00:00:00 轮转
- `open_mode = std::ios::app`（追加模式）

### 3.5 结构化字段

每条日志自动附加 `source_location`（文件名、行号、函数名），由 `GetSourceLocation` 统一前缀化；时间戳作为全局属性 `TimeStamp` 注入 Boost.Log 上下文。

### 3.6 两种调用风格

1. **传统静态方法**：`LoggerSingletone::LogInfo(msg)` / `LogDebug` / `LogWarning` / `LogError`。
2. **流式字面量**：`"message"_log_info()` / `_log_debug()` / `_log_warning()` / `_log_error()`，内部通过 `LogState` RAII 对象延迟执行，便于条件编译下零开销。

## 4. 约定与约束

- **平台分支**：非 Android/iOS 平台走 Boost.Log；Android 走 `__android_log_print`；iOS 走 `os_log_with_type`。这是由 `#ifdef __ANDROID__` / `#if defined(TARGET_OS_IOS)` 硬编码决定的行为，不是可选插件机制。
- **日志级别必须使用 0–5 的整数**：`Log()` 的 switch 只处理 0–5，其他值落入 default 分支（Android/iOS 降级为 INFO，桌面端降级为 info）。
- **调试/发布默认级别不同**：`_DEBUG` 下默认 `log_level_=1`（debug），否则 `log_level_=3`（warning）。该逻辑在构造函数的 `#if _DEBUG` 分支中强制设定，随后才被 `logcfg.ini` 覆盖。
- **配置文件路径固定**：始终为 `<current_path>/logcfg.ini`，无命令行参数或环境变量覆盖入口。
- **文件日志路径固定拼接**：`current_path + ptree.get<std::string>("log.log_file")`，不允许绝对路径以外的任意根。
- **Android/iOS 上 `UseLogFile()` 恒返回 true**：因为这两个平台不写本地文件，而是走系统日志，API 语义被改写为“是否有日志输出”。
- **所有公开 API 均带 `HSBA_SLICER_LOG_API` 导出宏**，供 DllHsBaSlicer、samples 等外部模块调用。
- **示例代码中的使用方式**：`HsBaSlicer/HsBaSlicer.cpp`、`samples/*/main.cpp` 中统一通过 `LoggerSingletone::LogInfo` 和 `_log_info()` 字面量记录进度与状态，未见直接调用 `BOOST_LOG_TRIVIAL` 绕过封装的代码。

## 5. 依赖方向

`logger` 模块仅依赖 `base/singleton.hpp`（单例基元）与 `fileoperator/rw_ptree.hpp`（INI 解析），不反向依赖业务模块，是仓库中最底层的横切基础设施之一。