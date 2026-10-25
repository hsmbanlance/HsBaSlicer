---
kind: logging_system
name: 跨平台日志系统（LoggerSingletone + Boost.Log/Android/iOS）
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - logger/CMakeLists.txt
    - utils/logcfg.ini
---

## 1. 系统与架构

HsBaSlicer 在 `logger/` 子模块中实现了自托管的线程安全单例日志器 `HsBa::Slicer::Log::LoggerSingletone`，对外暴露静态方法 `LogDebug/LogInfo/LogWarning/LogError` 以及用户自定义字面量 `_log_debug/_log_info/_log_warning/_log_error`。底层根据编译目标平台选择不同后端：

- **桌面 / Linux**：使用 Boost.Log (`boost/log/trivial.hpp`)，同时输出到控制台与可选文件。
- **Android**：使用 `__android_log_print` 写入内核 logcat。
- **iOS**：使用 `os_log` 写入系统日志。

日志级别采用统一的整数枚举（0=trace, 1=debug, 2=info, 3=warning, 4=error, 5=fatal），由 `GetLogLevel` / `GetAndroidLogPriority` / `GetiOSLogType` 三个平台函数映射到各后端的原生级别。

构造时通过 `std::filesystem::current_path() + "/logcfg.ini"` 加载配置；若不存在或字段缺失则回退为默认值（DEBUG 构建默认 level=1，Release 默认 level=3，关闭文件输出）。日志格式包含时间戳、严重级别和消息体，并附加源码位置信息（`[file:line] function_name:`）。

## 2. 关键文件

- `logger/logger.hpp` — 公共 API：`LoggerSingletone` 类、`LogState`、四个 `_log_*` 字面量。
- `logger/logger.cpp` — 实现：平台分支、Boost.Log 初始化、Android/iOS 后端、配置文件解析、日志路由。
- `logger/CMakeLists.txt` — 构建定义：按 WIN32/ANDROID/IOS/else 链接不同库（Boost.log、android log、仅基础库）。
- `utils/logcfg.ini` — 运行时配置模板，键包括 `log.log_level`、`log.log_level_debug`、`log.use_log_file`、`log.log_file`、`log_format.log_datatime_format`。
- `docs/en/logger/`、`docs/zh/logger/` — Doxygen 文档目录（说明该模块有独立文档）。

## 3. 设计约定与行为

- **单例模式**：通过 `GetInstance()` + `std::call_once` 延迟创建；所有 `Log*` 入口在实例为空时也会触发懒初始化。
- **线程安全**：`UseLogFile()` 使用 `std::shared_mutex` 保护；全局实例初始化用 `std::once_flag`。
- **源码位置**：所有日志调用都携带 `std::source_location::current()`，由 `GetSourceLocation` 格式化进输出。
- **文件轮转**：Boost.Log 文件 sink 启用 `rotation_size = 50MB` 与每日午夜轮转（`rotation_at_time_point(0,0,0)`）。
- **平台差异**：
  - Android/iOS 上 `UseLogFile()` 始终返回 true（因为不写磁盘）。
  - Android/iOS 构造时会打印一条提示日志表明使用了系统日志。
  - 非 Android/iOS 路径下才初始化 Boost.Log 控制台+文件 sink。
- **配置优先级**：`logcfg.ini` 存在且字段可解析时覆盖默认值；`ptree_bad_path` 异常被捕获后回退到默认路径 `/log/log.txt` 且关闭文件输出。
- **导出宏**：共享库构建时通过 `HSBA_SLICER_LOG_EXPORTS` 配合 `export.h` 中的 `HSBA_SLICER_LOG_API` 控制符号导出。

## 4. 使用方式（仓库内观察到的模式）

- 直接调用：`LoggerSingletone::LogInfo(msg);`（见 `HsBaSlicer/HsBaSlicer.cpp`、各 `samples/*/main.cpp`）。
- 流式字面量：`"initialize completed"_log_info();`（见 `HsBaSlicer/HsBaSlicer.cpp`）。

## 5. 约束与规则

- 日志级别必须使用 0–5 的整数值，超出范围在 DEBUG 构建下回退到 debug，Release 下回退到 warning（`logger.cpp` 中各 `Get*` 函数的 default 分支）。
- 日志文件路径总是以当前工作目录为前缀拼接 `logcfg.ini` 中 `log.log_file` 的值（`logger.cpp` 第 166 行）。
- 只有非 Android 且非 iOS 的平台才会启用 Boost.Log 控制台与文件 sink（`logger.cpp` 第 185–209 行的条件编译块）。
- 配置文件键名固定为 `[log]` 下的 `log_level` / `log_level_debug` / `use_log_file` / `log_file` 以及 `[log_format]` 下的 `log_datatime_format`（`logger.cpp` 第 160–167 行读取逻辑）。
- 所有公开 API 均带 `HSBA_SLICER_LOG_API` 导出标记，确保动态库场景下符号可见（`logger.hpp` 声明处及 `CMakeLists.txt` 中 `HSBA_SLICER_LOG_EXPORTS` 定义）。