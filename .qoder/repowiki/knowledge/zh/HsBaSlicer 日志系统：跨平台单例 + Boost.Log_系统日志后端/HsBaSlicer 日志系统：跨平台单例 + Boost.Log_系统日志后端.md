---
kind: logging_system
name: HsBaSlicer 日志系统：跨平台单例 + Boost.Log/系统日志后端
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - logger/export.h
---

## 1. 使用的系统与框架

- **核心实现**：`logger/` 目录下的 `LoggerSingletone`（单例）+ `LogState`（流式 DSL）。
- **桌面端后端**：Boost.Log（`boost/log/trivial.hpp`、`text_file_backend`、`console` sink），通过 `add_console_log()` / `add_file_log()` 同时输出到控制台与文件，支持按大小（50 MB）和每日（`rotation_at_time_point(0,0,0)`）轮转。
- **Android 后端**：`__android_log_print`（`android/log.h`），优先级映射为 `VERBOSE/DEBUG/INFO/WARN/ERROR/FATAL`。
- **iOS 后端**：`os_log`（`os_log.h`），类型映射为 `OS_LOG_TYPE_DEBUG/INFO/DEFAULT/ERROR/FAULT`。
- **配置来源**：进程当前工作目录下的 `logcfg.ini`，由 `fileoperator/rw_ptree.hpp` 的 `Config::from_ini` 解析。

## 2. 关键文件

| 文件 | 作用 |
|---|---|
| `logger/logger.hpp` | 公共 API：`LoggerSingletone`、`LogState`、`_log_debug/_log_info/_log_warning/_log_error` 字面量 |
| `logger/logger.cpp` | 跨平台初始化、级别映射、sink 注册、实际输出 |
| `logger/export.h` | `HSBA_SLICER_LOG_API` 导出宏 |
| `utils/logcfg.ini` | 默认日志配置文件（level、是否写文件、路径、时间格式） |
| `base/singleton.hpp` | 被 logger 头文件 include（但 logger 未使用其模板，而是自行实现 `GetInstance` + `std::call_once`） |

## 3. 架构与设计约定

### 3.1 单例与线程安全
- `LoggerSingletone` 通过 `static std::shared_ptr<instance_>` + `std::once_flag instance_flag_` + `std::shared_mutex mutex_` 实现线程安全的懒加载单例；`GetInstance()` 用 `std::call_once` 保证只创建一次。
- `UseLogFile()` 在 Android/iOS 上直接返回 `true`（强制走系统日志），桌面端才读 `use_log_file_`。

### 3.2 日志级别映射
统一的整数级别（0–5）在各平台间映射：

| 数值 | 含义 | Boost.Log | Android | iOS |
|---|---|---|---|---|
| 0 | trace/verbose | `trace` | `ANDROID_LOG_VERBOSE` | `OS_LOG_TYPE_DEBUG` |
| 1 | debug | `debug` | `ANDROID_LOG_DEBUG` | `OS_LOG_TYPE_DEBUG` |
| 2 | info | `info` | `ANDROID_LOG_INFO` | `OS_LOG_TYPE_INFO` |
| 3 | warning | `warning` | `ANDROID_LOG_WARN` | `OS_LOG_TYPE_DEFAULT` |
| 4 | error | `error` | `ANDROID_LOG_ERROR` | `OS_LOG_TYPE_ERROR` |
| 5 | fatal | `fatal` | `ANDROID_LOG_FATAL` | `OS_LOG_TYPE_FAULT` |

未知级别时，`_DEBUG` 构建回退到 debug，Release 回退到 warning（或 DEFAULT）。

### 3.3 输出格式
桌面端 Boost.Log 格式化串为：
```
[TimeStamp] <severity> [file:line] function_name: message\n
```
其中 `TimeStamp` 是 `local_clock()` 全局属性，格式来自 `log_format.log_datatime_format`（默认 `%Y-%m-%d %H:%M:%S`）。

### 3.4 配置加载顺序
1. 构造时读取 `<current_path>/logcfg.ini`。
2. 若存在且字段合法：覆盖 `log_level` / `log_level_debug` / `use_log_file` / `log_file` / `log_datatime_format`。
3. 若缺失或抛 `ptree_bad_path`：回退到默认值——`log_path = current_path + "/log/log.txt"`，`use_log_file = false`，时间格式 `%Y-%m-%d %H:%M:%S`。
4. `_DEBUG` 构建默认 `log_level_ = 1`（debug），Release 默认 `log_level_ = 3`（warning）。

### 3.5 调用接口
- 静态函数：`LoggerSingletone::LogDebug/Info/Warning/Error(message, location)`，自动捕获 `std::source_location::current()`。
- 流式字面量：`"msg"_log_debug()` / `_log_info()` / `_log_warning()` / `_log_error()`，内部经 `LogState` 延迟执行，同样携带 source location。
- 所有输出前都会拼接 `[file:line] function_name: ` 前缀（`GetSourceLocation`）。

## 4. 约定与约束

- **级别语义固定**：日志级别必须为 0–5 的整数，新增级别需同步修改 `GetLogLevel` / `GetAndroidLogPriority` / `GetiOSLogType` 三个 switch 分支（`logger.cpp` 第 54–133 行）。
- **配置键名固定**：INI 中必须提供 `log.log_level`、`log.log_level_debug`、`log.use_log_file`、`log.log_file`、`log_format.log_datatime_format`，否则回退到硬编码默认值（`logger.cpp` 第 157–183 行）。
- **平台分支不可省略**：新增日志级别时必须同时补齐 Android 与 iOS 的映射分支，否则该级别会落入 default 分支。
- **桌面端双 sink**：Console sink 始终启用；File sink 仅在 `use_log_file == true` 时注册，且固定 50 MB 轮转 + 每日午夜轮转（`SIZE_50MB` 与 `rotation_at_time_point(0,0,0)`）。
- **Android/iOS 不写文件**：`UseLogFile()` 在这两个平台上恒返回 `true`，表示“使用系统日志”而非磁盘文件。
- **source_location 必填**：所有公开 `Log*` 方法都带 `std::source_location` 参数并以 `current()` 作为默认值，调用方不应手动传入位置。
- **导出范围**：所有对外符号均加 `HSBA_SLICER_LOG_API` 宏（定义于 `logger/export.h`），用于共享库导出控制。