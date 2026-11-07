---
kind: error_handling
name: HsBaSlicer 错误处理体系：自定义异常、REST 统一错误视图与作业执行兜底捕获
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - restapi/routes.cpp
    - restapi/job_manager.cpp
    - restapi/upload_store.cpp
    - cipher/encoder.cpp
    - cipher/encrypt.cpp
    - meshmodel/CgalModel.cpp
---

## 1. 采用的系统/方法

- **C++ 标准异常 + 自定义派生类**：核心库 `base/error.hpp` 定义了一组继承自 `std::runtime_error` 的语义化异常类型（`RuntimeError`、`OutOfRangeError`、`InvalidArgumentError`、`IOError`、`NotImplementedError`、`NullValueError`、`NotSupportedError`、`NotFoundError`、`AlreadyExistsError`、`PermissionDeniedError`、`TimeoutError`、`InterruptedError`、`CancelledError`、`OutOfMemoryError`），全部位于命名空间 `HsBa::Slicer`。
- **HTTP 层统一错误响应**：restapi 模块通过内部函数 `ReplyError(res, status, message, detail, fmt)` 将错误序列化为统一的 `ErrorView{status, message, detail}` JSON/XML 结构，由 `SerializeValue` 输出。
- **作业执行兜底 try/catch**：`JobManager::Execute` 用 `try { runner_(...) } catch(std::exception& e) { ... } catch(...) { "unknown pipeline error" }` 包裹整个切片流水线调用，把 C ABI 调用的任何异常都转化为可序列化的失败状态，不向客户端泄漏堆栈。
- **文件 I/O 使用 `std::error_code`**：restapi 中所有 `std::filesystem` 操作（创建目录、写入/删除上传文件）均通过 `std::error_code ec` 参数返回错误，避免抛出异常；仅在无法恢复时调用 `ReplyError(500, ...)`。

## 2. 关键文件与包

| 文件 | 职责 |
|---|---|
| `base/error.hpp` / `base/error.cpp` | 自定义异常类型定义（仅头文件实现，`.cpp` 仅为翻译单元占位） |
| `restapi/routes.cpp` | HTTP 路由与 `ReplyError` / `ReplyValue` 等统一回复辅助函数 |
| `restapi/job_manager.cpp` | 作业队列与 `WorkerLoop`/`Execute`，包含对底层流水线的异常兜底 |
| `restapi/upload_store.cpp` | 基于 `std::error_code` 的文件系统错误处理示例 |
| `cipher/encoder.cpp`、`cipher/encrypt.cpp` | 直接抛 `std::invalid_argument` 的参数校验异常 |
| `meshmodel/CgalModel.cpp` | 注释说明 CGAL 可能抛出 `std::exception`，体现对第三方异常的容忍 |

## 3. 架构与约定

### 3.1 异常层次
```
std::runtime_error
└── HsBa::Slicer::RuntimeError
    ├── OutOfRangeError
    ├── InvalidArgumentError
    ├── IOError
    ├── NotImplementedError
    ├── NullValueError
    ├── NotSupportedError
    ├── NotFoundError
    ├── AlreadyExistsError
    ├── PermissionDeniedError
    ├── TimeoutError
    ├── InterruptedError
    ├── CancelledError
    └── OutOfMemoryError
```\n所有派生类仅通过 `using std::runtime_error::runtime_error;` 和 `using std::runtime_error::what;` 复用基类构造与消息接口，没有额外字段——错误信息完全依赖 `what()`。

### 3.2 REST 错误响应格式
`ReplyError` 调用 `ReplyValue` 序列化一个 `ErrorView`，其字段为 `status` (int)、`message` (string)、`detail` (string)。例如：
- 400 `missing model part` / `expected a multipart part named "model"`
- 413 `model file too large` / `exceeds [upload] max_file_mb`
- 429 `job queue full` / `max_jobs reached`
- 404 `job not found` / `<id>`
- 409 `job was cancelled` / `<id>` 或 `job not finished successfully` / `<id>`
- 500 `could not store model file` / `<BaseFileName(path)>`（仅暴露文件名，不泄露服务器路径）
- 503 `upload pool full` / `raise [upload] max_files or delete old uploads`

### 3.3 作业执行异常隔离
`JobManager::Execute` 在 worker 线程内调用 `runner_`（即 DllHsBaSlicer 的 C ABI 入口），并用两层 catch 捕获：
- `catch(const std::exception&)` → 取 `e.what()` 存入 `thrown`；
- `catch(...)` → 固定消息 `"unknown pipeline error"`。
随后统一释放 config/arena、更新 job 状态，使上层 `routes.cpp` 始终看到稳定的 JobStatusView，而不是被异常打断。

### 3.4 资源清理与错误恢复
- 上传写入失败后显式 `std::filesystem::remove(stored)` 并移除父目录，再 `ReplyError(500, ...)`。
- 配置反序列化失败后先 `DestroyConfig(kind, config)` 并 `ctx.uploads.Remove(upload_id)` 回滚已注册的上传，再 `ReplyError(400, ...)`。
- 队列满 (`Submit` 返回空 id) 时先 `ctx.uploads.ReleaseByJob(job_id)` 再 `Remove`，最后 `ReplyError(429, ...)`。

## 4. 观察到的约定与约束

- **自定义异常命名**：所有业务异常以 `Error` 后缀结尾（如 `NotFoundError`、`PermissionDeniedError`），根类型为 `RuntimeError`，而非直接使用裸 `std::runtime_error`。这是 `base/error.hpp` 中定义的完整集合，未见新增类型。
- **HTTP 端点不直接向客户端抛出异常**：所有错误路径走 `ReplyError(status, message, detail, outfmt)`，由 `routes.cpp` 内的局部 helper 统一序列化；`main.cpp` 顶层也有一处 `catch(const std::exception&)` 作为进程级兜底。
- **文件系统 I/O 不抛异常**：restapi 中所有 `std::filesystem` 调用均传 `std::error_code` 参数，错误分支转为 `ReplyError` 或静默忽略（如删除临时目录时只记录 `ec` 而不传播）。
- **第三方/底层异常被吞掉并降级**：`JobManager::Execute` 的 `catch(...)` 把未知异常映射为固定字符串 `"unknown pipeline error"`，确保 C ABI 边界外的任意崩溃不会泄漏到 HTTP 层。
- **CGAL 等第三方库异常按 `std::exception` 处理**：`meshmodel/CgalModel.cpp` 中注释明确说明“CGAL operations may throw std::exception or derived types”，表明代码预期底层几何库抛标准异常而非自定义异常。
- **未观察到 panic/recover 机制**：项目是纯 C++，无 Rust-style recover；唯一的全局保护是 restapi 主进程的顶层 `catch(std::exception&)`。
- **未观察到全局错误码枚举或 `std::error_code` 分类**：除 `std::error_code` 用于 I/O 外，业务错误通过异常类型区分，没有统一的 `ErrorCode` 枚举或 `Result<T,E>` 类型。
