---
kind: logging_system
name: 跨平台日志子系统（HsBaSlicerLog）
category: logging_system
scope:
    - '**'
source_files:
    - logger/logger.hpp
    - logger/logger.cpp
    - logger/CMakeLists.txt
    - utils/logcfg.ini
---

## 1. 使用的系统/方案

HsBaSlicer 自研了一个轻量级、线程安全的单例日志器 `LoggerSingletone`，位于 `logger/` 目录，对外以 `HsBa::Slicer::Log` 命名空间暴露。其核心思路是：

- **桌面 / Linux / macOS**：底层使用 Boost.Log (`boost/log/trivial.hpp`)，通过 `add_console_log` + `add_file_log` 同时输出到控制台与文件。
- **iOS**：绕过 Boost.Log，直接使用 `<os/log.h>` 的 `os_log_with_type` 写入系统日志。
- **Android**：绕过 Boost.Log，直接使用 `<android/log.h>` 的 `__android_log_print` 写入内核日志。

日志级别采用统一的整数语义（0–5），在三个平台分别映射到各自后端的等级枚举：

| 内部 level | 含义   | Boost.Log          | iOS os_log_type_t    | Android __android_log | 默认行为（_DEBUG） |
|-----------|--------|--------------------|----------------------|-----------------------|-------------------|
| 0         | trace  | trace              | OS_LOG_TYPE_DEBUG    | ANDROID_LOG_VERBOSE   | debug             |
| 1         | debug  | debug              | OS_LOG_TYPE_DEBUG    | ANDROID_LOG_DEBUG     | debug             |
| 2         | info   | info               | OS_LOG_TYPE_INFO     | ANDROID_LOG_INFO      | warning           |
| 3         | warning| warning            | OS_LOG_TYPE_DEFAULT  | ANDROID_LOG_WARN      | warning           |
| 4         | error  | error              | OS_LOG_TYPE_ERROR    | ANDROID_LOG_ERROR     | warning           |
| 5         | fatal  | fatal              | OS_LOG_TYPE_FAULT    | ANDROID_LOG_FATAL     | warning           |

## 2. 关键文件与包

- `logger/logger.hpp` — 公共接口：`LoggerSingletone` 类、`LogState` 流式状态对象、以及 `_log_debug/_log_info/_log_warning/_log_error` 用户自定义字面量。
- `logger/logger.cpp` — 实现：构造时加载配置、初始化各平台后端、按 level 分发到对应 sink。
- `logger/CMakeLists.txt` — 构建目标 `HsBaSlicerLog`（静态或共享库），按平台链接 Boost.log / log / 无依赖。
- `utils/logcfg.ini` — 默认配置文件模板，定义 `[log]` 与 `[log_format]` 两个 section。
- `docs/en/logger/` 与 `docs/zh/logger/` — Doxygen 文档入口。

调用方集中在各示例可执行入口（`samples/*/main.cpp`、`HsBaSlicer/HsBaSlicer.cpp`），统一通过 `LoggerSingletone::LogInfo(...)` 打印流水线进度；未发现业务模块中直接调用 `_log_*` 字面量的代码。

## 3. 架构与约定

### 3.1 单例与线程安全

`LoggerSingletone` 使用 `std::shared_ptr` + `std::once_flag` + `std::shared_mutex` 实现线程安全的懒加载单例：

- `GetInstance()` 通过 `std::call_once` 保证只创建一次。
- `CreateInstance()` 通过私有嵌套结构体 `Private` 控制构造。
- `UseLogFile()` 读取时使用 `std::shared_lock` 保护成员变量。

### 3.2 配置加载

构造时从当前工作目录下的 `logcfg.ini` 读取配置（路径拼接为 `current_path()/"/logcfg.ini"`），使用 `fileoperator/rw_ptree.hpp` 提供的 `Config::from_ini` 解析。字段约定如下：

```ini
[log]
log_level = 3                # release 默认值
log_level_debug = 1          # debug 默认值
use_log_file = false
log_file = /log/log.txt
[log_format]
log_datatime_format=%Y-%m-%d %H:%M:%S
log_time_format=%H:%M:%S
```

若文件不存在或字段缺失（捕获 `ptree_bad_path`），回退到默认值：`log_path_ = current_path() + "/log/log.txt"`，`use_log_file_ = false`，时间格式 `%Y-%m-%d %H:%M:%S`。

### 3.3 输出格式

Boost.Log 端 formatter 固定为：

```
[TimeStamp] <severity> [file:line] function_name: message\n
```

其中 `TimeStamp` 由 `log_datatime_format` 控制，`[file:line] function_name:` 由 `GetSourceLocation(std::source_location)` 生成，自动携带 C++20 源码位置信息。

### 3.4 文件轮转

当启用文件输出时，Boost.Log 配置了两种轮转策略：

- `rotation_size = SIZE_50MB`（50 MB）
- `time_based_rotation = rotation_at_time_point(0, 0, 0)`（每日 00:00 切分）
- `open_mode = std::ios::app`（追加模式）

常量 `SIZE_1KB=1024`、`SIZE_1MB=1024*1024`、`SIZE_50MB=50*SIZE_1MB` 定义在 logger.cpp 内部命名空间中。

### 3.5 流式 API（用户自定义字面量）

`inline namespace LogLiterals` 暴露四个字面量：

- `"msg"_log_debug` → level 1
- `"msg"_log_info` → level 2
- `"msg"_log_warning` → level 3
- `"msg"_log_error` → level 4

它们返回 `LogState`，析构时调用 `LoggerSingletone::Log(message_, log_lv_, location)`，从而获得自动捕获的 `std::source_location`。

### 3.6 平台分支策略

所有日志输出点均通过 `#ifdef __ANDROID__` / `#if TARGET_OS_IOS` / 其他分支三向分流：

- Android：`__android_log_print(priority, "HsBaSlicer", "%s%s", source_location, message)`
- iOS：`os_log_with_type(handle, type, "%s%s", source_location, message)`
- 桌面：`BOOST_LOG_TRIVIAL(level) << source_location << message`

## 4. 约定与约束

- **日志级别为 0–5 的整数**，不是枚举；新增级别需同步修改 `GetLogLevel`、`GetAndroidLogPriority`、`GetiOSLogType` 以及 `LoggerSingletone::Log` 的 switch 分支（logger.cpp 内三处 switch）。这是实现上的硬约束——任意不在 0–5 的值会落入 default 分支。
- **调试/发布默认级别不同**：`_DEBUG` 下 `log_level_ = 1`（debug），非 `_DEBUG` 下 `log_level_ = 3`（warning）；release 下未匹配到的 level 也回落到 warning。
- **配置文件路径固定**：构造时总是查找 `current_path()/"/logcfg.ini"`，且文件路径前缀固定为 `/log/log.txt`（注意该前缀是绝对路径风格，在 Windows 上可能不符合预期）。
- **iOS/Android 不写文件**：`UseLogFile()` 在这两个平台上始终返回 `true`（表示“有日志输出”），但实际走系统日志而非磁盘文件。
- **Boost.Log 仅在非移动端启用**：`logger.cpp` 顶部 `#if !defined(__ANDROID__) && !(defined(TARGET_OS_IOS) && TARGET_OS_IOS)` 条件编译包含 Boost.Log 头文件，CMake 也在非 Android/iOS 平台链接 `Boost::log_setup` 与 `Boost::log`。
- **导出宏**：所有公开符号使用 `HSBA_SLICER_LOG_API` 宏（来自 `logger/export.h`），Windows 动态库构建时通过 `HSBA_SLICER_LOG_EXPORTS` 定义控制导出。
- **调用约定**：仓库中的现有调用方（`HsBaSlicer/HsBaSlicer.cpp` 与各 `samples/*/main.cpp`）统一使用 `LoggerSingletone::LogInfo(...)` 静态方法，而非 `_log_*` 字面量；未见业务逻辑中使用 `_log_debug/_log_warning/_log_error` 字面量的代码。
- **结构化字段**：当前日志消息是纯字符串拼接，没有独立的 structured fields（如 JSON 字段）；唯一附加的结构化元数据是 `std::source_location` 生成的 `[file:line] function_name:` 前缀。