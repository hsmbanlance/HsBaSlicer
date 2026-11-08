# REST API 服务进程

<cite>
**本文引用的文件**   
- [restapi/CMakeLists.txt](file://restapi/CMakeLists.txt)
- [restapi/main.cpp](file://restapi/main.cpp)
- [restapi/routes.cpp](file://restapi/routes.cpp)
- [restapi/job_manager.hpp](file://restapi/job_manager.hpp)
- [restapi/job_manager.cpp](file://restapi/job_manager.cpp)
- [restapi/pipeline_executor.hpp](file://restapi/pipeline_executor.hpp)
- [restapi/pipeline_executor.cpp](file://restapi/pipeline_executor.cpp)
- [restapi/config_bridge.hpp](file://restapi/config_bridge.hpp)
- [restapi/config_bridge.cpp](file://restapi/config_bridge.cpp)
- [restapi/server_config.hpp](file://restapi/server_config.hpp)
- [restapi/server_config.cpp](file://restapi/server_config.cpp)
- [restapi/content_negotiate.hpp](file://restapi/content_negotiate.hpp)
- [restapi/content_negotiate.cpp](file://restapi/content_negotiate.cpp)
- [restapi/auth_gate.hpp](file://restapi/auth_gate.hpp)
- [restapi/auth_gate.cpp](file://restapi/auth_gate.cpp)
- [restapi/upload_bridge.hpp](file://restapi/upload_bridge.hpp)
- [restapi/upload_bridge.cpp](file://restapi/upload_bridge.cpp)
- [restapi/upload_store.hpp](file://restapi/upload_store.hpp)
- [restapi/upload_store.cpp](file://restapi/upload_store.cpp)
- [restapi/restapi.ini](file://restapi/restapi.ini)
- [tests/RestApi/job_manager_test.cpp](file://tests/RestApi/job_manager_test.cpp)
- [tests/RestApi/config_roundtrip_test.cpp](file://tests/RestApi/config_roundtrip_test.cpp)
- [tests/RestApi/auth_gate_test.cpp](file://tests/RestApi/auth_gate_test.cpp)
- [tests/RestApi/endpoint_http_test.cpp](file://tests/RestApi/endpoint_http_test.cpp)
</cite>

## 更新摘要
**变更内容**   
- 新增认证门控机制，支持 Bearer Token 和 X-API-Key 两种认证方式
- 实现多部分文件上传处理系统，包含模型文件安全处理和上传池管理
- 完善作业管理系统，增加协作式取消支持和完整的作业生命周期控制
- 集成 HTTPS 支持，通过 OpenSSL 提供 TLS 加密通信
- 新增几何导出端点，支持螺旋路径和逐层几何数据访问
- 扩展配置桥接机制，支持新的安全参数和上传池配置项

## 目录
1. [项目概述与快速开始](#项目概述与快速开始)
2. [架构总览](#架构总览)
3. [核心概念](#核心概念)
4. [核心模块详解](#核心模块详解)
5. [高级主题](#高级主题)
6. [API 参考文档](#api-参考文档)
7. [集成指南](#集成指南)
8. [开发测试与贡献](#开发测试与贡献)
9. [扩展开发指南](#扩展开发指南)
10. [示例与教程](#示例与教程)
11. [部署故障排除与FAQ](#部署故障排除与faq)
12. [附录](#附录)

## 项目概述与快速开始

`HsBaSlicerRestServer` 是一个可选编译的 RESTful API 服务进程，基于 `cpp-httplib`，对外暴露健康检查、版本信息、流水线元数据、配置转换、异步切片作业队列以及参数持久化接口。它不直接实现切片算法，而是通过 `DllHsBaSlicer` 的 C ABI 调用具体流水线（FDM、SLA、SLS、SLM、LOM、TDP、WAAM、CUSTOM、FILETRANSFER），并以 JSON 或 XML 双格式返回结果。

**更新** 新增了认证门控、HTTPS 支持、多部分文件上传和协作式作业取消等安全与管理功能。

### 构建与启用条件

- 顶层开关：`HSBA_BUILD_REST_API=ON`。
- vcpkg feature：需要启用 `rest-api`，使 `cpp-httplib` 进入依赖集；CMake 包名是 `httplib`。
- 平台限制：默认仅桌面平台启用；非桌面环境会强制关闭该选项。
- 产物名称：`HsBaSlicerRestServer`。
- 运行时配置文件：`restapi.ini`，默认位于可执行文件工作目录。

```mermaid
flowchart TD
Start["启动 HsBaSlicerRestServer"] --> ParseArgs["解析命令行参数<br/>--config / --host / --port / --help"]
ParseArgs --> LoadIni["加载 restapi.ini<br/>ServerConfig"]
LoadIni --> ApplyOverrides["应用命令行覆盖值"]
ApplyOverrides --> InitLib["初始化 DllHsBaSlicer"]
InitLib --> CreateServer["创建 httplib::Server"]
CreateServer --> RegisterJobs["创建 JobManager<br/>workers / max_jobs / ttl"]
RegisterJobs --> RegisterRoutes["注册 HTTP 路由"]
RegisterRoutes --> Listen["监听 host:port"]
Listen --> Running["服务运行中"]
```

**图示来源**
- [restapi/main.cpp:27-82](file://restapi/main.cpp#L27-L82)
- [restapi/routes.cpp:505-529](file://restapi/routes.cpp#L505-L529)
- [restapi/server_config.cpp:34-79](file://restapi/server_config.cpp#L34-L79)

**章节来源**
- [restapi/CMakeLists.txt:1-66](file://restapi/CMakeLists.txt#L1-L66)
- [restapi/main.cpp:1-84](file://restapi/main.cpp#L1-L84)
- [restapi/restapi.ini:1-50](file://restapi/restapi.ini#L1-L50)

## 架构总览

REST 服务采用分层设计：HTTP 路由层负责请求解析、格式协商和错误封装；认证门控层提供统一的安全验证；作业管理层维护线程池、作业状态和结果 TTL；执行层将流水线 kind 分派到 `DllHsBaSlicer` 的 C ABI；配置桥接层利用反射表把扁平 C 结构体转换为 JSON/XML。

```mermaid
graph TB
Client["客户端"] --> Http["httplib 服务器<br/>routes.cpp"]
Http --> AuthGate["认证门控<br/>auth_gate.*"]
Http --> Negotiate["内容协商<br/>content_negotiate.*"]
Http --> ConfigBridge["配置桥接<br/>config_bridge.*"]
Http --> UploadBridge["上传桥接<br/>upload_bridge.*"]
Http --> JobManager["作业管理器<br/>job_manager.*"]
JobManager --> PipelineExec["流水线执行器<br/>pipeline_executor.*"]
PipelineExec --> Dll["DllHsBaSlicer C ABI<br/>各 *_pipeline.h"]
Http --> ServerCfg["服务器配置<br/>server_config.*"]
Http --> Ini["restapi.ini"]
```

**图示来源**
- [restapi/routes.cpp:441-492](file://restapi/routes.cpp#L441-L492)
- [restapi/job_manager.hpp:85-159](file://restapi/job_manager.hpp#L85-L159)
- [restapi/pipeline_executor.hpp:20-85](file://restapi/pipeline_executor.hpp#L20-L85)
- [restapi/config_bridge.hpp:26-51](file://restapi/config_bridge.hpp#L26-L51)
- [restapi/server_config.hpp:21-51](file://restapi/server_config.hpp#L21-L51)
- [restapi/auth_gate.hpp:20-73](file://restapi/auth_gate.hpp#L20-L73)

**章节来源**
- [restapi/routes.cpp:1-533](file://restapi/routes.cpp#L1-L533)
- [restapi/job_manager.hpp:1-164](file://restapi/job_manager.hpp#L1-L164)
- [restapi/pipeline_executor.hpp:1-90](file://restapi/pipeline_executor.hpp#L1-L90)
- [restapi/config_bridge.hpp:1-56](file://restapi/config_bridge.hpp#L1-L56)
- [restapi/server_config.hpp:1-56](file://restapi/server_config.hpp#L1-L56)

## 核心概念

### 认证与安全机制

**新增** 服务现在支持两种认证方式：`Authorization: Bearer <token>` 和 `X-API-Key: <token>`。认证在路由前执行，对除 `/` 和 `/api/health` 外的所有端点进行保护。

| 认证方式 | 请求头 | 说明 |
|---|---|---|
| Bearer Token | `Authorization: Bearer <token>` | 标准 OAuth2 风格令牌 |
| API Key | `X-API-Key: <token>` | 简单 API 密钥 |
| 本地回环 | 无需认证 | 绑定到 localhost/127.0.0.1 时跳过认证 |

```mermaid
flowchart TD
Request["收到请求"] --> CheckPublic{"是否公共路径"}
CheckPublic --> |是| Allow["允许访问"]
CheckPublic --> |否| CheckAuth{"是否需要认证"}
CheckAuth --> |否| Allow
CheckAuth --> |是| ExtractToken["提取 Authorization 或 X-API-Key"]
ExtractToken --> Validate["验证令牌匹配"]
Validate --> |匹配| Allow
Validate --> |不匹配| Deny["返回 401 未授权"]
```

**图示来源**
- [restapi/auth_gate.cpp:115-133](file://restapi/auth_gate.cpp#L115-L133)
- [restapi/auth_gate.cpp:73-89](file://restapi/auth_gate.cpp#L73-L89)

**章节来源**
- [restapi/auth_gate.hpp:20-73](file://restapi/auth_gate.hpp#L20-L73)
- [restapi/auth_gate.cpp:1-160](file://restapi/auth_gate.cpp#L1-L160)

### HTTPS 支持

**新增** 服务支持通过 OpenSSL 启用 HTTPS。需要在配置文件中设置 `tls_cert` 和 `tls_key` 字段，两者必须同时配置。

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `tls_cert` | 字符串 | 空 | PEM 证书文件路径 |
| `tls_key` | 字符串 | 空 | PEM 私钥文件路径 |
| `require_tls` | 布尔值 | false | 是否要求必须配置 TLS |

**章节来源**
- [restapi/server_config.hpp:30-32](file://restapi/server_config.hpp#L30-L32)
- [restapi/server_config.cpp:77-78](file://restapi/server_config.cpp#L77-L78)
- [restapi/routes.cpp:1130-1157](file://restapi/routes.cpp#L1130-L1157)

### 数据格式协商

响应格式优先级如下：

| 优先级 | 来源 | 行为 |
|---|---|---|
| 最高 | `?format=` 查询参数 | 显式指定 `json` 或 `xml` |
| 次高 | `Accept` 请求头 | 包含 `xml` 且未更强偏好 `json` 时选择 XML |
| 最低 | `server.default_format` | 默认 `json`，非法值归一化为 `json` |

请求体格式由 `Content-Type` 决定：包含 `xml` 时按 XML 解析，否则按 JSON 解析。

```mermaid
flowchart TD
Start["收到请求"] --> CheckQuery{"是否提供 ?format="}
CheckQuery --> |是| UseQuery["使用查询参数格式"]
CheckQuery --> |否| CheckAccept{"Accept 是否包含 xml"}
CheckAccept --> |是| UseXml["选择 XML"]
CheckAccept --> |否| CheckJson{"Accept 是否包含 json"}
CheckJson --> |是| UseJson["选择 JSON"]
CheckJson --> |否| UseDefault["使用 server.default_format"]
UseQuery --> End["确定 DataFormat"]
UseXml --> End
UseJson --> End
UseDefault --> End
```

**图示来源**
- [restapi/content_negotiate.cpp:24-56](file://restapi/content_negotiate.cpp#L24-L56)
- [restapi/content_negotiate.hpp:21-41](file://restapi/content_negotiate.hpp#L21-L41)

**章节来源**
- [restapi/content_negotiate.hpp:1-55](file://restapi/content_negotiate.hpp#L1-L55)
- [restapi/content_negotiate.cpp:1-65](file://restapi/content_negotiate.cpp#L1-L65)

### 作业生命周期

作业状态包括排队、运行、成功、失败、取消。作业 ID 形如 `job-<序号>`，删除操作对运行中的作业拒绝，对已完成的作业受 TTL 控制。

**更新** 新增了协作式取消机制，支持在所有九个流水线类型中进行作业取消。

```mermaid
stateDiagram-v2
[*] --> 排队
排队 --> 运行 : "工作线程取出"
运行 --> 成功 : "流水线成功"
运行 --> 失败 : "流水线失败或抛出异常"
运行 --> 取消 : "协作式取消"
成功 --> 保留期 : "记录完成时间"
失败 --> 保留期 : "记录完成时间"
取消 --> 保留期 : "记录完成时间"
保留期 --> [*] : "超过 result_ttl_seconds"
```

**图示来源**
- [restapi/job_manager.hpp:27-61](file://restapi/job_manager.hpp#L27-L61)
- [restapi/job_manager.cpp:292-349](file://restapi/job_manager.cpp#L292-L349)

**章节来源**
- [restapi/job_manager.hpp:1-164](file://restapi/job_manager.hpp#L1-L164)
- [restapi/job_manager.cpp:1-352](file://restapi/job_manager.cpp#L1-L352)

### 多部分文件上传

**新增** 支持通过 multipart/form-data 提交模型文件。上传文件经过严格的安全清洗，只允许安全的字符集，并存储在独立的上传池中。

| 配置项 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `upload.dir` | 字符串 | 空 | 上传根目录（空则禁用） |
| `upload.max_files` | 整数 | 256 | 保留的文件数量上限 |
| `upload.ttl_seconds` | 长整型 | 604800 | 文件保留时间（秒） |
| `upload.evict_mode` | 字符串 | "both" | 驱逐模式：ttl/count/both/manual |
| `upload.max_file_mb` | 整数 | 256 | 单文件大小限制（MB） |
| `upload.max_request_mb` | 整数 | 260 | 整个请求大小限制（MB） |

**章节来源**
- [restapi/server_config.hpp:38-47](file://restapi/server_config.hpp#L38-L47)
- [restapi/upload_bridge.hpp:19-68](file://restapi/upload_bridge.hpp#L19-L68)
- [restapi/upload_store.cpp:41-60](file://restapi/upload_store.cpp#L41-L60)

## 核心模块详解

### 入口与服务器启动

`main.cpp` 负责解析命令行参数、加载 `restapi.ini`、应用 `--host` 与 `--port` 覆盖，然后调用 `RunServer`。`RunServer` 初始化底层库、创建 `JobManager`、注册路由、安装信号处理器并启动监听。

**更新** 新增了 HTTPS 服务器创建逻辑和认证门控注册。

```mermaid
sequenceDiagram
participant CLI as "命令行"
participant Main as "main.cpp"
participant Cfg as "server_config.cpp"
participant Routes as "routes.cpp"
participant Server as "httplib : : Server"
CLI->>Main : 传入 --config / --host / --port
Main->>Cfg : LoadServerConfig(path)
Cfg-->>Main : ServerConfig
Main->>Main : 应用 host/port 覆盖
Main->>Routes : RunServer(cfg)
Routes->>Server : listen(host, port)
Server-->>Routes : 监听结果
Routes-->>CLI : 退出码
```

**图示来源**
- [restapi/main.cpp:27-82](file://restapi/main.cpp#L27-L82)
- [restapi/routes.cpp:505-529](file://restapi/routes.cpp#L505-L529)
- [restapi/server_config.cpp:34-79](file://restapi/server_config.cpp#L34-L79)

**章节来源**
- [restapi/main.cpp:1-84](file://restapi/main.cpp#L1-L84)
- [restapi/routes.cpp:505-529](file://restapi/routes.cpp#L505-L529)
- [restapi/server_config.cpp:1-82](file://restapi/server_config.cpp#L1-L82)

### 路由与请求处理

`routes.cpp` 注册所有 `/api/*` 端点，统一通过 `TargetFormat` 决定响应格式，并通过 `ReplyValue`/`ReplyError` 封装结构化响应。关键职责包括：

- 健康检查与版本信息。
- 流水线种类列表及默认配置。
- 配置双向转换：JSON/XML ↔ proto。
- 异步作业提交、状态轮询、结果下载、列表与删除。
- 参数保存与加载。
- **新增** 认证门控注册。
- **新增** 多部分文件上传处理。
- **新增** 几何数据导出端点。

```mermaid
flowchart TD
Request["HTTP 请求"] --> Route["路由匹配"]
Route --> Health["/api/health"]
Route --> Version["/api/version"]
Route --> Pipelines["/api/pipelines / /api/kinds"]
Route --> DefaultConfig["/api/pipelines/{kind}/default-config"]
Route --> Convert["/api/config/convert"]
Route --> Submit["POST /api/jobs"]
Route --> Status["GET /api/jobs/{id}"]
Route --> Result["GET /api/jobs/{id}/result"]
Route --> Geometry["GET /api/jobs/{id}/geometry"]
Route --> Path["GET /api/jobs/{id}/geometry/path"]
Route --> ListJobs["GET /api/jobs"]
Route --> DeleteJob["DELETE /api/jobs/{id}"]
Route --> Cancel["POST /api/jobs/{id}/cancel"]
Route --> ParamSave["PUT /api/params/{kind}/{key}"]
Route --> ParamLoad["GET /api/params/{kind}/{key}"]
```

**图示来源**
- [restapi/routes.cpp:441-492](file://restapi/routes.cpp#L441-L492)

**章节来源**
- [restapi/routes.cpp:1-533](file://restapi/routes.cpp#L1-L533)

### 认证门控

**新增** 认证门控在路由前执行，对所有端点进行统一的安全验证。支持 Bearer Token 和 X-API-Key 两种认证方式，对本地回环地址自动跳过认证。

```mermaid
classDiagram
class AuthGate {
+IsLoopbackHost(boundHost) bool
+IsPublicPath(path) bool
+AuthRequired(cfg, boundHost) bool
+Authorize(cfg, path, boundHost, authHeader, apiKeyHeader) AuthOutcome
+TlsConfigured(cfg) bool
+ValidateSecurityConfig(cfg) string
}
class AuthOutcome {
<<enumeration>>
Allowed
MissingCredentials
InvalidCredentials
}
AuthGate --> AuthOutcome : "返回认证结果"
```

**图示来源**
- [restapi/auth_gate.hpp:20-73](file://restapi/auth_gate.hpp#L20-L73)
- [restapi/auth_gate.cpp:115-133](file://restapi/auth_gate.cpp#L115-L133)

**章节来源**
- [restapi/auth_gate.hpp:1-73](file://restapi/auth_gate.hpp#L1-L73)
- [restapi/auth_gate.cpp:1-160](file://restapi/auth_gate.cpp#L1-L160)

### 作业管理器

`JobManager` 是进程内异步作业队列，维护：

- 固定数量的工作线程。
- 作业映射表 `jobs_`。
- 就绪队列 `ready_`。
- 互斥锁与条件变量。
- 作业 TTL 回收。
- **新增** 协作式取消支持。

它支持注入自定义 runner，便于测试替换真实流水线。

**更新** 新增了完整的作业取消机制，支持在作业排队、运行和终止状态下进行不同的取消处理。

```mermaid
classDiagram
class JobManager {
-mtx_ mutex
-cv_ condition_variable
-jobs_ map<string, unique_ptr<JobEntry>>
-ready_ deque<string>
-workers_ vector<thread>
-stop_ bool
-next_id_ unsigned long long
-max_jobs_ int
-ttl_ seconds
-runner_ Runner
+Submit(kind, config, arena) string
+Status(id, out) bool
+Outcome(id, out) bool
+Exists(id) bool
+Remove(id) RemoveResult
+Cancel(id) CancelResult
+List(state_filter, offset, limit) vector<JobStatusView>
+SetProgress(id, percent, stage) void
-WorkerLoop() void
-Execute(job) void
-ReapLocked(now) void
}
class JobEntry {
+id string
+kind HsBaPipelineKind
+state JobState
+percent int
+stage string
+config void*
+arena StringArena
+outcome SliceOutcome
+error_message string
+finish steady_clock : : time_point
+cancel shared_ptr<atomic<bool>>
}
JobManager --> JobEntry : "管理"
```

**图示来源**
- [restapi/job_manager.hpp:85-159](file://restapi/job_manager.hpp#L85-L159)
- [restapi/job_manager.cpp:43-81](file://restapi/job_manager.cpp#L43-L81)
- [restapi/job_manager.cpp:83-104](file://restapi/job_manager.cpp#L83-L104)
- [restapi/job_manager.cpp:264-332](file://restapi/job_manager.cpp#L264-L332)

**章节来源**
- [restapi/job_manager.hpp:1-164](file://restapi/job_manager.hpp#L1-L164)
- [restapi/job_manager.cpp:1-352](file://restapi/job_manager.cpp#L1-L352)

### 流水线执行器

`pipeline_executor` 是 kind 分发层，为每个流水线 kind 生成统一的 create/destroy/run/from_proto/to_proto/free_strings 操作表，并调用 `DllHsBaSlicer` 对应头文件中的函数。它将不同流水线的结果统一成 `SliceOutcome`，屏蔽 gcode 内容、导出路径、输出路径、文件传输计数等差异。

```mermaid
classDiagram
class PipelineExecutor {
+IsValidKind(kind) bool
+KindName(kind) string_view
+ParseKind(name) HsBaPipelineKind
+ConfigTypeInfo(kind) TypeInfo*
+CreateDefaultConfig(kind) void*
+DestroyConfig(kind, config) void
+RunPipeline(kind, config, callback, user_data) SliceOutcome
+ConfigFromProto(kind, data, size, fmt) string
+ConfigToProto(kind, text, inFmt) vector<unsigned char>
}
class KindOps {
+name const char*
+typeinfo() TypeInfo*
+create() void*
+destroy(void*) void
+run(const void*, callback, user_data) SliceOutcome
+from_proto(data, size, cfg) int
+to_proto(cfg, out, size) int
+free_strings(cfg) void
}
class SliceOutcome {
+success int
+total_layers int
+output_path string
+inline_content string
+error_message string
+elapsed_seconds double
+files_transferred int
+total_files int
+cancelled int
+geometry_id long long
}
PipelineExecutor --> KindOps : "kind 表"
PipelineExecutor --> SliceOutcome : "统一结果"
```

**图示来源**
- [restapi/pipeline_executor.hpp:20-85](file://restapi/pipeline_executor.hpp#L20-L85)
- [restapi/pipeline_executor.cpp:50-93](file://restapi/pipeline_executor.cpp#L50-L93)
- [restapi/pipeline_executor.cpp:125-171](file://restapi/pipeline_executor.cpp#L125-L171)
- [restapi/pipeline_executor.cpp:224-287](file://restapi/pipeline_executor.cpp#L224-L287)

**章节来源**
- [restapi/pipeline_executor.hpp:1-90](file://restapi/pipeline_executor.hpp#L1-L90)
- [restapi/pipeline_executor.cpp:1-290](file://restapi/pipeline_executor.cpp#L1-L290)

### 服务器配置

`ServerConfig` 同时承载服务器运行时参数和 ParamStore 后端连接参数。缺失的 `restapi.ini` 不会报错，只使用内置默认值；存在但格式错误的 ini 会抛出解析异常。端口、默认格式、并发数、最大作业数和 TTL 都会进行防御性归一化。

**更新** 新增了认证、TLS 和上传池相关配置项。

```mermaid
flowchart TD
Start["LoadServerConfig(path)"] --> Exists{"文件是否存在"}
Exists --> |否| ReturnDefaults["返回默认 ServerConfig"]
Exists --> |是| ReadIni["读取 INI 树"]
ReadIni --> ParseServer["解析 [server] 段"]
ParseServer --> ParseAuth["解析 [auth] 段"]
ParseAuth --> ParseUpload["解析 [upload] 段"]
ParseUpload --> ParseParamstore["解析 [paramstore] 段"]
ParseParamstore --> Normalize["数值与格式归一化"]
Normalize --> ReturnCfg["返回 ServerConfig"]
```

**图示来源**
- [restapi/server_config.cpp:34-79](file://restapi/server_config.cpp#L34-L79)
- [restapi/server_config.hpp:21-51](file://restapi/server_config.hpp#L21-L51)

**章节来源**
- [restapi/server_config.hpp:1-56](file://restapi/server_config.hpp#L1-L56)
- [restapi/server_config.cpp:1-82](file://restapi/server_config.cpp#L1-L82)
- [restapi/restapi.ini:1-50](file://restapi/restapi.ini#L1-L50)

## 高级主题

### 异步作业提交与结果下载流程

```mermaid
sequenceDiagram
participant Client as "客户端"
participant Routes as "routes.cpp"
participant Jobs as "JobManager"
participant Exec as "pipeline_executor"
participant Dll as "DllHsBaSlicer"
Client->>Routes : POST /api/jobs {kind, config}
Routes->>Routes : 解析 kind 与请求体格式
Routes->>Exec : CreateDefaultConfig(kind)
Routes->>Exec : DeserializeConfig(...)
Routes->>Jobs : Submit(kind, config, arena)
alt 队列已满
Jobs-->>Routes : 空 id
Routes-->>Client : 429 job queue full
else 接受作业
Jobs-->>Routes : job-id
Routes-->>Client : 202 Job
Note over Jobs,Exec : 工作线程执行流水线
Client->>Routes : GET /api/jobs/{id}
Routes->>Jobs : Status(id)
Jobs-->>Routes : JobStatusView
Routes-->>Client : 200 Job
Client->>Routes : GET /api/jobs/{id}/result
Routes->>Jobs : Outcome(id)
alt 有 inline_content
Jobs-->>Routes : outcome.inline_content
Routes-->>Client : 200 text/plain
else 有 output_path
Jobs-->>Routes : outcome.output_path
Routes-->>Client : 200 application/octet-stream
else 无结果
Routes-->>Client : 404 no downloadable result
end
end
```

**图示来源**
- [restapi/routes.cpp:213-300](file://restapi/routes.cpp#L213-L300)
- [restapi/job_manager.cpp:83-104](file://restapi/job_manager.cpp#L83-L104)
- [restapi/job_manager.cpp:292-332](file://restapi/job_manager.cpp#L292-L332)

**章节来源**
- [restapi/routes.cpp:213-300](file://restapi/routes.cpp#L213-L300)
- [restapi/job_manager.cpp:83-104](file://restapi/job_manager.cpp#L83-L104)

### 配置双向转换

`/api/config/convert` 支持两种方向：

- 默认方向：proto 字节 → JSON/XML。
- `?to=proto`：JSON/XML → proto 字节。

```mermaid
flowchart TD
Start["POST /api/config/convert"] --> ParseKind["解析 kind"]
ParseKind --> CheckTo{"?to=proto ?"}
CheckTo --> |是| ToProto["JSON/XML → proto 字节"]
CheckTo --> |否| FromProto["proto 字节 → JSON/XML"]
ToProto --> Binary["application/x-protobuf"]
FromProto --> Text["JSON 或 XML"]
```

**图示来源**
- [restapi/routes.cpp:302-332](file://restapi/routes.cpp#L302-L332)
- [restapi/pipeline_executor.cpp:238-287](file://restapi/pipeline_executor.cpp#L238-L287)

**章节来源**
- [restapi/routes.cpp:302-332](file://restapi/routes.cpp#L302-L332)
- [restapi/pipeline_executor.cpp:238-287](file://restapi/pipeline_executor.cpp#L238-L287)

### 协作式作业取消

**新增** 作业取消采用协作式机制，通过原子标志位通知流水线在安全检查点停止执行。取消请求可以针对排队、运行中和已终止的作业进行不同的处理。

| 作业状态 | 取消行为 | 返回状态 |
|---|---|---|
| 排队中 | 直接从队列移除 | 202 Removed |
| 运行中 | 设置取消标志 | 202 Accepted |
| 已终止 | 拒绝取消请求 | 409 Already finished |
| 不存在 | 作业未找到 | 404 Not found |

**章节来源**
- [restapi/job_manager.cpp:327-359](file://restapi/job_manager.cpp#L327-L359)
- [restapi/routes.cpp:879-901](file://restapi/routes.cpp#L879-L901)

### 多部分文件上传处理

**新增** 多部分文件上传处理流程，包括文件安全清洗、路径渲染和配置注入。

```mermaid
flowchart TD
Multipart["multipart/form-data"] --> ParseParts["解析 parts"]
ParseParts --> CheckConfig{"是否有 config part"}
CheckConfig --> |否| ErrorConfig["返回 400 missing config part"]
CheckConfig --> |是| CheckModel{"是否有 model part"}
CheckModel --> |否| ErrorModel["返回 400 missing model part"]
CheckModel --> |是| Sanitize["SanitizeUploadName"]
Sanitize --> RenderPath["RenderUploadPath"]
RenderPath --> SaveFile["保存文件到上传池"]
SaveFile --> BindConfig["BindUploadedModel"]
BindConfig --> SubmitJob["提交作业"]
```

**图示来源**
- [restapi/routes.cpp:372-461](file://restapi/routes.cpp#L372-L461)
- [restapi/upload_bridge.cpp:34-60](file://restapi/upload_bridge.cpp#L34-L60)

**章节来源**
- [restapi/routes.cpp:372-461](file://restapi/routes.cpp#L372-L461)
- [restapi/upload_bridge.cpp:1-102](file://restapi/upload_bridge.cpp#L1-L102)

## API 参考文档

### 端点清单

| 方法 | 路径 | 说明 | 主要状态码 |
|---|---|---|---|
| GET | `/api/health` | 健康检查，返回进程存活时间与状态 | 200 |
| GET | `/api/version` | 返回服务版本信息，支持 JSON/XML | 200/500 |
| GET | `/api/pipelines` | 列出支持的流水线种类 | 200 |
| GET | `/api/kinds` | `/api/pipelines` 别名 | 200 |
| GET | `/api/pipelines/{kind}/default-config` | 获取某 kind 的默认配置 | 200/404 |
| GET | `/api/config/default/{kind}` | 默认配置别名 | 200/404 |
| POST | `/api/config/convert` | 配置在 proto 与文本之间转换 | 200/400 |
| POST | `/api/jobs` | 提交异步作业（支持 multipart） | 202/400/429/503 |
| POST | `/api/jobs/{kind}` | 通过路径指定 kind 提交作业 | 202/400/429/503 |
| POST | `/api/jobs/{id}/cancel` | 取消作业 | 202/404/409 |
| GET | `/api/jobs` | 列出作业，支持 state/offset/limit | 200 |
| GET | `/api/jobs/{id}` | 查询作业状态 | 200/404 |
| GET | `/api/jobs/{id}/result` | 下载作业结果 | 200/404/409 |
| GET | `/api/jobs/{id}/geometry` | 获取几何概览 | 200/404 |
| GET | `/api/jobs/{id}/geometry/{layer}` | 获取单层几何 | 200/404 |
| GET | `/api/jobs/{id}/geometry/path` | 获取连续路径 | 200/404 |
| DELETE | `/api/jobs/{id}` | 删除作业 | 204/404/409 |
| GET | `/api/uploads` | 列出上传文件 | 200/503 |
| GET | `/api/uploads/{id}` | 查询上传状态 | 200/404/503 |
| DELETE | `/api/uploads/{id}` | 删除上传文件 | 204/404/409/503 |
| PUT | `/api/params/{kind}/{key}` | 保存参数 | 200/400/503 |
| POST | `/api/params/{kind}?key=...` | 保存参数别名 | 200/400/503 |
| GET | `/api/params/{kind}/{key}` | 加载参数 | 200/404/503 |
| GET | `/` | 服务索引 | 200 |

**更新** 新增了作业取消、几何数据访问和上传文件管理等端点。

**章节来源**
- [restapi/routes.cpp:441-492](file://restapi/routes.cpp#L441-L492)

### 请求与响应模型

#### 健康检查

- 响应根对象：`Health`。
- 字段：`status`、`uptime_seconds`。

#### 版本信息

- 响应体直接来自 `DllHsBaSlicer` 的版本序列化函数。
- 失败时返回 `Error` 视图。

#### 流水线种类

- 响应根对象：`Pipelines`。
- 子项：`kinds[]`，每项含 `name`。

#### 作业状态视图

- 响应根对象：`Job`。
- 关键字段：`id`、`kind`、`state`、`progress_percent`、`progress_stage`、`success`、`total_layers`、`elapsed_seconds`、`output_path`、`error_message`、`files_transferred`、`total_files`、`geometry_available`、`geometry_layers`。

#### 作业列表

- 响应根对象：`Jobs`。
- 字段：`jobs[]`、`count`。

#### 参数结果

- 响应根对象：`ParamResult`。
- 字段：`success`、`param_id`、`error_message`、`elapsed_seconds`。

#### 错误视图

- 响应根对象：`Error`。
- 字段：`status`、`message`、`detail`。

#### 几何概览

**新增** 几何数据概览视图。

- 响应根对象：`Geometry`。
- 字段：`id`、`geometry_id`、`layer_count`、`bounds[4]`、`z_heights[]`、`layer_bounds[]`、`has_path`。

#### 几何路径

**新增** 连续工具路径视图。

- 响应根对象：`GeometryPath`。
- 字段：`id`、`geometry_id`、`point_count`、`xyz[]`、`bounds[4]`、`z_range[2]`。

#### 上传信息

**新增** 上传文件信息视图。

- 响应根对象：`Upload`。
- 字段：`id`、`job_id`、`file_name`、`original_name`、`path`、`state`、`size_bytes`、`created_at`、`expires_at`、`remaining_seconds`。

**章节来源**
- [restapi/routes.cpp:37-77](file://restapi/routes.cpp#L37-L77)
- [restapi/routes.cpp:93-108](file://restapi/routes.cpp#L93-L108)
- [restapi/job_manager.hpp:47-61](file://restapi/job_manager.hpp#L47-L61)

### 内容协商规则

| 场景 | 行为 |
|---|---|
| 请求体为 XML | `Content-Type` 包含 `xml` 时按 XML 解析 |
| 响应为 JSON | 默认 `application/json; charset=utf-8` |
| 响应为 XML | 默认 `application/xml; charset=utf-8` |
| 显式 `?format=xml` | 强制 XML |
| 显式 `?format=json` | 强制 JSON |
| Accept 含 xml | 优先 XML |
| Accept 含 json | 优先 JSON |
| 均未指定 | 使用 `server.default_format` |

**章节来源**
- [restapi/content_negotiate.cpp:24-61](file://restapi/content_negotiate.cpp#L24-L61)
- [restapi/content_negotiate.hpp:21-47](file://restapi/content_negotiate.hpp#L21-L47)

## 集成指南

### 构建系统集成

`restapi/CMakeLists.txt` 明确依赖：

- `httplib::httplib`。
- `Threads::Threads`。
- `DllHsBaSlicer`。
- `HsBaSlicerFileOperator`。
- `HsBaSlicerUtils`。
- `HsBaSlicerBase`。
- `HsBaPipelineTypes`。
- 可选 `HsBaSlicerLog`（非游戏控制台模式）。
- **新增** OpenSSL（用于 HTTPS 支持）。

Windows 下还会调用 UTF-8 代码页适配宏。

```mermaid
graph LR
RestServer["HsBaSlicerRestServer"] --> Httplib["httplib::httplib"]
RestServer --> Threads["Threads::Threads"]
RestServer --> Dll["DllHsBaSlicer"]
RestServer --> FileOp["HsBaSlicerFileOperator"]
RestServer --> Utils["HsBaSlicerUtils"]
RestServer --> Base["HsBaSlicerBase"]
RestServer --> Types["HsBaPipelineTypes"]
RestServer --> OpenSSL["OpenSSL (可选)"]
```

**图示来源**
- [restapi/CMakeLists.txt:15-52](file://restapi/CMakeLists.txt#L15-L52)

**章节来源**
- [restapi/CMakeLists.txt:1-66](file://restapi/CMakeLists.txt#L1-L66)

### 配置文件驱动

`restapi.ini` 分为多段：

- `[server]`：监听地址、端口、默认格式、并发作业数、最大作业数、结果保留时间、TLS 证书和密钥。
- `[auth]`：认证开关和令牌。
- `[upload]`：上传池配置。
- `[paramstore]`：后端类型、SQLite 路径、MySQL/PostgreSQL 连接参数、加密密码。

**更新** 新增了认证和上传池配置段。

**章节来源**
- [restapi/restapi.ini:1-50](file://restapi/restapi.ini#L1-L50)
- [restapi/server_config.hpp:21-51](file://restapi/server_config.hpp#L21-L51)
- [restapi/server_config.cpp:20-79](file://restapi/server_config.cpp#L20-L79)

## 开发测试与贡献

### 测试位置

仓库中包含 REST API 相关测试，位于 `tests/RestApi/`，可通过 ctest 过滤运行。测试覆盖配置往返、内容协商、作业管理器行为和认证门控。

**更新** 新增了认证门控测试和多部分上传测试。

**章节来源**
- [restapi/job_manager.hpp:88-92](file://restapi/job_manager.hpp#L88-L92)

### 错误处理策略

- 无效 kind：返回 400 或 404。
- 配置体解析失败：返回 400。
- 作业队列满：返回 429。
- 作业不存在：返回 404。
- 作业未完成或失败：返回 409。
- 输出文件不可读：返回 404。
- ParamStore 未配置：返回 503。
- 版本接口内部失败：返回 500。
- **新增** 认证失败：返回 401。
- **新增** 上传功能未启用：返回 503。

**章节来源**
- [restapi/routes.cpp:198-211](file://restapi/routes.cpp#L198-L211)
- [restapi/routes.cpp:213-250](file://restapi/routes.cpp#L213-L250)
- [restapi/routes.cpp:252-300](file://restapi/routes.cpp#L252-L300)
- [restapi/routes.cpp:346-361](file://restapi/routes.cpp#L346-L361)
- [restapi/routes.cpp:363-439](file://restapi/routes.cpp#L363-L439)

## 扩展开发指南

### 新增流水线 kind

要扩展新的流水线 kind，需要在 `pipeline_executor.cpp` 中：

1. 在 `HSBA_DEFINE_KIND` 宏中添加新 kind 条目。
2. 在 `KindTable` 数组中插入对应条目。
3. 确保 `pipelinetypes/pipeline_types.h` 中枚举顺序与静态断言一致。
4. 提供对应的 `*_pipeline.h` 头文件及 C ABI 函数。
5. 在 `config_bridge` 反射表中注册配置结构体字段。

```mermaid
flowchart TD
NewKind["新增流水线 kind"] --> Macro["添加 HSBA_DEFINE_KIND 条目"]
Macro --> Table["更新 KindTable 数组"]
Table --> Assert["校验枚举顺序 static_assert"]
Assert --> Abi["提供 DllHsBaSlicer C ABI"]
Abi --> Reflect["注册配置反射字段"]
Reflect --> Test["补充测试用例"]
```

**图示来源**
- [restapi/pipeline_executor.cpp:50-93](file://restapi/pipeline_executor.cpp#L50-L93)
- [restapi/pipeline_executor.cpp:152-171](file://restapi/pipeline_executor.cpp#L152-L171)

**章节来源**
- [restapi/pipeline_executor.cpp:50-171](file://restapi/pipeline_executor.cpp#L50-L171)

### 自定义作业执行器

`JobManager` 的 `Runner` 类型允许注入自定义执行函数，用于单元测试或替代实现。默认 runner 调用 `pipeline_executor::RunPipeline`。

**章节来源**
- [restapi/job_manager.hpp:88-92](file://restapi/job_manager.hpp#L88-L92)
- [restapi/job_manager.cpp:43-50](file://restapi/job_manager.cpp#L43-L50)

## 示例与教程

### 典型使用流程

1. 准备 `restapi.ini`，确认监听地址、端口、默认格式和安全配置。
2. 启动 `HsBaSlicerRestServer`，必要时使用 `--config`、`--host`、`--port`。
3. 调用 `/api/health` 验证服务可用。
4. 调用 `/api/pipelines` 查看支持的流水线种类。
5. 调用 `/api/pipelines/{kind}/default-config` 获取默认配置。
6. 调用 `/api/config/convert` 将 proto 转为 JSON/XML，或将 JSON/XML 转为 proto。
7. 调用 `/api/jobs` 提交作业，轮询 `/api/jobs/{id}`，最终从 `/api/jobs/{id}/result` 下载结果。
8. 如需参数持久化，配置 `[paramstore]` 后使用 `/api/params`。
9. **新增** 如需几何预览，提交作业时添加 `?geometry=1` 参数。
10. **新增** 如需文件上传，配置 `[upload]` 段并使用 multipart/form-data 提交。

**章节来源**
- [restapi/restapi.ini:1-50](file://restapi/restapi.ini#L1-L50)
- [restapi/routes.cpp:441-492](file://restapi/routes.cpp#L441-L492)

## 部署故障排除与FAQ

### 常见问题

| 问题 | 可能原因 | 处理建议 |
|---|---|---|
| 启动时报无法解析 ini | `restapi.ini` 存在但格式错误 | 检查 INI 语法，或使用默认值启动 |
| 端口被占用 | `server.port` 已被其他进程占用 | 修改端口或停止占用进程 |
| 提交作业立即返回 429 | `max_jobs` 达到上限 | 增大 `max_jobs` 或清理已完成作业 |
| 结果下载返回 404 | 作业未完成或没有可下载结果 | 先轮询状态，再根据 `output_path` 或 `inline_content` 判断 |
| 删除作业返回 409 | 作业正在运行 | 等待作业结束后再删除 |
| 参数接口返回 503 | 未配置 ParamStore 后端 | 填写 `[paramstore]` 段或移除相关调用 |
| 响应格式不符合预期 | Accept 或 `?format=` 设置不一致 | 显式使用 `?format=json` 或 `?format=xml` |
| **新增** 请求返回 401 未授权 | 缺少认证令牌或令牌不正确 | 添加 `Authorization: Bearer <token>` 或 `X-API-Key: <token>` |
| **新增** HTTPS 启动失败 | TLS 证书或密钥文件缺失 | 检查 `tls_cert` 和 `tls_key` 配置 |
| **新增** 上传功能返回 503 | 未配置上传目录 | 设置 `[upload] dir` 配置项 |
| **新增** 几何数据返回 404 | 作业未启用几何捕获或快照已过期 | 提交时添加 `?geometry=1` 参数 |

**章节来源**
- [restapi/server_config.cpp:34-79](file://restapi/server_config.cpp#L34-L79)
- [restapi/routes.cpp:213-300](file://restapi/routes.cpp#L213-L300)
- [restapi/routes.cpp:346-439](file://restapi/routes.cpp#L346-L439)
- [restapi/content_negotiate.cpp:34-56](file://restapi/content_negotiate.cpp#L34-L56)

## 附录

### 性能特性

- 并发切片：由 `server.max_concurrent_jobs` 控制工作线程数。
- 作业容量：由 `server.max_jobs` 控制队列上限。
- 结果保留：由 `server.result_ttl_seconds` 控制已完成作业结果的内存保留时间。
- 请求体上限：路由层设置较大负载上限以容纳嵌入式模型配置。
- 序列化开销：JSON/XML 通过 DOM 构建，避免手工拼接字符串，提高编码正确性。
- **新增** 上传池容量：由 `upload.max_files` 和 `upload.max_total_mb` 控制。
- **新增** 认证开销：使用恒定时间比较防止时序攻击。

**章节来源**
- [restapi/routes.cpp:441-443](file://restapi/routes.cpp#L441-L443)
- [restapi/config_bridge.cpp:5-13](file://restapi/config_bridge.cpp#L5-L13)
- [restapi/job_manager.cpp:334-349](file://restapi/job_manager.cpp#L334-L349)

### 安全与边界

- **新增** 认证机制：支持 Bearer Token 和 X-API-Key 认证，本地回环地址自动跳过认证。
- **新增** HTTPS 支持：通过 OpenSSL 提供 TLS 加密通信。
- **新增** 文件上传安全：文件名经过严格清洗，只允许安全字符集。
- **新增** 上传池隔离：上传文件与作业结果分离管理，具有独立的保留策略。
- 作业删除不允许中断运行中的任务。
- 结果下载可能返回二进制文件或纯文本，客户端应根据 Content-Type 处理。

**章节来源**
- [restapi/restapi.ini:44-50](file://restapi/restapi.ini#L44-L50)
- [restapi/routes.cpp:346-361](file://restapi/routes.cpp#L346-L361)
- [restapi/routes.cpp:280-299](file://restapi/routes.cpp#L280-L299)
- [restapi/auth_gate.cpp:24-37](file://restapi/auth_gate.cpp#L24-L37)
- [restapi/upload_bridge.cpp:16-23](file://restapi/upload_bridge.cpp#L16-L23)