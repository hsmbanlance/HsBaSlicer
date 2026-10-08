---
kind: logging_system
name: 跨平台单例日志子系统（Boost.Log / os_log / __android_log_print）
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
---

## 1. 使用的系统与框架

HsBaSlicer 在 `logger/` 目录下实现了自研的跨平台线程安全日志子系统，对外暴露命名空间 `HsBa::Slicer::Log`。核心类为 `LoggerSingletone`（头文件拼写为 `LoggerSingletone`），通过 `std::shared_mutex` + `std::once_flag` 实现懒加载的单例。

后端按编译目标选择：
- 桌面端（非 Android、非 iOS）：使用 **Boost.Log**（`boost/log/trivial.hpp`、`text_file_backend`、`console` sink），同时输出到控制台与可选的文件 sink；文件默认位于当前工作目录下的 `/log/log.txt`，支持 50MB 轮转与每日滚动。
- iOS：使用系统 API `os_log` / `os_log_with_type`，通过 `os_log_create("HsBaSlicer", "Log")` 创建 logger handle。
- Android：使用内核日志 `__android_log_print`，tag 固定为 `"HsBaSlicer"`。

日志级别采用整数枚举（0–5），映射关系如下：
| 内部 level | Boost.Log | iOS os_log | Android |
|---|---|---|---|
| 0 | trace | OS_LOG_TYPE_DEBUG | ANDROID_LOG_VERBOSE |
| 1 | debug | OS_LOG_TYPE_DEBUG | ANDROID_LOG_DEBUG |
| 2 | info | OS_LOG_TYPE_INFO | ANDROID_LOG_INFO |
| 3 | warning | OS_LOG_TYPE_DEFAULT | ANDROID_LOG_WARN |
| 4 | error | OS_LOG_TYPE_ERROR | ANDROID_LOG_ERROR |
| 5 | fatal | OS_LOG_TYPE_FAULT | ANDROID_LOG_FATAL |

## 2. 关键文件

- `logger/logger.hpp`：公共接口，定义 `LoggerSingletone`、便捷方法 `LogDebug/LogInfo/LogWarning/LogError`、以及 `inline namespace LogLiterals` 中的用户自定义字面量 `_log_debug/_log_info/_log_warning/_log_error` 和 `LogState` 流式对象。
- `logger/logger.cpp`：实现主体，包含平台分支初始化、Boost.Log 配置、配置文件解析、以及各后端的实际写入逻辑。
- `utils/logcfg.ini`：默认日志配置文件，由 `fileoperator/rw_ptree.hpp` 提供的 `Config::from_ini` 读取。
- `docs/en/logger/`、`docs/zh/logger/`：Doxygen 文档中对应的 logger 模块页面。

## 3. 架构与约定

### 3.1 单例与线程安全
- 单例通过静态成员 `instance_` + `instance_flag_` + `CreateInstance()` 构造，`GetInstance()` 使用 `std::call_once` 保证只初始化一次。
- `UseLogFile()` 等读路径使用 `std::shared_lock(mutex_)` 加共享锁；写路径（如构造函数内）不持锁，因为构造发生在首次访问之前。

### 3.2 源码位置信息
- 所有 `Log*` 重载接受 `const std::source_location& location = std::source_location::current()`，内部通过 `GetSourceLocation()` 统一格式化为 `[file:line] function_name: message` 前缀。

### 3.3 配置加载顺序
1. 取 `std::filesystem::current_path() + "/logcfg.ini"`。
2. 若存在则解析 INI：
   - `_DEBUG` 下读取 `log.log_level_debug`，否则读取 `log.log_level`。
   - 读取 `log.use_log_file`、`log.log_file`、`log_format.log_datatime_format`。
3. 解析失败或文件不存在时回退默认值：`log_path_ = current_path + "/log/log.txt"`，`use_log_file_ = false`，时间格式 `"%Y-%m-%d %H:%M:%S"`。
4. 桌面端根据 `use_log_file_` 决定是否添加 Boost.Log 文件 sink；iOS/Android 上 `UseLogFile()` 始终返回 true。

### 3.4 默认日志级别
- `_DEBUG` 构建：默认 `log_level_ = 1`（debug）。
- Release 构建：默认 `log_level_ = 3`（warning）。
- 最终过滤通过 `boost::log::core::get()->set_filter(severity >= log_level)` 生效。

### 3.5 用户自定义字面量语法
提供 `inline namespace LogLiterals`，允许以 C++ 字符串字面量的方式调用：
```cpp
"message"_log_info();
"message"_log_debug();
"message"_log_warning();
"message"_log_error();
```
底层通过 `LogState` 保存消息与级别，析构时触发 `LoggerSingletone::Log`。

### 3.6 结构化字段
当前实现未引入结构化字段（JSON/键值对）；日志输出是纯文本，仅包含时间戳、级别、源码位置与消息正文。Boost.Log formatter 定义为：
`TimeStamp <severity> [file:line] function_name: message\n`。

## 4. 约定与约束

- **平台分支约定**：所有平台差异通过 `#ifdef __ANDROID__`、`#if defined(TARGET_OS_IOS) && TARGET_OS_IOS`、`#ifndef __ANDROID__ && !TARGET_OS_IOS` 三段互斥分支处理，新增后端需遵循此模式。
- **级别数字约定**：新增级别需在 `GetLogLevel`、`GetiOSLogType`、`GetAndroidLogPriority` 三处同步维护映射，并在 `Log()` 的 switch 分支中添加 case。
- **配置键名约定**：INI 中必须存在的键包括 `log.log_level`、`log.log_level_debug`、`log.use_log_file`、`log.log_file`、`log_format.log_datatime_format`；缺失任一键会抛出 `boost::property_tree::ptree_bad_path` 并回退默认值。
- **线程安全约定**：所有公开读操作（如 `UseLogFile()`）必须持有 `std::shared_lock`；构造期间不持锁，因此构造必须在首次 `GetInstance()` 之前完成。
- **源码位置约定**：所有 `Log*` 方法签名均带 `std::source_location` 默认参数，调用方不应显式传入，应依赖 `std::source_location::current()` 自动捕获。
- **导出宏约定**：所有对外符号使用 `HSBA_SLICER_LOG_API` 宏标记，由 `logger/export.h` 控制 DLL 导出。
- **日志输出目标**：桌面端同时启用 console sink 与可选 file sink；iOS/Android 不产生本地文件，而是走系统日志通道。