---
kind: configuration_system
name: 配置系统：INI/JSON/XML 多源加载、结构体反射与进程参数持久化
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - restapi/restapi.ini
    - restapi/main.cpp
    - fileoperator/rw_ptree.hpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - fileoperator/param_store.hpp
    - utils/struct_json.hpp
    - logger/logger.cpp
---

## 1. 总体方案

HsBaSlicer 的运行时配置由三层组成，分别面向不同生命周期和用途：

| 层次 | 载体 | 位置 | 说明 |
|---|---|---|---|
| 进程级应用配置 | 单例 `AppConfigSingletone` | `utils/app_config.{hpp,cpp}` | 线程安全的进程内共享设置（当前暴露 `GetSevenZPath()`），通过 `GetInstance()` / `DeleteInstance()` 管理生命周期 |
| REST 服务器配置 | INI + CLI + 环境变量 | `restapi/server_config.{hpp,cpp}` + `restapi/restapi.ini` + `restapi/main.cpp` | 服务启动时从 `restapi.ini` 解析为 `ServerConfig` 结构体，再被命令行与环境变量覆盖 |
| 用户参数持久化 | SQL 后端（SQLite / MySQL / PostgreSQL） | `fileoperator/param_store.{hpp,cpp}` + `sql_adapter.*` | 通过 `ParamStore` 将任意 `PipelineConfig` 结构体序列化为宽表行，支持加密、迁移钩子与进度回调 |

此外，`fileoperator/rw_ptree.hpp` 提供基于 `boost::property_tree` 的通用 INI/JSON/XML 读写接口，`utils/struct_json.hpp`、`utils/struct_yaml.hpp`、`utils/struct_xml.hpp` 提供基于 `StaticReflect` 的结构体到 JSON/YAML/XML 的双向序列化。

## 2. 关键文件与包

- `restapi/server_config.hpp` — `ServerConfig` 结构体定义，所有字段带内置默认值；`LoadServerConfig(path)` 声明。
- `restapi/server_config.cpp` — INI → `ServerConfig` 的映射实现，含布尔/枚举/数值防御性归一化。
- `restapi/restapi.ini` — 默认配置文件，包含 `[server]`、`[upload]`、`[paramstore]`、`[auth]`、`[rate_limit]` 五个段。
- `restapi/main.cpp` — 入口：CLI 参数 → `LoadServerConfig` → `ApplyEnvFallbacks` → 最终覆盖顺序。
- `fileoperator/rw_ptree.hpp/.cpp` — `Config::from_ini/from_xml/from_json` 等统一属性树接口。
- `utils/app_config.hpp/.cpp` — 进程级单例配置。
- `fileoperator/param_store.hpp` — 进程参数持久化 API（CRUD、批量事务、加密、迁移钩子）。
- `logger/logger.cpp` — 日志子系统也使用同样的 `Config::from_ini("logcfg.ini")` 模式。
- `utils/struct_json.hpp` — 基于 RapidJSON + `StaticReflect` 的结构体 JSON 序列化模板库。

## 3. 架构与约定

### 3.1 配置加载优先级（REST 服务器）

`restapi/main.cpp` 中明确的覆盖链是：**INI 默认值 < 环境变量 < 命令行参数**。

```text
restapi.ini (LoadServerConfig)
    ↓ ApplyEnvFallbacks() 仅当 ini 值为空时才注入
    ↓ main() 中的 --host/--port/--auth-token/--tls-cert/--tls-key/--require-tls
    ↓ RunServer(cfg)
```

环境变量名固定为 `HSBA_REST_AUTH_TOKEN`、`HSBA_REST_TLS_CERT`、`HSBA_REST_TLS_KEY`，仅在 ini 对应字段为空时生效。该设计使容器镜像可以保留无凭据的 `restapi.ini`，凭据通过环境变量注入。

### 3.2 INI 解析与默认值策略

`LoadServerConfig` 对缺失的配置文件不报错——直接返回带内置默认值的 `ServerConfig`；只有文件存在但语法错误时才抛出 `boost::property_tree::file_parser_error`。每个字段通过 `pt.get<T>("section.key", cfg.field)` 读取，缺省值即结构体成员初始值。

### 3.3 类型安全与反射

- 结构体配置（如 `ServerConfig`）用普通 C++ 聚合类型承载，字段自带默认值，无需额外 schema。
- 用户参数（`PipelineConfig*`）通过 `StaticReflect` + `AnyObject` 在运行时遍历字段，配合 `ParamSchema` 生成 SQL 表。
- JSON/YAML/XML 序列化走 `utils/struct_json.hpp` 等模板库，区分三种类型：`Aggregte`（Boost.PFR 反射）、`Reflectable`（自定义 `StaticReflect`）、`RapidJsonValueConvertible`（自定义 `to_json/from_json`）。

### 3.4 进程参数存储（ParamStore）

`ParamStore` 以 `ISQLAdapter` 为后端抽象，支持 SQLite / MySQL / PostgreSQL。核心流程：

1. `EnsureSchema()` 按注册的 `PipelineConfigTag` 创建宽表。
2. `Save()` 执行 Validate → Reflect → Coerce → Upsert 阶段式流水线。
3. `SaveBatch()` 包裹在单个事务中，失败回滚并抛异常。
4. 可选 AES-256-CBC 列级加密（`SetCryptoKey`），明文列仅保留 `param_key`、`schema_version`、`created_at`、`updated_at`。
5. 错误不吞掉：`last_error_` 记录最后错误消息，同时重新抛出异常供调用方处理。

### 3.5 其他组件的配置模式

- 日志 (`logger/logger.cpp`)：从工作目录下的 `logcfg.ini` 读取 `log.log_level` / `log.use_log_file` / `log_format.log_datatime_format`，不存在则回退到默认路径 `/log/log.txt`。
- 应用配置 (`utils/app_config.cpp`)：双检锁单例，`shared_mutex` 保护读/写，暴露 `GetSevenZPath()`。

## 4. 约定与约束

- **INI 键命名**：采用 `section.key` 形式（如 `server.host`、`upload.evict_mode`、`paramstore.backend`），由 `restapi/restapi.ini` 作为权威示例。
- **缺失配置文件 = 默认值**：`LoadServerConfig` 明确注释“Missing file is not an error”，`logger` 同样对 `logcfg.ini` 做存在性检查后回退。
- **布尔值解析**：接受 `1/true/yes/on` 与 `0/false/no/off`（不区分大小写），其余回退默认值。
- **服务端点开关**：`upload.dir` 为空即禁用上传端点但不影响其它端点；`paramstore.backend` 为空或 `none` 即禁用 `/api/params` 端点。
- **数值防御性归一化**：端口范围 `(0, 65535]`，`default_format` 非 `xml` 即回退 `json`，`max_concurrent_jobs`/`max_jobs` ≥ 1，`upload_ttl_seconds` ∈ [0, 2592000]（30 天），`upload_max_file_mb` ∈ [1, 4096]，`upload_sweep_interval_seconds` ∈ [5, 3600]，`upload_evict_mode` 仅接受 `ttl/count/both/manual`。
- **上传池下限**：`upload_max_files` 不低于 `HSBA_MODEL_POOL_SIZE`（CGAL/OCCT 构建时为 10，否则为 1），以保证已上传模型可回填内存池。
- **认证触发条件**：非环回绑定（非 `127.0.0.1`/`::1`）时强制要求 token；`auth.enabled=auto`（或留空）时，只要 `auth.token` 非空即自动启用。
- **TLS 强制启动**：`require_tls=true` 且未配置证书/密钥对时拒绝启动。
- **ParamStore 错误语义**：任何失败都记录 `last_error_` 并重新抛出异常，调用方不得静默忽略。
- **加密密钥一致性**：现有密文行只能用相同 `crypto_password` 解密，密钥变更会导致旧数据不可读。
- **日志配置路径**：始终相对于 `std::filesystem::current_path()` 拼接 `logcfg.ini`，而非程序目录或安装目录。
- **CLI 与 env 覆盖顺序**：main.cpp 中命令行参数在 `ApplyEnvFallbacks` 之后再次赋值，因此 CLI > env > ini 是代码保证的顺序。

## 5. 依赖关系

```
restapi/main.cpp
    → restapi/server_config.cpp
        → fileoperator/rw_ptree.hpp (Config::from_ini)
        → base/string_helper.hpp
    → restapi/restapi.ini (静态默认)

logger/logger.cpp
    → fileoperator/rw_ptree.hpp (Config::from_ini)
    → logcfg.ini (静态默认)

utils/app_config.cpp
    → 无外部配置源（进程内状态）

fileoperator/param_store.cpp
    → sql_adapter.* (SQLite/MySQL/PostgreSQL)
    → param_schema.hpp / param_reflect.hpp / param_convert.hpp
    → cipher/* (AES-256-CBC at-rest encryption)
```

## 6. 未观察到的内容

- 未发现 `.env` 文件或统一的 `dotenv` 加载器；环境变量仅在 `restapi/main.cpp` 中硬编码三个变量名。
- 未发现 TOML 格式的配置文件；TOML 相关测试目录存在但未在运行期配置路径中被引用。
- 未发现全局 feature flag 系统；功能开关以 INI 字段（如 `upload_enabled`、`paramstore_enabled`、`auth_enabled`）表达。
- 未发现集中式的配置热重载机制；所有配置在进程启动时一次性加载。