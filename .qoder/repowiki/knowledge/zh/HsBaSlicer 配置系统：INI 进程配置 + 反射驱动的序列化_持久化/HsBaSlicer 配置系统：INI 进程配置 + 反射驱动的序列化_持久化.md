---
kind: configuration_system
name: HsBaSlicer 配置系统：INI 进程配置 + 反射驱动的序列化/持久化
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - restapi/restapi.ini
    - restapi/main.cpp
    - restapi/config_bridge.cpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - utils/logcfg.ini
    - utils/struct_json.hpp
    - utils/struct_yaml.hpp
    - utils/struct_xml.hpp
    - fileoperator/param_store.hpp
    - fileoperator/param_schema.hpp
    - base/static_reflect.hpp
---

## 1. 总体方案

HsBaSlicer 的配置体系由三层组成，没有使用统一的运行时配置框架（如 Boost.Config、envoy 等），而是按职责拆分：

- **进程级配置**：REST 服务通过 `restapi.ini`（boost::property_tree）加载 `ServerConfig`，默认值写在代码中，缺失文件等价于空覆盖。
- **应用级共享配置**：`utils/app_config.{hpp,cpp}` 提供线程安全的 `AppConfigSingletone` 单例，目前暴露外部工具路径（7-Zip）。
- **流水线参数配置（ParamStore）**：基于反射的 C ABI 结构体 → SQL wide-table 持久化管线，支持 SQLite / MySQL / PostgreSQL，并可选 AES-256-CBC 静态加密。

所有结构化数据（JSON/XML/YAML）统一走 `utils/struct_json.hpp`、`utils/struct_yaml.hpp`、`utils/struct_xml.hpp` 三套模板库，底层依赖 `base/static_reflect.hpp` 提供的 `StaticReflect::Reflector<T>` 与 `Utils::TypeInfo` 字段表。

## 2. 关键文件

| 层次 | 文件 | 作用 |
|---|---|---|
| REST 进程配置 | `restapi/server_config.hpp/.cpp` | `LoadServerConfig(path)` 从 ini 填充 `ServerConfig` |
| REST 配置文件 | `restapi/restapi.ini` | `[server]`、`[paramstore]`、`[auth]` 三段键值对 |
| 命令行入口 | `restapi/main.cpp` | 解析 `--config` / `-c`、`--host`、`--port`，默认 `./restapi.ini` |
| 应用单例 | `utils/app_config.hpp/.cpp` | `AppConfigSingletone::GetInstance()` 暴露 `GetSevenZPath()` |
| 日志配置 | `utils/logcfg.ini` | `[log]`、`[log_format]` 段（供 logger 模块读取） |
| JSON 序列化 | `utils/struct_json.hpp` | `to_json`/`from_json` 泛型模板，基于 RapidJSON |
| YAML 序列化 | `utils/struct_yaml.hpp` | `to_yaml`/`from_yaml` 泛型模板，基于 yaml-cpp |
| XML 序列化 | `utils/struct_xml.hpp` | `to_xml`/`from_xml` 泛型模板，基于 tinyxml2 |
| 配置桥接 | `restapi/config_bridge.cpp` | 将 C ABI 配置结构体 ↔ JSON/XML 双向转换 |
| 参数存储 | `fileoperator/param_store.hpp` | `ParamStore` CRUD + 事务 + 进度回调 + 迁移钩子 |
| 参数 Schema | `fileoperator/param_schema.hpp` | 从 `TypeInfo::fields` 推导 DDL，映射 Double/Int64/Text → 各后端列类型 |
| 反射基础 | `base/static_reflect.hpp` | `StaticReflect::Reflector<T>::FieldCount()/FieldName/N GetField` |
| Proto 定义 | `proto/*.proto` | 各工艺 PipelineConfig 的结构定义（生成 C ABI 头） |

## 3. 架构与约定

### 3.1 REST 进程配置（`restapi.ini`）

- 格式：INI，由 `Config::from_ini`（`fileoperator/rw_ptree.hpp` 包装 boost::property_tree）解析。
- 分段：`[server]`（host/port/default_format/max_concurrent_jobs/max_jobs/result_ttl_seconds）、`[paramstore]`（backend/sqlite_path/host/user/password/database/port/crypto_password）、`[auth]`（预留，当前未读）。
- 默认值策略：`ServerConfig` 每个成员都有内置默认值；`LoadServerConfig` 在文件不存在时直接返回默认实例，不抛异常。`pt.get<std::string>("key", cfg.key)` 模式实现“仅覆盖非空项”。
- 数值校验：端口范围 `(0, 65535]`、`default_format` 只接受 `json`/`xml`、并发数 ≥ 1、TTL ≥ 0，非法值被归一化到安全默认值。
- 命令行覆盖：`main.cpp` 先加载 ini，再按优先级用 `--host` / `--port` 覆盖。

### 3.2 应用单例配置（`AppConfigSingletone`）

- 线程安全：`std::shared_mutex` 保护懒加载的单例指针和 `sevenZ_path_` 字段。
- 生命周期：`GetInstance()` 懒初始化，`DeleteInstance()` 显式释放（无 RAII 自动销毁）。
- 用途：集中存放跨切片流程的外部工具路径，避免散落的字符串常量。

### 3.3 反射驱动的配置序列化（JSON/YAML/XML）

三个头文件遵循同一设计模式：

- 支持三类目标类型：
  - **Aggregate 类型**（`Aggregte<T>` concept）：通过 `boost::pfr::names_as_array` + `for_each_field` 遍历字段。
  - **Reflectable 类型**（`Reflectable<T>` concept）：通过 `StaticReflect::Reflector<T>` 编译期展开字段。
  - **自定义可转换类型**：实现 `to_json/from_json`、`to_yaml/from_yaml` 或 `to_xml/from_xml` API。
- 字段类型分发：枚举 → 字符串（`EnumName`）、算术类型、`std::string`/`string_view`、`OptionalLike`、range、嵌套聚合/reflectable。
- 公共 API：`to_json`/`from_json`、`write_pretty_json`/`read_json_from_file`、`to_yaml`/`from_yaml`、`to_xml`/`from_xml` 等重载族。
- 错误处理：不支持的类型抛出 `RuntimeError`；缺少根元素/非对象抛出 `InvalidArgumentError`。

### 3.4 配置桥接（`config_bridge`）

- 面向 C ABI 的扁平结构体（来自 `pipelinetypes/pipeline_types.h`）。
- 通过 `Utils::TypeInfo::fields`（name → (child TypeInfo*, byte offset)）+ `FieldToAny`/`AnyToField` 完成任意偏移字段的读写。
- 支持的 Kind：`CStr`（const char*）、`Double`（float/double）、`Int64`（int/int64_t/enum），其余为 `Unsupported`（反序列化时跳过，保留基值）。
- JSON 输出使用 `rapidjson::PrettyWriter`，XML 使用 `tinyxml2::XMLDocument` + `XMLPrinter`，保证转义正确。

### 3.5 ParamStore（流水线参数持久化）

- 每类 `PipelineConfig` 对应一张 wide-table，固定元数据列：`param_id`（自增主键）、`param_key`（业务唯一键）、`schema_version`、`created_at`、`updated_at`。
- 字段列类型由 `ColumnKind`（Double/Int64/Text）映射到各后端方言：SQLite REAL/BIGINT/TEXT、MySQL DOUBLE/BIGINT/TEXT、PostgreSQL DOUBLE PRECISION/BIGINT/TEXT。
- 可选静态加密：`SetCryptoKey` 启用后，Save 把反射后的列映射序列化为二进制并用 AES-256-CBC（密码派生）加密存入 `param_blob`，Load 时解密还原；明文列（param_key/schema_version/时间戳）始终可读。
- 事务：`SaveBatch` 包裹单次事务，失败回滚并抛异常。
- 扩展点：`SetProgressCallback`（percent + stage 描述）、`SetMigrationHook`（后端/表/版本回调）。

## 4. 约定与约束

- **INI 是唯一进程级配置格式**：`restapi.ini` 与 `logcfg.ini` 均通过 `Config::from_ini` 解析，键名大小写敏感但 `backend` 字段会经 `to_lower` 规范化后再匹配（`mysql`/`postgresql`/`postgres`/`sqlite`/`none`）。
- **代码内默认值优先**：`ServerConfig` 所有成员均有默认值，ini 文件缺失等价于空覆盖；`LoadServerConfig` 文档明确声明“missing file is not an error”。
- **命令行覆盖 ini**：`--config`/`-c` 指定 ini 路径，`--host`/`--port` 在加载后覆盖对应字段，优先级高于 ini。
- **数值边界强制归一化**：端口必须 ∈ (0, 65535]，`default_format` 非 `xml` 即回退为 `json`，`max_concurrent_jobs`/`max_jobs` ≥ 1，`result_ttl_seconds` ≥ 0（见 `server_config.cpp:65-76`）。
- **反射字段命名即序列化键名**：JSON/YAML/XML 字段名直接取自 `StaticReflect::Reflector<T>::FieldName<N>()` 或 `boost::pfr::names_as_array`，新增字段需同时出现在 proto 定义与反射注册处。
- **C ABI 配置结构体仅支持三种 Kind**：`const char*`、`double`/`float`、`int`/`int64_t`/enum；其他类型在 `config_bridge` 中被归类为 `Unsupported`，反序列化时静默忽略。
- **ParamStore 错误不吞掉**：`last_error` 记录最近失败，但异常仍向上传播，调用方（含 Lua）自行决定处理方式（见 `param_store.hpp` 注释）。
- **ParamStore 加密密钥为空即禁用加密**：`crypto_key_.empty()` 表示回退到宽表明文模式；已加密行只能用相同密钥解密。
- **未使用环境变量**：整个仓库未发现 `getenv`/`setenv`/`.env` 文件加载逻辑，配置来源仅限于 ini 文件、命令行参数与代码默认值。
- **Feature flags**：未见独立的 feature flag 机制；开关以 ini 键（如 `auth.enabled`、`paramstore.backend = none`）或编译期宏形式存在。