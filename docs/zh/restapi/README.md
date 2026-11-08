# HsBaSlicer REST API 服务进程

`HsBaSlicerRestServer` 是一个**可选**可执行进程，通过 HTTP 暴露 `DllHsBaSlicer`
的 C ABI，响应同时支持 **JSON** 与 **XML**。仅当 CMake 选项 `HSBA_BUILD_REST_API`
为 `ON` 时才构建，且**仅限桌面平台**（Windows / Linux / macOS），其他平台强制 `OFF`。

## 启用构建

HTTP 服务基于 [cpp-httplib](https://github.com/yhirose/cpp-httplib)，该依赖通过
vcpkg 的 **`rest-api`** manifest feature 安装（不属于 `default-features`），该
feature 同时选定 `cpp-httplib[openssl]`，因此启用 REST 的构建都具备 HTTPS 能力。
CMake 选项与 vcpkg feature 必须在同一次配置会话中一起启用：

```sh
cmake --preset <现有preset> \
      -DHSBA_BUILD_REST_API=ON \
      -DVCPKG_MANIFEST_FEATURES=rest-api
cmake --build --preset <现有preset>
```

若只设 `HSBA_BUILD_REST_API=ON` 而未启用 `rest-api`，配置会以 `FATAL_ERROR` 失败并
提示启用（检测逻辑与既有的 `HSBA_COPL` 模式对称）。

只有 HTTP 传输层（`routes.cpp`）依赖 cpp-httplib。非 HTTP 单元（`content_negotiate`、
`config_bridge`、`pipeline_executor`、`job_manager`、`server_config`、`auth_gate`、
`upload_bridge`、`upload_store`）不依赖 HTTP，由 `tests/RestApi/` 下的独立单元测试覆盖。

单元测试与服务共用同一个门控：`HSBA_BUILD_REST_API=OFF`（默认）时既不构建服务，也不构建
这些测试，不会出现“测试开着、被测模块却没构建”的配置。开启后每个用例都链接实现库
`HsBaSlicerRestApi`（其中包含 `routes.cpp`），因此 HTTP 层一旦无法编译，测试构建同样失败。

## 配置（`restapi.ini`）

服务从工作目录读取 `restapi.ini`（与 `logcfg.ini` 约定一致）；构建时会在可执行文件
旁放置一份副本。可用 `--config <file>` 覆盖路径；命令行选项优先级高于 ini，而
`HSBA_REST_AUTH_TOKEN` / `HSBA_REST_TLS_CERT` / `HSBA_REST_TLS_KEY` 环境变量仅在
ini 未给出对应值时生效（镜像因此可以不含 token）。文件缺失不算错误——采用内置默认值。

| 段 | 键 | 默认 | 含义 |
|---|---|---|---|
| `[server]` | `host` | `127.0.0.1` | 绑定地址（`0.0.0.0` 暴露到局域网）。 |
| `[server]` | `port` | `8080` | TCP 端口。 |
| `[server]` | `default_format` | `json` | 既无 `?format=` 又无 `Accept` 时的响应格式。 |
| `[server]` | `max_concurrent_jobs` | `2` | 并发执行切片作业的 worker 线程数。 |
| `[server]` | `max_jobs` | `128` | 跟踪作业上限，超出后新提交返回 429。 |
| `[server]` | `result_ttl_seconds` | `3600` | 终态作业结果保留时长（秒）。 |
| `[server]` | `tls_cert` / `tls_key` | 空 | 启用 HTTPS 的 PEM 证书/私钥，必须同时设置；为空则为纯 HTTP。 |
| `[server]` | `require_tls` | `false` | 为 `true` 时，未配置证书对则拒绝启动。 |
| `[auth]` | `enabled` | 自动 | `true`/`false`/留空（自动）；自动等价于“有 token 就启用”。 |
| `[auth]` | `token` | 空 | 共享令牌，比对 `Authorization: Bearer` 与 `X-API-Key`。 |
| `[paramstore]` | `backend` | `sqlite` | `sqlite`/`mysql`/`postgresql`；留空则 `/api/params` 返回 503。 |
| `[paramstore]` | `sqlite_path`/`host`/`user`/`password`/`database`/`port`/`crypto_password` | | ParamStore 连接（按请求解析）。 |
| `[upload]` | `dir` | 空 | 上传模型落盘根目录；**留空即禁用** multipart 提交（返回 503）。 |
| `[upload]` | `max_files` | `256` | 保留的上传文件数上限，自动抬到不低于 `HSBA_MODEL_POOL_SIZE`（有 CGAL/OCCT 时 10，否则 1）。 |
| `[upload]` | `ttl_seconds` | `604800` | 保留时长（秒），**从作业终态起算**；`0` = 不按时间回收，上限 30 天。 |
| `[upload]` | `evict_mode` | `both` | 回收方式：`ttl`/`count`/`both`/`manual`；未知值回落 `both`。 |
| `[upload]` | `max_file_mb` | `256` | 单个模型文件上限，超出返回 413。 |
| `[upload]` | `max_request_mb` | `260` | 整个请求体上限（含 multipart 边界），自动抬到 `max_file_mb + 4`。 |
| `[upload]` | `max_total_mb` | `0` | 目录总容量上限（MB），`0` = 不限。 |
| `[upload]` | `sweep_interval_seconds` | `60` | 后台回收线程周期，夹到 `[5, 3600]`。 |

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
| `POST` | `/api/jobs?kind={kind}` 或 `/api/jobs/{kind}` | 提交切片作业（JSON/XML 请求体，或 `multipart/form-data`，见下）；追加 `?geometry=1` 则采集逐层几何供下列端点查询。 | `HsBaRun*Pipeline`（异步） |
| `GET` | `/api/jobs?state=&offset=&limit=` | 作业列表 / 分页（可选状态过滤）。 | 作业队列 |
| `GET` | `/api/jobs/{id}` | 作业状态（进度/结果元数据）。 | 作业队列 |
| `GET` | `/api/jobs/{id}/result` | 下载产物（gcode 文本 / 输出文件）；文件分支只回报文件名；已取消的作业返回 409。 | 作业队列 |
| `POST` | `/api/jobs/{id}/cancel` | 请求取消：运行中 → `202` 置位，排队中 → `202` 直接摘除，终态 → `409`，未知 → `404`。 | 作业队列（`cancel_flag`） |
| `GET` | `/api/jobs/{id}/geometry` | 几何摘要（层数 + 全局包围盒）；`?full=1` 时额外展开逐层 `z_heights` / `layer_bounds`。 | `HsBaGeometryLayerCount` |
| `GET` | `/api/jobs/{id}/geometry/{layer}` | 单层环数据（轮廓 / 填充 / 支撑，坐标为扁平数值数组）。 | `HsBaGetLayerGeometry` |
| `GET` | `/api/jobs/{id}/geometry/path` | 螺旋运行那条连续工具路径，坐标为扁平 `x,y,z` 三元组；其他运行一律 `404`。 | `HsBaGetGeometryPath` |
| `GET` | `/api/jobs/{id}/package` | 结果包（zip）的条目清单：名字、字节数、媒体类型；单文件结果返回 `404`。 | 用 miniz 直读 `output_path` |
| `GET` | `/api/jobs/{id}/package/{entry}` | 包内单个条目的字节，类型由扩展名推出；`{entry}` 可以带目录。 | 用 miniz 直读 `output_path` |
| `DELETE` | `/api/jobs/{id}` | 删除排队中或已终态的作业（运行中拒绝），只解除上传关联。 | 作业队列 |
| `GET` | `/api/uploads?state=&offset=&limit=` | 列出上传池条目（`pending`/`in_use`/`terminal`）。 | 上传池 |
| `GET` | `/api/uploads/{id}` | 单条上传详情。 | 上传池 |
| `DELETE` | `/api/uploads/{id}` | 删除上传文件（`in_use` 时返回 409）。 | 上传池 |
| `PUT` | `/api/params/{kind}/{key}` | upsert 配置到 ParamStore。 | `HsBaSavePipelineParams` |
| `GET` | `/api/params/{kind}/{key}` | 从 ParamStore 读取配置。 | `HsBaLoadPipelineParams` |

`{kind}` 取值：`fdm`、`sla`、`sls`、`slm`、`lom`、`tdp`、`waam`、`custom`、`filetransfer`。

同时提供服务字面量（实施计划）别名：`GET /api/kinds`（≡ `/api/pipelines`）、
`GET /api/config/default/{kind}`（≡ `/api/pipelines/{kind}/default-config`）、
`POST /api/params/{kind}?key=`（≡ `PUT /api/params/{kind}/{key}`）。

## 作业取消与层几何

### 状态机

作业 `state` 取值：`queued`、`running`、`success`、`failed`、`cancelled`（后三者为终态）。
`GET /api/jobs/{id}` 的 `progress_stage_id` 是稳定阶段标识符（如 `slicing.layer`），无法分类时
为 `unknown`；`geometry_available` / `geometry_layers` 只在终态后非零。

取消是**协作式**的：流水线只在阶段边界与逐层循环内检查标志，因此 `202` 代的是“已登记”而非
“已停止”；正在进行的大块工作（网格加载、一次 Lua 回调、zip 压缩）必须跑完。终态后再 `POST
cancel` 会得到 `409 job already finished`，但作业记录仍可查。

```json
{
  "id": "7f3c1a2b",
  "kind": "fdm",
  "state": "cancelled",
  "progress_percent": 60,
  "progress_stage": "Slicing layer",
  "success": 0,
  "total_layers": 0,
  "elapsed_seconds": 4.19,
  "output_path": "",
  "error_message": "Cancelled: pipeline cancelled",
  "progress_stage_id": "cancelled",
  "geometry_layers": 0,
  "geometry_available": 0
}
```

### 提交时开启几何采集

`POST /api/jobs/{kind}?geometry=1`（也接受 `geometry=true`）在服务端把结构体的
`capture_geometry` 置 1。该开关**不属于配置字段表**，因此不会出现在 `default-config`、请求体
校验、proto 消息或参数宽表中；`custom` 与 `filetransfer` 不采集几何，这三个端点永远返回 `404`。
**螺旋运行**（`fdm`、`tdp`、`waam` 配置里的 `spiral_mode = 1`）除了逐层环以外还会存下它挤出
的那一条不间断直线，由下文的 `/geometry/path` 提供；摘要里的 `has_path` 会告诉你该不该去请求它。

### `GET /api/jobs/{id}/geometry`

```json
{
  "id": "7f3c1a2b",
  "geometry_id": 12,
  "layer_count": 214,
  "bounds": [0.0, 0.0, 68.4, 51.2],
  "has_path": 1,
  "z_heights": [0.2, 0.4],
  "layer_bounds": [0.0, 0.0, 68.4, 51.2, 0.0, 0.0, 67.9, 50.8]
}
```

`z_heights` 与 `layer_bounds` **仅在 `?full=1` 时输出**；默认响应只有前五个字段（`layer_count` 可达
上千，故不全量展开）。`geometry_id` 对原生宿主直接可用（同一进程内可传给 `HsBaGetLayerGeometry`），
对 HTTP 客户端只是个不透明数字。`has_path` 只在 `GET /api/jobs/{id}/geometry/path` 能成功时为 `1`，
因此客户端不必对“没有连续路径”的运行去碰运气。

### `GET /api/jobs/{id}/geometry/{layer}`

```json
{
  "id": "7f3c1a2b",
  "layer_index": 0,
  "z_height": 0.2,
  "bounds": [0.0, 0.0, 68.4, 51.2],
  "outlines": [{"point_count": 4, "closed": 1, "xy": [0.0, 0.0, 68.4, 0.0, 68.4, 51.2, 0.0, 51.2]}],
  "fills": [{"point_count": 2, "closed": 0, "xy": [1.0, 2.0, 30.0, 2.0]}],
  "supports": []
}
```

坐标单位为毫米，`xy` 是扁平的 `x0,y0,x1,y1,...`（以此控制响应体体积）；`closed=1` 为闭合轮廓，
`0` 为开放折线。槽位含义按工艺不同：FDM 三个槽均有数据；SLA 的地板/筏层只出现在 **layer 0 的
`fills`**；SLS / SLM / LOM / TDP / WAAM 只有 `outlines`。

几何只在作业**终态之后**可读（采集发生在流水线末尾）；越界的 `{layer}` 返回
`404 layer index out of range`（`detail` 带 `layer_count=N`），非数字层号返回
`400 layer index must be a non-negative integer`，快照被进程内池上限（环境变量
`HSBA_GEOMETRY_POOL_MAX`，默认 4）回收后返回 `404 geometry snapshot is no longer available`。

### `GET /api/jobs/{id}/geometry/path`

```json
{
  "id": "7f3c1a2b",
  "geometry_id": 12,
  "point_count": 5210,
  "xyz": [34.1, 21.8, 0.2, 34.3, 21.8, 0.2, 34.5, 21.9, 0.4],
  "bounds": [0.0, 0.0, 68.4, 51.2],
  "z_range": [0.2, 42.8]
}
```

`xyz` 是一个长度 `3 * point_count` 的扁平毫米数组（`x0,y0,z0,x1,y1,z1,...`），按**挤出顺序**存放整条
折线；`z` 沿数组单调不减，因为每个点带的是它所属那层的高度。这条线是开放的——没有 `closed` 标志
——由客户端把相邻点直接连成直线，因为这些直线正是喷嘴走过的轨迹（包括逐层环视图无法展示的
空驶段）。只有螺旋运行有这条线，本服务其他任何运行都不会产生它。

错误阶梯在上面那套之外多一层：`404 job not found`（id 未知）、`detail=submit with ?geometry=1` 的
`404 job captured no layer geometry`（提交时没要快照）、`404 geometry snapshot is no longer available`
（已被池回收），以及 `404 job emitted no continuous path` 且 `detail=spiral_mode=1`——快照存在、层也
有，只是这次运行不是螺旋的，没有折线可给。`path` 是保留段：URL 里字面量 `path` 会被本端点拿走，
因此它不是一个合法的 `{layer}` 取值。

## 结果包条目

有些流水线最后写出一个压缩包（SLA 总是如此；SLS、SLM、LOM、TDP 通过各自的导出 Lua 做到）。
`/result` 只能把包当成一个不透明的下载文件递出去，因此这里多两个只读路由：一个列条目，一个递
单个条目的字节。两者都不引入任何状态——包已在磁盘上、作业已完成，所以每次请求直接打开它。

### `GET /api/jobs/{id}/package`

```json
{
  "id": "7f3c1a2b",
  "name": "result.zip",
  "entry_count": 6,
  "entries": [
    { "path": "config.json", "size": 412, "uncompressed_size": 1180, "media_type": "application/json" },
    { "path": "layers/layer_0.png", "size": 5033, "uncompressed_size": 18241, "media_type": "image/png" }
  ]
}
```

`path` 是**压缩包内存放的原样名字**，用 `/` 分隔；`name` 是流水线写出的包文件名本身（永远不包含
目录）。`size` 是压缩后字节数，`uncompressed_size` 是展开后的字节数；`media_type` 由扩展名推得，
与 `GET /package/{entry}` 发出的 `Content-Type` 同一张表。目录条目会被跳过，所以 `entry_count` 只数文件。

内置 SLA 导出器写入 `config.json`、`layers/layer_N.png|jpg|svg`、`floor/floor_raft.*` 与
`supports/layer_N.*`——但渲染为空的层**根本不写入** zip，所以层号不一定连续，总数只是下界。SLS、
SLM、LOM、TDP 的条目名来自调用方提供的导出 Lua，它存什么都有可能：清单对「你问的这一次作业」是准确
的，但**不是可供代码依赖的稳定契约**。

### `GET /api/jobs/{id}/package/{entry}`

把单个条目以原字节发出，媒体类型由扩展名决定——`image/png`、`image/jpeg`、`image/svg+xml`、
`application/json`，`.txt`/`.gcode`/`.nc` 为 `text/plain; charset=utf-8`，其余为
`application/octet-stream`。`{entry}` 是贪婪匹配，因此 `layers/layer_0.png` 这样的存内目录可以直接按
写法访问；但 URL 的每一段仍需客户端各自做百分号编码。

所有响应都带 `X-Content-Type-Options: nosniff`，误标的条目不会被浏览器重新解读。`image/svg+xml`
响应额外带 `Content-Security-Policy: default-src 'none'`：包里的 SVG 出自导出脚本，可信度和其他
结果字节持平，该头是所有会执行其脚本的上下文（`<img>` 引用不会）的兜底。

### 两个包路由的错误阶梯

前几层与 `/result` 完全相同且同序，已经处理过 `/result` 的客户端不需要新的错误词表：

| 状态 | `message` | `detail` | 何时 |
| --- | --- | --- | --- |
| `400` | `invalid package entry name` | 条目名 | 仅条目路由，且在查作业**之前**就校验：名字为空、超 512 字节、以 `/` 开头、含反斜杠、含控制字符或任意位置含 `..`。中间的 `/` 合法。 |
| `401` | `missing credentials` / `invalid credentials` | 发送提示 / 不匹配说明 | `[auth]` 的 pre-routing 关卡，与非公开端点一致；它先于两个包处理函数。 |
| `404` | `job not found` | 作业 id | id 未知。 |
| `409` | `job was cancelled` | 作业 id | 已取消的作业没有可用结果。 |
| `409` | `job not finished successfully` | 作业 id | 仍为 `queued` / `running` / `failed`。 |
| `404` | `result is not a package` | `inline result`；完全没有结果时为作业 id | 流水线内联回答（FDM G-code）或什么都没写出。 |
| `404` | `package not readable` | 包文件名本身 | 文件已不在、不是 zip、或打不开。永不报出服务端工作目录。 |
| `404` | `package entry not found` | 条目名 | 这个包里没这个条目。 |
| `409` | `package entry too large` | 条目名 | `uncompressed_size` 超过 32 MiB（`PACKAGE_ENTRY_MAX_BYTES`）。该判定在分配任何字节**之前**直接用包内记录完成，所以恶意的占位条目无法让本进程展开出 GB 级内存。 |

`404` 回答从不复述包路径；条目名检查先于一切运行，意味着路径穿越尝试在没碰文件系统之前就被拒掉。

## 模型文件上传

`POST /api/jobs/{kind}` 除 JSON/XML 请求体外还接受 **`multipart/form-data`**：一个名为
`config` 的 part（JSON 或 XML，由该 part 的 `Content-Type` 决定），加一个名为 `model`
的 part（模型文件）。

```sh
curl -X POST http://127.0.0.1:8080/api/jobs/fdm \
     -F 'config=@fdm.json;type=application/json' \
     -F 'model=@cube.stl'
# 202 + Location: /api/jobs/job-7
```

服务端处理顺序：

1. 文件名清洗：只保留 `[A-Za-z0-9._-]`，剔除目录部分与 `..`，限长 128 字节，空则回落 `model.bin`。
2. 落盘到 `<upload_dir>/<job-id>/<清洗名>`，并将该路径反射注入配置的 `model_path`
   （若结构体声明了 `model_name`，则填入客户端原始名）。字符串复制进 `StringArena`，
   与 `DeserializeConfig` 同一所有权模型（**禁止 malloc**）。
3. 登记进上传池并提交作业；作业被 worker 取走前条目状态为 `in_use`，不会被回收。

失败路径：`400`（缺 `config`/`model` part、config 解析失败，或该 kind 没有 `model_path`，
例如 `filetransfer`）、`413`（模型文件超 `max_file_mb`）、`429`（作业队列满）、`503`
（`[upload] dir` 未配置，或上传池已满）。任一失败都会删除刚落盘的文件。

### 上传池与保留策略

上传池与作业的 `result_ttl_seconds` **完全解耦**：作业结束只把条目从 `in_use` 变为
`terminal` 并开始计时；作业被 reap（或 `DELETE /api/jobs/{id}`）只解除关联，**不会删除模型
文件**。文件只按上传池自身的策略回收：

- `ttl`：从 `terminal_at`（未关联时从 `created_at`）起算，超过 `ttl_seconds` 回收。
- `count`：条目数超过 `max_files` 时，按 `created_at` 升序淘汰最旧。
- `both`（默认）：两者同时生效。
- `manual`：只能由 `DELETE /api/uploads/{id}` 删除。

`in_use` 的条目在 `ttl` 与 `count` 下都不会被回收；若池内条目全部在使用中，新上传返回 503，
而不是牺牲正在加工的模型。进程重启时 `upload_dir` 下的 `job-*` 目录会被重新登记
（`created_at` 取目录 mtime），因此重启既不会丢失去向，也不会让已过期文件永久留存；
非 `job-*` 目录不予接管。

条目字段：`id`（`upl-<n>`）、`job_id`、`file_name`（清洗后）、`original_name`（客户端原名）、
`path`、`state`、`size_bytes`、`created_at`、`expires_at`（0 = 不按时间过期）、
`remaining_seconds`。

```sh
curl 'http://127.0.0.1:8080/api/uploads?state=terminal&limit=10'
curl http://127.0.0.1:8080/api/uploads/upl-3        # 404 表示已删除或从未存在
curl -X DELETE http://127.0.0.1:8080/api/uploads/upl-3   # 204；in_use 时 409
```

### 输出路径

配置里的 `output_path` 对所有工序都生效：流水线把产物写到该路径，作业进入终态后
`GET /api/jobs/{id}` 的 `output_path` 返回这个文件（WebUI 的 Output 列直接显示并可复制）。
`GET /api/jobs/{id}/result` 的分支取决于工序：FDM 把 G-code 文本额外留在内存里并直接返回该文本，
custom 返回脚本的返回值文本，其余工序返回 `export_path` 指向的文件。

各工序的产物形态不同，但走同一条回报路径：`fdm` 写 G-code 文本；`sla`、`sls`、`slm`、`lom`、`tdp`
写 zip 包（由结果结构的 `export_path` 回报）；`waam` 写机器人路径文本；`custom` 由脚本自行决定写什么，
并用 `HsBa.setOutputPath` 回报路径。其中 `sls`/`slm`/`lom`/`tdp` 没有内置导出器，必须给出
`export_lua_script`；`custom` 必须给出 `pipeline_lua_script` 或 `pipeline_lua_source`。缺少这些脚本时
作业以 `failed` 结束且 `output_path` 保持为空——服务端绝不会回报一个并未写出的文件。
`custom` 写出的文件只能按 `output_path` 取用，因为它的 result 端点让位于脚本返回值。

文件分支的 `Content-Disposition` 只携带 `output_path` 的最后一段文件名，不泄露服务端目录；
名字中出现引号、反斜杠或控制字符时退回作业 id，内联文本分支不发该头。Web 控制台按同一
规则决定保存名（无头时按工序补上扩展名：`.gcode`、`.zip`、`.txt`），并提供 **Preview** 面板：
文本类结果渲染前 64 KiB 并报告整个文件的字节数与行数，二进制结果只报名称与大小。该契约由
`tests/RestApi/endpoint_http_test.cpp`（把生产路由表装到真实 cpp-httplib 监听上）与
`webui/test/e2e.test.js`（用示例下载的开放模型真实跑一遍所有会写文件的工序）覆盖。

## 鉴权与 TLS

鉴权**仅在非回环地址绑定后生效**。`127.0.0.1` / `::1` / `localhost` 绑定按设计免 token，
本地的 curl 调试与容器 `HEALTHCHECK` 探针无需携带凭据。

```sh
# 以 HTTPS 暴露到局域网，并要求共享令牌
HsBaSlicerRestServer --host 0.0.0.0 \
    --tls-cert server.crt --tls-key server.key --require-tls \
    --auth-token "$HSBA_REST_AUTH_TOKEN"

curl -k -H "Authorization: Bearer $HSBA_REST_AUTH_TOKEN" https://host:8080/api/version
curl -k -H "X-API-Key: $HSBA_REST_AUTH_TOKEN"            https://host:8080/api/jobs
curl -k https://host:8080/api/health   # 探针：始终免凭据可达
```

细节与有意保留的边界：

- 接受的凭据：`Authorization: Bearer <token>`（`bearer` 方案名大小写无关；无方案名时按
  原文比对）或 `X-API-Key: <token>`；两者同传时以 `Authorization` 为准。比对为常量时间。
- 免除路径：`/` 与 `/api/health`。其余端点（含以后新增的）均由 pre-routing 钩子拦截。
- 以下配置会直接启动失败并提示：`enabled=true` 但 token 为空、`tls_cert`/`tls_key` 只配
  一个、`require_tls=true` 却无证书对。
- 非回环绑定但未配 token 仅输出警告（并同时提示是否明文），不会假装安全。
- 只有单一共享令牌：无多用户、无令牌轮换接口、无限流（`[rate_limit]` 仍为预留）。需要
  这些能力时请在前面放一个反向代理。

## Web 控制台（独立模块）

浏览器控制台位于 [`webui/`](../../../webui/README.md)，它刻意**不**属于 `HsBaSlicerRestServer`：
不链接任何 C++ 库，也不进入 CMake/vcpkg 构建，而是用自己的 CI 工作流
（`.github/workflows/rest-webui.yml`）独立发布。它是一个零依赖的 Node.js 进程，托管静态页面
并把 `/api/*` 反向代理到本服务，因此本服务无需任何 CORS 支持。

```sh
cd webui
node server.js --rest-url http://127.0.0.1:8080 --token "$HSBA_REST_AUTH_TOKEN"
# http://127.0.0.1:5080/
```

代理像普通客户端一样调用 API，因此服务端的端点、格式协商、上传与鉴权契约均无变化。

## 切片流程

1. `GET /api/pipelines/{kind}/default-config` 获取模板。
2. 修改字段后 `POST /api/jobs/{kind}` 提交配置体 → `202` 且 `Location: /api/jobs/{id}`；
   若模型不在服务端本地路径上，改用 `multipart/form-data` 一并上传（见上节）。
3. 轮询 `GET /api/jobs/{id}` 直到 `state` 为 `success`、`failed` 或 `cancelled`。
4. 成功后取 `GET /api/jobs/{id}/result`。
5. （可选）提交时加 `?geometry=1`，终态后用 `GET /api/jobs/{id}/geometry` 拿摘要、
   `GET /api/jobs/{id}/geometry/{layer}` 拿单层轮廓做预览绘制；摘要里 `has_path` 为 1 时，
   再用 `GET /api/jobs/{id}/geometry/path` 取整条连续路径。
6. （可选）结果是压缩包时（SLA 总是如此），`GET /api/jobs/{id}/package` 列条目，`GET /api/jobs/{id}/package/{entry}`
   取其中一张层图，不必先下载整个包。
7. （可选）`POST /api/jobs/{id}/cancel` 中途取消；取消后 `result` 端点返回 `409`。

## 错误

失败响应统一为 `{code, message, detail}` JSON / `<Error>` XML：`400` kind/body/config 转换非法
或缺 multipart part（几何层号非非负整数、包条目名不合法也在此列）、`401` 缺凭据或凭据不匹配（附 `WWW-Authenticate: Bearer realm="HsBaSlicer"`）、
`404` 作业/上传/key 不存在，或几何未采集 / 快照已回收 / 层号越界 / 结果不是包 / 包读不了 / 条目不存在、`409` 作业未完成（取结果）、
已取消（取结果，`job was cancelled`）、仍在运行（删除作业）、已终态（取消请求），或上传仍被作业引用
（删除上传）、条目展开后超过 32 MiB（`package entry too large`）、`413` 模型文件超过 `[upload] max_file_mb`、`429` 队列已满、`503` 未配置 ParamStore
或未启用模型上传 / 上传池已满。`DELETE /api/jobs/{id}` 与 `DELETE /api/uploads/{id}` 成功
返回 `204`；`POST /api/jobs/{id}/cancel` 成功返回 `202`（取消尚在展开中，不是已完成）。

## 安全说明

回环绑定按设计不鉴权；一旦绑到其他地址，只要配了 `[auth] token`（或 `--auth-token` /
`HSBA_REST_AUTH_TOKEN`）即强制鉴权。对外网络请启用 TLS——自配 PEM 对，或由终结 TLS 的反向
代理承接——并切勿将真实 token 烧入镜像。
