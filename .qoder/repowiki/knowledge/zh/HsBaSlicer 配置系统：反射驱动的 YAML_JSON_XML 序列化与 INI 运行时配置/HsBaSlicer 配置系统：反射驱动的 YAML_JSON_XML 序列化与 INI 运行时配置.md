---
kind: configuration_system
name: HsBaSlicer 配置系统：反射驱动的 YAML/JSON/XML 序列化与 INI 运行时配置
category: configuration_system
scope:
    - '**'
source_files:
    - utils/app_config.hpp
    - utils/app_config.cpp
    - utils/struct_yaml.hpp
    - utils/struct_json.hpp
    - utils/struct_xml.hpp
    - utils/logcfg.ini
    - logger/logger.cpp
    - fileoperator/rw_ptree.cpp
    - tests/AppConfig/app_config_test.cpp
    - tests/ConfigMap/json_convert_test.cpp
    - tests/ConfigMap/struct_yaml_test.cpp
    - tests/ConfigMap/struct_xml_test.cpp
    - convert/Msg2PipelineConfig.cpp
    - convert/PipelineConfig2Msg.cpp
---

## 1. 使用的系统与工具

HsBaSlicer 没有统一的“应用配置框架”，而是由三块独立但风格一致的组件构成运行期配置能力：

- **INI 运行时配置**：`logger/logger.cpp` 通过 `boost::property_tree` 读取当前目录下的 `logcfg.ini`，解析 `log.log_level*`、`log.use_log_file`、`log.log_file`、`log_format.log_datatime_format` 等键，驱动日志级别、文件开关和路径。
- **通用结构体序列化（YAML / JSON / XML）**：`utils/struct_yaml.hpp`、`utils/struct_json.hpp`、`utils/struct_xml.hpp` 提供基于模板元编程的零样板序列化库，分别依赖 `yaml-cpp`、`rapidjson`、`tinyxml2`。
- **进程内单例配置**：`utils/app_config.cpp/.hpp` 暴露 `HsBa::Slicer::AppConfigSingletone`，目前仅持有 `sevenZ_path_` 字段并通过 `GetSevenZPath()` 访问，作为外部工具路径等共享设置的容器。

此外，`fileoperator/rw_ptree.cpp` 提供 `Config::from_ini` / `from_json` / `from_xml` 包装，供 logger 等模块直接调用；`convert/Msg2PipelineConfig.cpp`、`convert/PipelineConfig2Msg.cpp` 则把 protobuf 流水线配置在消息与 C++ 对象之间转换，是“流水线参数”的配置载体。

## 2. 关键文件

- `utils/app_config.hpp`, `utils/app_config.cpp` — 应用级单例配置
- `utils/struct_yaml.hpp` — YAML 反射序列化（`to_yaml` / `from_yaml` / `read_yaml_from_file` / `write_yaml`）
- `utils/struct_json.hpp` — JSON 反射序列化（`to_json` / `from_json` / `read_json_from_file` / `write_pretty_json`）
- `utils/struct_xml.hpp` — XML 反射序列化（`to_xml` / `from_xml` / `read_xml_from_file` / `write_xml`）
- `utils/logcfg.ini` — 默认日志配置文件
- `logger/logger.cpp` — 启动时加载 `logcfg.ini` 并初始化 Boost.Log
- `fileoperator/rw_ptree.cpp` — `Config::from_ini/json/xml` 对 boost::property_tree 的薄封装
- `tests/AppConfig/app_config_test.cpp` — AppConfigSingletone 的单例与线程安全测试
- `tests/ConfigMap/*` — JSON/YAML/XML 序列化测试套件
- `convert/Msg2PipelineConfig.cpp`, `convert/PipelineConfig2Msg.cpp` — PipelineConfig ↔ protobuf 转换

## 3. 架构与约定

### 3.1 反射驱动的序列化层
三个 `struct_*.hpp` 采用完全相同的模式：

1. 定义一个 concept（`YAMLValueConvertible` / `RapidJsonValueConvertible` / `TinyXmlConvertible`），要求类型实现 `to_*` 成员 + `from_*` 静态方法。
2. 对 `Aggregte`（POD-like，由 `base/concepts.hpp` 定义）使用 `boost::pfr` 按字段名自动遍历。
3. 对 `Reflectable`（由 `base/static_reflect.hpp` 的 `StaticReflect::Reflector<T>` 描述）按编译期字段索引展开。
4. 递归处理枚举（通过 `EnumName` / `EnumFromName`）、字符串、算术类型、`std::ranges::range`、`OptionalLike`（如 `std::optional`）。
5. 对外暴露统一 API：`to_*` / `from_*` / `read_*_from_file` / `write_*`。

这意味着新增一种可配置的结构体只需满足 `Aggregte` 或 `Reflectable`，无需手写序列化代码。

### 3.2 INI 配置加载约定
`logger/logger.cpp` 中构造 `LoggerSingletone` 时：

- 计算 `<current_dir>/logcfg.ini`。
- 若文件存在，用 `Config::from_ini(cfg_path)` 解析；否则回退到默认值（`log/log.txt`、关闭文件输出、`%Y-%m-%d %H:%M:%S`）。
- `_DEBUG` 下默认 log level = 1，Release 下为 3。
- 异常捕获 `boost::property_tree::ptree_bad_path`，失败时同样走默认配置。

### 3.3 进程内配置单例
`AppConfigSingletone` 使用 `std::shared_mutex` + 双重检查锁定（double-checked locking）实现懒加载单例，并提供 `DeleteInstance()` 显式销毁。测试覆盖多线程并发获取同一实例、删除后重建、重复删除无副作用等行为。

### 3.4 流水线配置
流水线参数以 protobuf 定义（`proto/fdm_pipeline.proto`、`sla_pipeline.proto`、`slm_pipeline.proto`、`sls_pipeline.proto`、`tdp_pipeline.proto`、`waam_pipeline.proto`、`custom_pipeline.proto`、`slice_config.proto`、`base_config.proto` 等），通过 `convert/Msg2PipelineConfig.*` 与 `convert/PipelineConfig2Msg.*` 在 protobuf 消息与 C++ 对象间双向转换，是 HsBaSlicer 对外暴露的“流水线配置”形态。

## 4. 约定与约束

- **日志配置文件位置固定**：logger 硬编码查找 `<current_path>/logcfg.ini`，不存在即回退默认值（见 `logger/logger.cpp:147-183`）。
- **INI 键名约定**：logger 期望 `log.log_level_debug`（debug 构建）/ `log.log_level`（release 构建）、`log.use_log_file`、`log.log_file`、`log_format.log_datatime_format` 这些键（见 `logger/logger.cpp:160-167`）。
- **AppConfigSingletone 生命周期**：必须通过 `GetInstance()` 获取，通过 `DeleteInstance()` 释放；测试断言多次 `GetInstance()` 返回同一地址（`tests/AppConfig/app_config_test.cpp:18-20`），且 `DeleteInstance()` 幂等（`tests/AppConfig/app_config_test.cpp:42-43`）。
- **序列化 API 一致性**：YAML/JSON/XML 三套库均暴露 `to_*` / `from_*` / `read_*_from_file` / `write_*` 命名空间函数，新增格式需遵循相同概念（`YAMLValueConvertible` / `RapidJsonValueConvertible` / `TinyXmlConvertible`）。
- **可选字段语义**：`OptionalLike` 字段在 YAML 中为空节点、JSON 中为 null、XML 中带 `null="true"` 属性时视为 reset（见 `struct_yaml.hpp:367-401`、`struct_json.hpp:402-440`、`struct_xml.hpp:452-508`）。
- **不支持的类型抛出 RuntimeError**：所有未识别字段类型在序列化/反序列化路径上均抛 `RuntimeError("Unsupported field type in ...")`，由上层 catch 或测试断言（见各 `struct_*.hpp` fallback 分支）。
- **PipelineConfig 不通过 YAML/JSON/XML 持久化**：流水线配置走 protobuf + Msg2PipelineConfig 转换器，而非上述反射序列化层。

该仓库没有集中式的“应用配置入口”（如 `config.yaml` 被主程序一次性加载），配置分散在日志 INI、进程内单例、protobuf 流水线参数三类载体中，并由 `utils/` 下的反射序列化库提供通用的结构化数据编解码能力。