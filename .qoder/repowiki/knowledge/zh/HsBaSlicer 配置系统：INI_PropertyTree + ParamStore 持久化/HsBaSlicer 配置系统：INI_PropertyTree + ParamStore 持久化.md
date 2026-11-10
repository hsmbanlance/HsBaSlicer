---
kind: configuration_system
name: HsBaSlicer 配置系统：INI/PropertyTree + ParamStore 持久化
category: configuration_system
scope:
    - '**'
source_files:
    - restapi/restapi.ini
    - restapi/server_config.hpp
    - restapi/server_config.cpp
    - fileoperator/rw_ptree.hpp
    - utils/app_config.hpp
    - utils/app_config.cpp
    - fileoperator/param_store.hpp
    - restapi/config_bridge.hpp
    - restapi/main.cpp
---

## 1. 使用的系统与工具

- **INI 配置文件**：基于 `boost::property_tree`，通过 `fileoperator/rw_ptree.hpp` 提供的 `Config::from_ini / to_ini / from_json / from_xml` 等封装统一读写。
- **进程内运行时配置**：`utils/app_config.cpp` 中的 `AppConfigSingletone`（单例，`std::shared_mutex` 保护），当前仅暴露外部工具路径（如 7-Zip）。
- **REST API 服务配置**：`restapi/server_config.{hpp,cpp}` 定义 `ServerConfig` 结构体，由 `LoadServerConfig(path)` 从 `restapi.ini` 加载，缺失文件时返回内置默认值。
- **参数持久化存储**：`fileoperator/param_store.{hpp,cpp}` 的 `ParamStore` 类，将切片流水线参数序列化到 SQLite / MySQL / PostgreSQL，支持可选 AES-256-CBC 静态加密（`SetCryptoKey`）。
- **C ABI 流水线配置编解码桥**：`restapi/config_bridge.{hpp,cpp}` 通过反射（`Utils::AnyObject` + `fileoperator/param_reflect.hpp`）在 JSON/XML 与 C 结构体之间转换。
- **环境变量注入**：`restapi.ini` 注释说明 `[auth] token` 可从 `HSBA_REST_AUTH_TOKEN` 环境变量读取（当 ini 中未显式配置时）。

## 2. 关键文件与包

| 路径 | 职责 |
|---|---|
| `restapi/restapi.ini` | REST 服务器默认 INI 配置（server / upload / paramstore / auth / rate_limit 段） |
| `restapi/server_config.hpp/.cpp` | `ServerConfig` 结构与 `LoadServerConfig()` 解析器 |
| `fileoperator/rw_ptree.hpp` | 对 `boost::property_tree` 的 INI/JSON/XML 读写封装 + `VariantConfigMap` / `AnyConfigMap` |
| `utils/app_config.hpp/.cpp` | 进程级单例应用配置 |
| `fileoperator/param_store.hpp/.cpp` | 参数持久化（SQLite/MySQL/PostgreSQL + 可选加密） |
| `restapi/config_bridge.hpp/.cpp` | C ABI 流水线配置的 JSON/XML 序列化桥 |
| `proto/*.proto` | 各工艺 PipelineConfig 的结构定义（被反射系统使用） |
| `convert/Msg2PipelineConfig.cpp` | proto ↔ 内存结构体映射（参与配置流转） |

## 3. 架构与约定

### 3.1 配置来源分层

1. **编译期默认值**：每个配置结构体的成员自带默认值（如 `host = "127.0.0.1"`、`port = 8080`、`default_format = "json"`）。这是第一层。
2. **INI 文件覆盖**：`LoadServerConfig` 用 `pt.get<T>("section.key", cfg.field)` 逐个字段覆盖默认值。缺失文件不报错，直接返回默认配置；格式错误抛出 `boost::property_tree::file_parser_error`。
3. **环境变量注入**：`[auth] token` 在 ini 为空时回退到 `HSBA_REST_AUTH_TOKEN` 环境变量（见 `restapi.ini` 注释与 `auth_gate` 测试用例）。
4. **运行时可写配置**：`ParamStore` 提供 Save/Load/Update/Delete 接口，把切片参数以宽表形式持久化到后端数据库，并可通过 `SetCryptoKey` 开启列级静态加密。

### 3.2 配置加载流程（REST API）

```text
main.cpp --config <path>
    → LoadServerConfig(path)
        → boost::property_tree::ptree (via Config::from_ini)
        → 逐字段填充 ServerConfig
        → 防御性归一化（端口范围、format 白名单、evict_mode 白名单、数值 clamp）
        → 返回不可变配置对象供后续组件消费
```

### 3.3 配置校验与归一化策略

`LoadServerConfig` 在解析后执行集中式归一化：
- `port` 必须 ∈ [1, 65535]，否则回退到 8080。
- `default_format` 非 `xml` 一律归为 `json`。
- `max_concurrent_jobs`、`max_jobs`、`result_ttl_seconds` 下界为 1/0。
- `upload_evict_mode` 仅接受 `ttl | count | both | manual`，非法值回退 `both`。
- `upload_max_files` 不低于 `HSBA_MODEL_POOL_SIZE`（构建期宏，CGAL/OCCT=10，否则=1），保证上传池可回填模型池。
- `upload_ttl_seconds` 钳制到 [0, 2592000]（30 天），`upload_sweep_interval_seconds` 钳制到 [5, 3600]。

### 3.4 安全相关约定

- `[auth] enabled` 支持 `auto | true | false`；当值为空且设置了 token 时自动启用认证。
- 仅在绑定到非 loopback 地址时才强制要求 token（`127.0.0.1` / `::1` 跳过认证）。
- TLS 证书/密钥成对出现；`require_tls = true` 时缺少任一则拒绝启动。
- `ParamStore` 的 at-rest 加密：设置 `crypto_password` 后，Save 将反射后的列映射经 AES-256-CBC 加密存入 `param_blob`，`param_key`、`schema_version`、`created_at`、`updated_at` 保持明文以便查询；不同 key 的数据互不可读。

### 3.5 配置与流水线的桥接

`config_bridge` 通过 `param_reflect.hpp` 注册的 `TypeInfo` 遍历 C ABI 结构体字段，借助 `param_convert` 的 `FieldToAny` / `AnyToField` 完成类型收敛，再交给 `struct_json` / `struct_xml` 输出 JSON/XML。这使得 REST API 能透明地处理所有 `HsBa*PipelineConfig_t` 结构体，无需手写序列化代码。

## 4. 约定与约束

- **INI 键命名**：采用 `section.key` 点号路径（如 `server.host`、`upload.max_files`、`paramstore.backend`），由 `boost::property_tree` 解析。
- **缺失 INI 文件不是错误**：`LoadServerConfig` 明确声明“missing file is not an error”，返回内置默认值（头文档契约）。
- **布尔值解析**：`ParseBool` 接受 `1/true/yes/on` 和 `0/false/no/off`，其余回退默认值。
- **字符串值统一 trim + to_lower**：所有字符串字段在写入前调用 `trim`，枚举/模式字段再 `to_lower`。
- **ParamStore 错误传播**：`ParamStore` 不吞异常——`last_error_` 记录消息的同时重新抛出，调用方（含 Lua）自行决定处理方式（`param_store.hpp` 注释明确约定）。
- **上传池大小下限**：`upload_max_files` 始终不低于 `HSBA_MODEL_POOL_SIZE`，这是由 `server_config.cpp` 中 `#ifndef HSBA_MODEL_POOL_SIZE` 宏守卫与 `std::max` 强制保证的。
- **环境变量优先级**：`HSBA_REST_AUTH_TOKEN` 仅在 ini 中 `[auth] token` 为空时作为后备（见 `restapi.ini` 注释）。
- **配置扩展方式**：新增配置项需在 `ServerConfig` 结构体中添加成员并在 `LoadServerConfig` 中增加对应 `pt.get` 行；新增流水线配置需注册反射元数据（`param_reflect.hpp`）并通过 `config_bridge` 自动获得 JSON/XML 支持。