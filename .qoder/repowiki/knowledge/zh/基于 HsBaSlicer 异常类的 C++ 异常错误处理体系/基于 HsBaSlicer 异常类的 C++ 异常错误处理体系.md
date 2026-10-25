---
kind: error_handling
name: 基于 HsBa::Slicer 异常类的 C++ 异常错误处理体系
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - restapi/job_manager.cpp
    - restapi/routes.cpp
    - fileoperator/LuaAdapter.cpp
    - fileoperator/RemoteExecutor.cpp
    - fileoperator/bit7z_zipper.cpp
    - fileoperator/bit7z_unzipper.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/FloatPolygons.cpp
---

## 1. 采用的系统/方式

HsBaSlicer 采用 **C++ 标准异常 + 自定义异常类** 的错误处理机制，没有使用返回值错误码（除 C ABI 层 `HsBaParamStoreResult_t` 等少数桥接结构外），也没有 panic/recover。核心异常类型集中在 `base/error.hpp`，全部继承自 `std::runtime_error`，并通过 `what()` 传递人类可读消息。

- 异常根类：`HsBa::Slicer::RuntimeError`
- 语义化子类：`OutOfRangeError`、`InvalidArgumentError`、`IOError`、`NotImplementedError`、`NullValueError`、`NotSupportedError`、`NotFoundError`、`AlreadyExistsError`、`PermissionDeniedError`、`TimeoutError`、`InterruptedError`、`CancelledError`、`OutOfMemoryError`
- 所有子类仅通过 `using` 复用基类构造函数与 `what()`，不扩展字段；错误分类完全依赖异常类型而非枚举码。

## 2. 关键文件与包

- `base/error.hpp` / `base/error.cpp`：异常类型定义与空实现单元
- `DllHsBaSlicer/*_pipeline.cpp`、`DllHsBaSlicer/model_preprocess.cpp`：C ABI 边界处统一捕获 `HsBa::Slicer::RuntimeError`，将异常转换为可跨 ABI 返回的 `error_message` 字符串
- `restapi/job_manager.cpp`：异步作业执行器，用 `try/catch(std::exception)` + `catch(...)` 兜底，把异常消息写入 `JobEntry.error_message` 并标记 `JobState::Failed`
- `restapi/routes.cpp`：HTTP 路由层捕获 `std::exception`，调用 `ReplyError(res, 400, ...)` 转为 JSON/XML 响应体
- `fileoperator/RemoteExecutor.cpp`、`bit7z_unzipper.cpp`、`bit7z_zipper.cpp`：底层 I/O 抛出 `IOError` / `InvalidArgumentError` / `NotSupportedError`，由上层 LuaAdapter 捕获后包装为 Lua 错误
- `2D/ImageToPolygons.cpp`、`2D/FloatPolygons.cpp`：算法层直接抛出自定义异常（如 FreeType 初始化失败、Lua 脚本错误）

## 3. 架构与约定

### 3.1 分层传播策略

| 层次 | 行为 |
|---|---|
| 核心算法层（`2D/`、`support/`、`paths/` 等） | 直接 `throw` 具体异常子类，携带描述性消息 |
| 业务库层（`LibHsBaSlicer/`、`fileoperator/`） | 继续向上抛出，或在 Lua 适配层捕获后转成 Lua error |
| C ABI 层（`DllHsBaSlicer/*_pipeline.cpp`、`model_preprocess.cpp`） | `catch (const HsBa::Slicer::RuntimeError&)`，提取 `what()` 放入 `SliceOutcome.error_message`，让 C 调用方拿到字符串错误 |
| HTTP API 层（`restapi/`） | `catch (const std::exception& e)` → `ReplyError(res, 400, e.what(), ...)`；未知异常走 `catch (...)` 写 `"unknown pipeline error"` |
| 异步作业层（`job_manager.cpp::Execute`） | 捕获所有异常后仍设置 `percent=100`、`finish=now()`，保证作业最终完成并被回收 |

### 3.2 与第三方异常的隔离

- OpenCV 异常 `cv::Exception` 在 `2D/ImageToPolygons.cpp` 中被显式捕获，避免泄漏到上游
- bit7z 异常 `bit7z::BitException` 在 `fileoperator/bit7z_*` 中被捕获并包装为 `IOError`
- 这些第三方异常不会穿透到 `DllHsBaSlicer` 的 C ABI 边界——该边界只捕获 `HsBa::Slicer::RuntimeError` 及其派生类

### 3.3 无全局错误码表

仓库中没有统一的 `ErrorCode` 枚举或错误码映射表；错误分类完全依赖异常类型。`base/error.cpp` 是空翻译单元，说明所有逻辑都在头文件中内联完成。

## 4. 约定与约束

- **异常类型即错误分类**：所有自定义错误都继承自 `HsBa::Slicer::RuntimeError`，调用方通过 `catch(const RuntimeError&)` 即可捕获整个错误族，或通过更具体的派生类做细粒度分支。
- **C ABI 边界必须捕获 `HsBa::Slicer::RuntimeError`**：`DllHsBaSlicer` 下每个 pipeline 文件（fdm/sla/slm/sls/tdp/waam/lom/file_transfer）以及 `model_preprocess.cpp` 都以相同模式 catch 该异常，将其 `what()` 写入 `SliceOutcome.error_message`，这是跨 DLL 边界的强制约定。
- **HTTP 层统一用 `ReplyError`**：`restapi/routes.cpp` 中所有错误路径均调用 `ReplyError(res, status_code, message, detail, outfmt)`，将异常消息序列化为请求端期望的格式（JSON/XML/proto）。
- **异步作业不可悬挂**：`JobManager::Execute` 无论是否抛异常都会设置 `percent=100`、`finish=now()`，确保作业状态最终收敛为 `Success` 或 `Failed`，从而被 `ReapLocked` 回收。
- **未分类异常兜底为 `catch(...)`**：在 `restapi/routes.cpp` 和 `restapi/job_manager.cpp` 中，`catch(...)` 作为最后防线，分别返回固定消息 `"unknown pipeline error"` 或 400 响应，防止非 `std::exception` 类型异常导致进程崩溃。
- **Lua 侧通过 `LuaAdapter` 转换异常**：`fileoperator/LuaAdapter.cpp` 捕获 `RuntimeError` 后以 Lua error 形式返回给脚本，避免 C++ 异常穿透到 Lua VM。
- **无 panic/recover、无 goto cleanup**：代码中未发现 `setjmp/longjmp`、`__try/__except` 等非标准恢复机制；资源清理主要依靠 RAII（`StringArena`、智能指针、`lock_guard`）。
- **测试覆盖**：`tests/` 目录下未见专门的异常单元测试文件，但各模块测试通过正常路径间接验证了异常未被误抛。