---
kind: logging_system
name: 跨平台单例日志系统（Boost.Log + 原生后端）
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - utils/logcfg.ini
    - logger/export.h
    - HsBaSlicer/HsBaSlicer.cpp
---

## 1. 使用的系统与框架

HsBaSlicer 在 `logger/` 子模块中实现了自研的跨平台日志子系统，对外暴露 `HsBa::Slicer::Log` 命名空间。核心类为 `LoggerSingletone`（单例），通过 `std::shared_mutex` + `std::call_once` 保证线程安全与一次性初始化。

后端按平台分支：
- 桌面 / Android 以外的平台：使用 **Boost.Log**（`boost/log/trivial.hpp`、`text_file_backend`、`console` sink），支持文件轮转（50MB + 每日零点滚动）与控制台输出。
- iOS：使用系统 `os_log` API（`os_log_create` + `os_log_with_type`）。
- Android：使用内核日志 `__android_log_print`。

日志级别采用整数 0–5 映射到各平台语义：trace/debug/info/warning/error/fatal。

## 2. 关键文件

- `logger/logger.hpp` — 公共接口：`LoggerSingletone` 静态方法 `LogDebug/LogInfo/LogWarning/LogError`、`GetInstance()`、`UseLogFile()`；以及 `LogLiterals` 内联命名空间下的用户自定义字面量 `_log_debug/_log_info/_log_warning/_log_error`，返回 `LogState` 以支持 `"msg"_log_info()` 风格的链式调用。
- `logger/logger.cpp` — 实现：构造时加载 `logcfg.ini`，配置 Boost.Log 或原生后端；`Log()` 根据 level 分发到对应后端；默认日志路径 `./log/log.txt`。
- `utils/logcfg.ini` — 默认配置文件模板，字段包括 `[log] log_level / log_level_debug / use_log_file / log_file` 与 `[log_format] log_datatime_format / log_time_format`。
- `logger/export.h` — 导出宏 `HSBA_SLICER_LOG_API`。

## 3. 架构与约定

- **单例模式**：通过 `CreateInstance()` + `GetInstance()` 配合 `std::once_flag` 延迟创建实例；所有 `Log*` 入口在 `instance_ == nullptr` 时触发懒初始化。
- **源码位置自动捕获**：所有公开 `Log*` 方法签名包含 `const std::source_location& location = std::source_location::current()`，内部 `GetSourceLocation()` 将其格式化为 `[file:line] function_name: ` 前缀并拼接到消息前。
- **结构化字段**：当前实现是纯文本日志，未引入键值对结构体；结构化信息通过字符串拼接（如 source_location 前缀）提供。
- **配置驱动**：构造时读取工作目录下的 `logcfg.ini`；若 key 缺失抛出 `ptree_bad_path` 则回退到默认值（debug 下 level=1，release 下 level=3；关闭文件输出；时间格式 `%Y-%m-%d %H:%M:%S`）。
- **平台条件编译**：`#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)` 包裹 Boost.Log 依赖；iOS/Android 分支分别处理。
- **日志级别阈值**：Boost.Log 侧通过 `core->set_filter(severity >= log_level)` 过滤；iOS/Android 侧通过各自 API 的类型/优先级参数控制。

## 4. 约定与约束

- **调用方式**：代码库中统一通过 `HsBa::Slicer::Log::LoggerSingletone::LogInfo(...)` 等静态方法调用，或使用 `using namespace HsBa::Slicer::Log::LogLiterals;` 后以 `"..."_log_info()` 字面量语法调用（见 `HsBaSlicer/HsBaSlicer.cpp`、`samples/*/main.cpp`）。
- **级别数值约定**：0=trace, 1=debug, 2=info, 3=warning, 4=error, 5=fatal（由 `GetLogLevel`/`GetiOSLogType`/`GetAndroidLogPriority` 三处 switch 共同定义）。
- **调试/发布差异**：`_DEBUG` 下默认 `log_level_=1`（debug），非 `_DEBUG` 下默认 `log_level_=3`（warning）；同时 `logcfg.ini` 中 debug/release 使用不同 key（`log_level_debug` vs `log_level`）。
- **文件输出开关**：`use_log_file=false` 时不挂载文件 sink；iOS/Android 上 `UseLogFile()` 始终返回 true（因为走系统日志）。
- **日志轮转策略**：Boost.Log 端固定为 `rotation_size = 50MB` + `rotation_at_time_point(0,0,0)`（每日零点）。该策略硬编码于 `logger.cpp` 常量 `SIZE_50MB`，未在配置文件中暴露。
- **线程安全**：`GetInstance()` 使用 `std::call_once`；`UseLogFile()` 加 `std::shared_lock(mutex_)`；但 `Log*` 写入路径直接调用 Boost.Log 或原生 API，未额外加锁——依赖底层 sink 自身的并发安全。
- **无结构化字段**：当前没有 JSON/XML 结构化日志字段；如需结构化输出需扩展 `formatter` 或新增 sink。

## 5. 文档参考

仓库 `docs/zh/logger/` 与 `docs/en/logger/` 下存在 logger 模块的 Doxygen 文档页，说明该子系统是项目正式文档覆盖的模块之一。