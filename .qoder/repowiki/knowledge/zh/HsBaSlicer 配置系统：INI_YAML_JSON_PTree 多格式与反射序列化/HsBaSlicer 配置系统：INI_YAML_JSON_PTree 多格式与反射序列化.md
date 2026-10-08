---
kind: configuration_system
name: HsBaSlicer 配置系统：INI/YAML/JSON/PTree 多格式与反射序列化
category: configuration_system
scope:
    - '**'
source_files:
    - utils/app_config.hpp
    - utils/app_config.cpp
    - utils/logcfg.ini
    - logger/logger.cpp
    - fileoperator/rw_ptree.hpp
    - fileoperator/rw_ptree.cpp
    - utils/struct_yaml.hpp
    - utils/struct_json.hpp
    - utils/struct_xml.hpp
    - tests/AppConfig/app_config_test.cpp
---

## 1. 使用的系统与工具

仓库没有统一的“应用配置框架”，而是由三套互补机制组成：
- **运行时进程内配置**：`utils/app_config.hpp/.cpp` 中的 `AppConfigSingletone`，以双检锁 + `std::shared_mutex` 实现线程安全的单例，当前仅暴露 `GetSevenZPath()` 一个外部工具路径字段。
- **日志子系统配置**：`logger/logger.cpp` 在构造时从工作目录下的 `logcfg.ini`（`fileoperator/rw_ptree.hpp` 提供的 `Config::from_ini`）加载 Boost.PropertyTree，再驱动 Boost.Log / iOS os_log / Android __android_log_print 三个后端。
- **结构化配置持久化**：`utils/struct_yaml.hpp`、`utils/struct_json.hpp`、`utils/struct_xml.hpp` 提供基于 `yaml-cpp`、`nlohmann/json`、`tinyxml2` 的模板化反射序列化；`fileoperator/rw_ptree.hpp` 提供 INI/JSON/XML 与 `boost::property_tree::ptree` 之间的双向转换，以及 `VariantConfigMap` / `AnyConfigMap` 两种键值存储抽象。

依赖来源：`vcpkg.json` 引入 yaml-cpp、nlohmann-json、tinyxml2、boost-property-tree；Boost.PFR (`boost/pfr/core_name.hpp`) 用于聚合类型字段名反射。

## 2. 关键文件

- `utils/app_config.hpp`, `utils/app_config.cpp` — 进程内单例配置（7-Zip 路径）。
- `utils/logcfg.ini` — 日志配置文件样例（`[log]`、`[log_format]` 两个 section）。
- `logger/logger.cpp` — 日志单例，读取 `logcfg.ini`，按平台选择后端。
- `fileoperator/rw_ptree.hpp`, `fileoperator/rw_ptree.cpp` — INI/JSON/XML ↔ ptree 转换，`IConfigMap` / `VariantConfigMap<T...>` / `AnyConfigMap` 配置存储接口。
- `utils/struct_yaml.hpp` — 基于 boost.pfr + StaticReflect 的 YAML 反射序列化（to_yaml/from_yaml/write_yaml/read_yaml_from_file 等）。
- `utils/struct_json.hpp`, `utils/struct_xml.hpp` — JSON/XML 同构反射序列化。
- `tests/AppConfig/app_config_test.cpp` — 验证单例稳定性、默认空值、DeleteInstance 幂等、多线程并发 GetInstance。

## 3. 架构与设计约定

### 3.1 运行时配置（AppConfigSingletone）
- 命名空间 `HsBa::Slicer`。
- 通过静态成员 `instance_` + `mutex_` 实现懒初始化；`GetInstance()` 先快速检查指针，再上锁二次检查；`DeleteInstance()` 释放并置空，支持重复调用。
- 读路径使用 `std::shared_lock`，写路径使用 `std::unique_lock`，保证读写分离的共享互斥。
- 测试覆盖 8 线程并发场景，断言所有线程拿到同一地址。

### 3.2 日志配置（LoggerSingletone + logcfg.ini）
- 构造函数中拼接 `current_path() + "/logcfg.ini"`，若文件存在则用 `Config::from_ini` 解析；否则回退到默认值：`log_path_ = current_path + "/log/log.txt"`、`use_log_file_ = false`、`log_datatime_format_ = "%Y-%m-%d %H:%M:%S"`。
- Debug 构建优先取 `log.log_level_debug`，Release 取 `log.log_level`。
- 桌面端（非 Android/iOS）启用 Boost.Log，同时输出 console 与可选 file sink（50MB 轮转 + 每日轮转）；Android 走 `__android_log_print`，iOS 走 `os_log`。
- 提供 `_log_debug` / `_log_info` / `_log_warning` / `_log_error` 用户自定义字面量，内部通过 `LogState` 延迟捕获 `std::source_location`。

### 3.3 结构化配置（YAML/JSON/XML + PTree）
- `struct_yaml.hpp` 定义 `YAMLValueConvertible` concept（要求 `to_yaml(YAML::Node&)` 与 `T::from_yaml(const YAML::Node&)`），对枚举、字符串、算术类型、`std::optional`、range、嵌套聚合/reflectable 类型做递归处理。
- 支持两类类型：
  - `Aggregte`（由 boost.pfr 自动推导字段名）。
  - `Reflectable`（由 `base/static_reflect.hpp` 的 `StaticReflect::Reflector<T>` 显式注册）。
- 顶层 API：`to_yaml` / `from_yaml` / `write_yaml` / `read_yaml` / `write_yaml(path, value)` / `read_yaml_from_file(path)` / `read_yaml_from_string(str)`。
- `rw_ptree.hpp` 提供 `from_ini` / `from_xml` / `from_json` / `to_ini` / `to_xml` / `to_json` 六个函数，统一抛 `boost::property_tree::file_parser_error`。
- `IConfigMap` 抽象出两种实现：
  - `VariantConfigMap<Args...>`：编译期已知允许类型的 `std::variant` 存储，`GetOptional<T>` 返回 `std::optional<T>`，类型不匹配返回 `nullopt`。
  - `AnyConfigMap`：运行期 `std::any` 存储，可互相转换（`ToAnyMap` / `ToVariantConfigMap`）。
- `ChangeTranslator` 系列函数支持 ptree 值的旧/新格式迁移。

### 3.4 流水线配置（PipelineConfig）
- 通过 `proto/` 下的 `.proto` 文件（如 `fdm_pipeline.proto`、`sla_pipeline.proto` 等）定义各工艺的配置结构，再由 `convert/Msg2PipelineConfig.*` 与 `PipelineConfig2Msg.*` 在 protobuf 消息与 C++ 结构体之间转换。这属于“协议层配置”而非运行时文件配置，但与上述 YAML/JSON 序列化体系配合使用。

## 4. 约定与约束

- **日志配置文件位置固定**：`logger/logger.cpp` 硬编码为 `<current_path>/logcfg.ini`，section 名为 `log` 和 `log_format`，key 包括 `log_level`、`log_level_debug`、`use_log_file`、`log_file`、`log_datatime_format`、`log_time_format`（见 `utils/logcfg.ini` 样例）。
- **缺失配置项行为**：`logger.cpp` 在 `ptree.get` 抛出 `ptree_bad_path` 时回退到默认值；`struct_yaml.hpp` 在字段不存在时跳过赋值（`if (!field_node) return;`），即“缺省字段保持默认构造值”。
- **不支持 TOML**：虽然 `static_tests/StructToml/` 目录存在，但未见对应实现头文件；实际可用的结构化配置格式为 YAML、JSON、XML、INI。
- **无环境变量或 .env 文件加载**：代码库中没有发现 `getenv` / `SetEnvironmentVariable` 等环境变量读取逻辑，也没有 `.env*` 文件。
- **无集中式 feature flag 系统**：功能开关通过编译期宏（如 `_DEBUG`、`TARGET_OS_IOS`、`__ANDROID__`）与配置文件（`logcfg.ini`）控制，未发现运行时 feature flag 表。
- **AppConfigSingletone 目前只管理 7-Zip 路径**：新增运行时配置应遵循相同模式——在 `AppConfigSingletone` 中添加字段，并用 `shared_mutex` 保护读写访问。
- **序列化异常语义**：`struct_yaml.hpp` 对非法节点类型抛 `InvalidArgumentError`，IO 失败抛 `IOError`，`rw_ptree.hpp` 抛 `boost::property_tree::file_parser_error`；调用方需区分这些异常类型进行错误处理。
- **枚举序列化**：YAML/JSON/XML 序列化依赖 `EnumName` / `EnumFromName`（来自 `base/concepts.hpp`），新增枚举必须实现这两个函数才能被反射序列化。