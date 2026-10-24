# HsBaSlicer REST API 服务进程

`HsBaSlicerRestServer` 是一个**可选**可执行进程，通过 HTTP 暴露 `DllHsBaSlicer`
的 C ABI，响应同时支持 **JSON** 与 **XML**。仅当 CMake 选项 `HSBA_BUILD_REST_API`
为 `ON` 时才构建，且**仅限桌面平台**（Windows / Linux / macOS），其他平台强制 `OFF`。

## 启用构建

HTTP 服务基于 [cpp-httplib](https://github.com/yhirose/cpp-httplib)，该依赖通过
vcpkg 的 **`rest-api`** manifest feature 安装（不属于 `default-features`）。CMake
选项与 vcpkg feature 必须在同一次配置会话中一起启用：

```sh
cmake --preset <现有preset> \
      -DHSBA_BUILD_REST_API=ON \
      -DVCPKG_MANIFEST_FEATURES=rest-api
cmake --build --preset <现有preset>
```

若只设 `HSBA_BUILD_REST_API=ON` 而未启用 `rest-api`，配置会以 `FATAL_ERROR` 失败并
提示启用（检测逻辑与既有的 `HSBA_COPL` 模式对称）。

只有 HTTP 传输层（`routes.cpp`）依赖 cpp-httplib。非 HTTP 单元（`content_negotiate`、
`config_bridge`、`pipeline_executor`、`job_manager`、`server_config`）不依赖 HTTP，
由 `tests/RestApi/` 下的独立单元测试覆盖。

## 配置（`restapi.ini`）

服务从工作目录读取 `restapi.ini`（与 `logcfg.ini` 约定一致）；构建时会在可执行文件
旁放置一份副本。可用 `--config <file>` 覆盖路径，命令行 `--host` / `--port` 覆盖 ini
值。文件缺失不算错误——采用内置默认值。

| 段 | 键 | 默认 | 含义 |
|---|---|---|---|
| `[server]` | `host` | `127.0.0.1` | 绑定地址（`0.0.0.0` 暴露到局域网）。 |
| `[server]` | `port` | `8080` | TCP 端口。 |
| `[server]` | `default_format` | `json` | 既无 `?format=` 又无 `Accept` 时的响应格式。 |
| `[server]` | `max_concurrent_jobs` | `2` | 并发执行切片作业的 worker 线程数。 |
| `[server]` | `max_jobs` | `128` | 跟踪作业上限，超出后新提交返回 429。 |
| `[server]` | `result_ttl_seconds` | `3600` | 终态作业结果保留时长（秒）。 |
| `[paramstore]` | `backend` | `sqlite` | `sqlite`/`mysql`/`postgresql`；留空则 `/api/params` 返回 503。 |
| `[paramstore]` | `sqlite_path`/`host`/`user`/`password`/`database`/`port`/`crypto_password` | | ParamStore 连接（按请求解析）。 |
| `[auth]` | — | — | 预留，本期不读取。 |

## 格式协商

- **响应**格式优先级：`?format=json|xml` → `Accept` 头 → `default_format`。
- **请求体**格式由 `Content-Type` 决定（`application/xml`/`text/xml` → XML，其余 → JSON）。

所有机读响应均由项目 `Utils` 序列化器（`write_pretty_json`/`write_xml`）生成，**禁止手工
拼接** JSON/XML。含 `const char*` 成员、序列化器不直接支持的 config 结构体，经
`param_reflect` 反射逐字段桥接（`config_bridge`）。

## 端点

| 方法 | 路径 | 用途 | 底层调用 |
|---|---|---|---|
| `GET` | `/api/health` | 存活探针（状态 + 运行时长）。 | 进程内 |
| `GET` | `/api/version` | 库版本。 | `HsBaGetVersionJson/Xml` |
| `GET` | `/api/pipelines` | 支持的 pipeline 种类。 | 分发表 |
| `GET` | `/api/pipelines/{kind}/default-config` | 某 kind 的默认配置。 | `HsBaCreateDefault*Config` |
| `POST` | `/api/config/convert?kind={kind}` | proto 字节 → JSON/XML；追加 `&to=proto` 则把 JSON/XML 请求体编码为 proto 字节。 | `HsBa*ConfigFromProtoBytes/ToProtoBytes` |
| `POST` | `/api/jobs?kind={kind}` 或 `/api/jobs/{kind}` | 提交切片作业。 | `HsBaRun*Pipeline`（异步） |
| `GET` | `/api/jobs?state=&offset=&limit=` | 作业列表 / 分页（可选状态过滤）。 | 作业队列 |
| `GET` | `/api/jobs/{id}` | 作业状态（进度/结果元数据）。 | 作业队列 |
| `GET` | `/api/jobs/{id}/result` | 下载产物（gcode 文本 / 输出文件）。 | 作业队列 |
| `DELETE` | `/api/jobs/{id}` | 删除排队中或已终态的作业（运行中拒绝）。 | 作业队列 |
| `PUT` | `/api/params/{kind}/{key}` | upsert 配置到 ParamStore。 | `HsBaSavePipelineParams` |
| `GET` | `/api/params/{kind}/{key}` | 从 ParamStore 读取配置。 | `HsBaLoadPipelineParams` |

`{kind}` 取值：`fdm`、`sla`、`sls`、`slm`、`lom`、`tdp`、`waam`、`custom`、`filetransfer`。

同时提供服务字面量（实施计划）别名：`GET /api/kinds`（≡ `/api/pipelines`）、
`GET /api/config/default/{kind}`（≡ `/api/pipelines/{kind}/default-config`）、
`POST /api/params/{kind}?key=`（≡ `PUT /api/params/{kind}/{key}`）。

## 切片流程

1. `GET /api/pipelines/{kind}/default-config` 获取模板。
2. 修改字段后 `POST /api/jobs/{kind}` 提交配置体 → `202` 且 `Location: /api/jobs/{id}`。
3. 轮询 `GET /api/jobs/{id}` 直到 `state` 为 `success` 或 `failed`。
4. 成功后取 `GET /api/jobs/{id}/result`。

## 错误

失败响应统一为 `{code, message, detail}` JSON / `<Error>` XML：`400` kind/body/config 转换非法、
`404` 作业或 key 不存在、`409` 作业未完成（取结果）或仍在运行（删除）、`429` 队列已满、`503` 未配置
ParamStore。`DELETE /api/jobs/{id}` 成功返回 `204`。

## 安全说明

本期不实现鉴权（本机/内网信任模型）。**切勿**在不可信网络上绑定公网地址。
