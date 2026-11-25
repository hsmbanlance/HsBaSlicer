---
kind: logging_system
name: HsBaSlicer 日志系统：基于 Boost.Log 的多平台单例日志器
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - docs/zh/logger/README.md
---

## 1. 使用的系统与框架

- **核心框架**：桌面/Android/iOS 三平台分别通过 `boost::log`、`__android_log_print`、`os_log` 输出。
- **配置格式**：INI，由 `fileoperator/rw_ptree.hpp` 的 `Config::from_ini` 解析。
- **线程安全**：`LoggerSingletone` 使用 `std::shared_mutex` + `std::once_flag` 实现线程安全的懒加载单例（`GetInstance()` / `CreateInstance()`）。
- **C++20 特性**：调用点通过 `std::source_location::current()` 自动捕获文件名、行号与函数名。

## 2. 关键文件

| 文件 | 职责 |
|---|---|
| `logger/logger.hpp` | 暴露 `HsBa::Slicer::Log::LoggerSingletone` 与 `LogLiterals` 命名空间下的 `_log_debug/_log_info/_log_warning/_log_error` 字面量 |
| `logger/logger.cpp` | 平台分支实现、Boost.Log 初始化、INI 配置读取、sink 注册 |
| `utils/logcfg.ini` | 默认日志配置文件（随仓库提供） |
| `docs/zh/logger/README.md` | 模块文档入口 |

## 3. 架构与约定

### 3.1 日志级别映射
内部使用 0–5 的整数表示级别，在三个平台上分别映射为：

| 内部值 | Boost.Log | iOS os_log | Android logcat |
|---:|---|---|---|
| 0 | trace | OS_LOG_TYPE_DEBUG | ANDROID_LOG_VERBOSE |
| 1 | debug | OS_LOG_TYPE_DEBUG | ANDROID_LOG_DEBUG |
| 2 | info | OS_LOG_TYPE_INFO | ANDROID_LOG_INFO |
| 3 | warning | OS_LOG_TYPE_DEFAULT | ANDROID_LOG_WARN |
| 4 | error | OS_LOG_TYPE_ERROR | ANDROID_LOG_ERROR |
| 5 | fatal | OS_LOG_TYPE_FAULT | ANDROID_LOG_FATAL |

未匹配值走 default 分支：`_DEBUG` 下退到 debug/verbose，Release 下退到 warning/INFO。

### 3.2 输出格式
桌面端 Boost.Log 格式串为：
```
[TimeStamp] <severity> [file:line] function_name: message\n
```
其中 `TimeStamp` 来自全局属性 `local_clock()`，格式字符串取自 INI 的 `log_format.log_datatime_format`（默认 `%Y-%m-%d %H:%M:%S`）。

### 3.3 Sink 策略
- **控制台 sink**：始终添加（桌面端）。
- **文件 sink**：仅在 `use_log_file = true` 时启用；按大小 50MB 轮转 + 每日零点时间轮转；追加模式打开。
- **移动端**：Android 走 kernel log，iOS 走 `os_log`，均不写本地文件（`UseLogFile()` 在这两个平台恒返回 `true` 仅用于对外语义）。

### 3.4 配置加载顺序
构造 `LoggerSingletone` 时：
1. 当前工作目录拼接 `/logcfg.ini`。
2. 若存在则读取：
   - `_DEBUG` 构建取 `log.log_level_debug`，Release 取 `log.log_level`。
   - `log.use_log_file`、`log.log_file`（相对路径拼接当前目录）、`log_format.log_datatime_format`。
3. 任一字段缺失抛 `ptree_bad_path` 时回退默认值：`log_path_ = current_path + "/log/log.txt"`，`use_log_file_ = false`，`log_datatime_format_ = "%Y-%m-%d %H:%M:%S"`。

### 3.5 调用方式
代码库中观察到两种用法：
- 直接静态方法：`LoggerSingletone::LogInfo(msg)`（如 `HsBaSlicer/HsBaSlicer.cpp`、各 `samples/*/main.cpp`）。
- 用户定义字面量：`"message"_log_info()`（如 `HsBaSlicer/HsBaSlicer.cpp`），底层经 `LogState` 委托给 `LoggerSingletone::Log`。

所有调用点都通过 `std::source_location` 自动注入来源位置信息，无需手动传参。

## 4. 约定与约束

- **单例访问**：所有公开 API（`Log*`、字面量）在 `instance_` 为空时会通过 `std::call_once(instance_flag_, ...)` 触发延迟创建，因此调用方不需要显式初始化 Logger。
- **级别阈值**：桌面端通过 `boost::log::core::get()->set_filter(severity >= log_level_)` 过滤，低于阈值的消息不会进入 sink。
- **平台分支**：`#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)` 包裹 Boost.Log 相关头与初始化逻辑；Android 与 iOS 各自有独立的 `GetAndroidLogPriority` / `GetiOSLogType` 映射函数。
- **默认行为**：Release 构建默认 `log_level_ = 3`（warning），Debug 构建默认 `log_level_ = 1`（debug）；文件输出默认关闭。
- **导出宏**：类与方法统一加 `HSBA_SLICER_LOG_API` 导出标记（见 `logger/export.h`）。
- **日志文件路径**：当 `use_log_file = true` 时，`log_file` 是相对于当前工作目录的路径片段（构造时用 `current_path.string() + ptree.get<std::string>("log.log_file")` 拼接），示例配置写的是 `/log/log.txt`。
- **源码位置追踪**：`GetSourceLocation` 将 `file_name`、`line`、`function_name` 拼成 `[file:line] func: ` 前缀，附加在每条消息之前。