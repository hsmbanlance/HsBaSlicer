---
kind: configuration_system
name: HsBaSlicer 配置系统：INI + Env + ParamStore 三层运行时配置
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - restapi/restapi.ini
    - restapi/main.cpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - utils/logcfg.ini
    - fileoperator/param_store.hpp
    - LibHsBaSlicer/ParamStore/config_json_ops.hpp
    - tests/AppConfig/app_config_test.cpp
    - tests/RestApi/auth_gate_test.cpp
---

## 1. 采用的方案

仓库没有统一的“应用配置框架”，而是按进程边界拆成两套独立的配置加载机制，外加一个可插拔的持久化参数存储：

- **REST API 服务进程**：基于 `boost::property_tree` 解析 INI 文件（`restapi/restapi.ini`），并通过环境变量注入敏感值；核心类型是 `ServerConfig`（`restapi/server_config.hpp`）。
- **通用应用层**：通过 `utils/app_config.hpp` 中的 `AppConfigSingletone` 单例暴露全局设置（目前只有 7-Zip 路径）。
- **切片流水线参数持久化**：`fileoperator/ParamStore` 提供 SQLite / MySQL / PostgreSQL 后端，支持 AES-256-CBC 静态数据加密，作为运行时的“参数库”。

日志级别等少量配置由 `logger/logger.cpp` 直接读取 `logcfg.ini`，属于独立的最小化子系统。

## 2. 关键文件

| 文件 | 职责 |
|---|---|
| `restapi/server_config.hpp/.cpp` | `ServerConfig` 结构体定义与 `LoadServerConfig(path)` 实现 |
| `restapi/restapi.ini` | REST 服务的默认 INI 配置（含 `[server]`、`[auth]`、`[upload]`、`[paramstore]`、`[rate_limit]` 段） |
| `restapi/main.cpp` | CLI 参数解析、`--config`、环境变量覆盖（`HSBA_REST_AUTH_TOKEN`、`HSBA_REST_TLS_CERT`、`HSBA_REST_TLS_KEY`） |
| `utils/app_config.hpp/.cpp` | 线程安全的 `AppConfigSingletone` 单例 |
| `utils/logcfg.ini` | logger 子系统的日志级别与格式配置 |
| `fileoperator/param_store.hpp` | `ParamStore` 主类（多后端、事务、迁移钩子、AES-256-CBC 加密） |
| `LibHsBaSlicer/ParamStore/config_json_ops.hpp` | 面向 C ABI 的 PipelineConfig JSON 编解码（反射驱动） |
| `tests/AppConfig/app_config_test.cpp` | 单例并发安全测试 |
| `tests/RestApi/auth_gate_test.cpp` | `LoadServerConfig` 行为测试（缺失文件、空 section、默认值） |

## 3. 架构与约定

### 3.1 REST 服务器配置（`ServerConfig`）

- 所有成员自带内置默认值（如 `host = "127.0.0.1"`、`port = 8080`、`default_format = "json"`），因此 `restapi.ini` 缺失时仍可直接启动。
- `LoadServerConfig(path)` 使用 `Config::from_ini`（封装 `boost::property_tree`）逐键读取，未出现的键保留默认值。
- 读取后执行防御性归一化：端口范围校验、`default_format` 非 xml 即 json、`max_concurrent_jobs`/`max_jobs` ≥ 1、`upload_ttl_seconds` 钳制到 0..30 天、`upload_evict_mode` 仅接受 `ttl|count|both|manual` 否则回退为 `both`。
- `[auth]` 段的 `enabled` 支持 `auto|true|false`；当为空字符串时自动启用（只要 `token` 非空）。`token` 为空且显式开启 auth 会报错。
- `[upload]` 的 `dir` 为空即禁用上传端点，其他端点不受影响；`max_files` 被强制提升到 `HSBA_MODEL_POOL_SIZE`（构建期宏，CGAL/OCCT 内核下为 10，否则为 1）。
- `[paramstore]` 支持 `sqlite|mysql|postgresql` 后端，连接串字段在每次请求时解析；`crypto_password` 用于 ParamStore 列级 AES-256-CBC 静态加密。

### 3.2 环境变量覆盖（仅 REST 进程）

`restapi/main.cpp` 在解析 INI 之后调用 `fill_from_env`，允许以下环境变量覆盖对应字段：

- `HSBA_REST_AUTH_TOKEN`
- `HSBA_REST_TLS_CERT`
- `HSBA_REST_TLS_KEY`

设计意图是容器镜像可以保持 INI 无 token，运行时通过环境变量注入密钥。

### 3.3 全局应用配置（`AppConfigSingletone`）

- 命名空间 `HsBa::Slicer`，通过 `GetInstance()` / `DeleteInstance()` 访问。
- 内部用 `std::shared_mutex` 实现双重检查锁的单例构造，读操作走 `std::shared_lock`，写操作走 `std::unique_lock`。
- 当前只暴露 `GetSevenZPath()` 一个配置项，其余扩展字段预留。
- 单元测试验证了：多次 `GetInstance` 返回同一地址、`DeleteInstance` 后可重建、8 线程并发竞争下仍只有一个实例。

### 3.4 参数持久化（ParamStore）

- 通过 `ISQLAdapter` 抽象 SQLite / MySQL / PostgreSQL。
- 每个 `PipelineConfig` 类型映射一张宽表；CRUD 通过 `AnyObject` + `param_reflect` 反射遍历字段完成。
- `Save` 走 Validate → Reflect → Coerce → Upsert 阶段流程；批量 API 包裹在单个事务中。
- `SetCryptoKey(key)` 启用列级 AES-256-CBC 加密：保存时将反射后的列映射序列化为二进制并加密存入 `param_blob`，明文列留空；`param_key`、`schema_version`、`created_at`、`updated_at` 始终明文以便查询。
- 错误不吞掉：`last_error_` 记录最近失败信息，异常继续抛出给调用方（包括 Lua）。

### 3.5 配置文件查找顺序

- `restapi.ini`：默认工作目录下同名文件；可通过 `--config <file>` 命令行参数指定。
- `logcfg.ini`：logger 子模块在当前目录拼接 `/logcfg.ini`。
- 两者均遵循“文件不存在 = 使用内置默认值”的策略，而非启动失败。

## 4. 约定与约束

- **INI 文件缺失不是错误**：`LoadServerConfig` 对 `std::filesystem::exists` 检查后直接返回默认 `ServerConfig`；logger 同样如此。这是代码明确保证的行为（见 `server_config.cpp:61-66` 与 `logger.cpp:148`）。
- **REST 服务绑定非 loopback 时必须鉴权**：`restapi.ini` 注释写明 `127.0.0.1 / ::1` 跳过 token 校验，非 loopback 绑定则要求 `[auth]` 配置 token；若 `require_tls=true` 但未同时配置 `tls_cert`/`tls_key`，服务拒绝启动。
- **上传池容量不低于模型池容量**：`LoadServerConfig` 将 `upload_max_files` 强制设为 `max(HSBA_MODEL_POOL_SIZE, 1)`，确保已保留的上传副本足以回填内存模型池（见 `server_config.cpp:131`）。
- **ParamStore 加密键一致性**：`config_json_ops.hpp` 注释说明，已加密行的解密必须使用相同 key，否则不可读；明文行与加密行共存时查询需兼容两种格式。
- **PipelineConfig 的 JSON 编解码统一走反射**：`config_json_ops.hpp` 明确 Dll 层只调用这些自由函数，不直接依赖 `fileoperator`，以保证 C ABI 与 GUI 消费者共享同一份字段元数据。
- **日志配置独立于应用配置**：`logcfg.ini` 由 logger 模块自行解析，不被 `AppConfigSingletone` 或 `ServerConfig` 管理。

## 5. 不适用场景说明

本仓库没有 `.env`、`.properties`、YAML/TOML 形式的“应用级配置中心”；配置集中在 INI 与环境变量两个层面，并由 ParamStore 承担“流水线参数的持久化存储”这一更细粒度的运行时状态管理。