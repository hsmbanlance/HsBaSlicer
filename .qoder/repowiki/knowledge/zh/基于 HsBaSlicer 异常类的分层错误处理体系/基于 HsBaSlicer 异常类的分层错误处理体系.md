---
kind: error_handling
name: 基于 HsBa::Slicer 异常类的分层错误处理体系
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - LibHsBaSlicer/Pipeline/custom_pipeline.hpp
    - LibHsBaSlicer/Pipeline/fdm_pipeline.hpp
    - LibHsBaSlicer/Pipeline/sla_pipeline.hpp
    - LibHsBaSlicer/Pipeline/lom_pipeline.hpp
    - LibHsBaSlicer/Pipeline/slm_pipeline.hpp
    - LibHsBaSlicer/Pipeline/sls_pipeline.hpp
    - LibHsBaSlicer/Pipeline/tdp_pipeline.hpp
    - LibHsBaSlicer/Pipeline/waam_pipeline.hpp
    - LibHsBaSlicer/Preprocess/model_preprocess.hpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - DllHsBaSlicer/geometry_store.cpp
    - DllHsBaSlicer/job_control.cpp
    - restapi/job_manager.cpp
    - restapi/routes.cpp
    - restapi/main.cpp
---

## 1. 总体方案

HsBaSlicer 采用 **C++ 异常 + 结构化结果对象** 的两层错误模型：

- **内核层（LibHsBaSlicer）**：通过 `base/error.hpp` 定义的自定义异常类表达预期失败，核心流水线函数在文档中声明“never throws for expected failures”，将成功/取消/错误统一封装到返回结构体（如 `CustomOutput`、各 pipeline result）。
- **ABI 层（DllHsBaSlicer）**：作为 C ABI 与 C++20 内核之间的边界，捕获 `HsBa::Slicer::*Error`，将其转换为 C 结果结构中的 `success` / `cancelled` / `error_message` 字段，保证 C 调用方不接触 C++ 异常。
- **REST API 层（restapi/）**：使用最外层的 `catch (const std::exception&)` 兜底，将异常消息写入 HTTP 响应；对作业调度还额外 `catch (...)` 兜底未类型化异常。

没有发现 `panic/recover` 语义（非 Rust），也没有全局 error middleware——错误处理是显式的 try/catch 块。

## 2. 关键文件与类型

- `base/error.hpp`：定义 `RuntimeError` 基类及其派生类 `OutOfRangeError`、`InvalidArgumentError`、`IOError`、`NotImplementedError`、`NullValueError`、`NotSupportedError`、`NotFoundError`、`AlreadyExistsError`、`PermissionDeniedError`、`TimeoutError`、`InterruptedError`、`CancelledError`、`OutOfMemoryError`。全部继承自 `std::runtime_error`，仅提供构造函数和 `using what`，无额外状态。
- `base/error.cpp`：空翻译单元（所有实现 inline）。实际抛出的多为标准库异常（如 `cipher/encoder.cpp` 抛 `std::invalid_argument`，`meshmodel/CgalModel.cpp` 抛 `std::runtime_error`）。
- `LibHsBaSlicer/Pipeline/*.hpp`：每个工艺流水线头文件都包含注释约定“Never throws for expected failures; they surface through success/cancelled/error”。例如 `custom_pipeline.hpp` 的 `CustomOutput` 携带 `success`、`cancelled`、`error`、`elapsed_seconds` 等字段。
- `DllHsBaSlicer/file_transfer_pipeline.cpp`：典型捕获模式——先 catch `CancelledError`，再 catch `RuntimeError`，将消息前缀化为 `"Cancelled: "` 或 `"File transfer error: "`，供上层按字符串前缀区分取消与失败。
- `DllHsBaSlicer/model_preprocess.cpp`、`DllHsBaSlicer/geometry_store.cpp`、`DllHsBaSlicer/job_control.cpp`：大量 `catch (const HsBa::Slicer::RuntimeError&)` 包裹底层调用，统一转为 C ABI 可表达的失败路径。
- `restapi/job_manager.cpp`、`restapi/routes.cpp`、`restapi/main.cpp`：顶层 `catch (const std::exception& e)` 兜底，`job_manager.cpp` 还有 `catch (...)` 兜底未知异常。

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
```

### 3.2 三层传播规则

| 层级 | 行为 |
|---|---|
| LibHsBaSlicer（内核） | 正常流程返回结构化结果；文档明确“never throws for expected failures”；参数校验等仍抛 `InvalidArgumentError` 等具体异常 |
| DllHsBaSlicer（ABI 边界） | 捕获 `HsBa::Slicer::*Error`，填充 `success=false` + `error_message`；对 `CancelledError` 额外置 `cancelled=true` 并加 `"Cancelled: "` 前缀 |
| restapi（HTTP 边界） | 捕获 `std::exception`，将 `what()` 写入 JSON 响应；`catch (...)` 兜底未类型化异常 |

### 3.3 取消语义
`CancelledError` 被单独识别：`file_transfer_pipeline.cpp` 中 catch 后设置 `result.cancelled = true`，并将消息以 `"Cancelled: "` 开头，注释说明这是 C ABI 调用方区分取消与失败的约定（因为 C 结果结构没有 status 字段）。

### 3.4 返回值约定
- 同步流水线返回 `struct CustomOutput` 等，字段含 `success`、`cancelled`、`error`、`elapsed_seconds`。
- C ABI 结果结构（如 `HsBaFileTransferPipelineResult_t`）通过 `HsBaFree*PipelineResult` 释放 `error_message` 等动态内存。

## 4. 观察到的约束与规则

- **ABI 层不得向 C 调用方抛出 C++ 异常**：`DllHsBaSlicer/*` 中所有对 LibHsBaSlicer 的调用均被 try/catch 包裹，只读取 `success`/`error_message` 字段（见 `file_transfer_pipeline.cpp`、`model_preprocess.cpp`、`geometry_store.cpp`、`job_control.cpp` 的大量 `catch (const HsBa::Slicer::RuntimeError&)`）。
- **流水线函数对预期失败不抛异常**：`LibHsBaSlicer/Pipeline/*.hpp` 头文件中每个工艺的 Run 函数注释都写明 “Never throws for expected failures; they surface through success/cancelled/error”，这是设计契约而非运行时强制。
- **取消与失败通过字符串前缀区分**：`file_transfer_pipeline.cpp` 注释明确指出 `"Cancelled: "` 前缀是 C ABI 调用方区分取消与失败的约定，因为 C 结果结构没有 status 字段。
- **REST 层使用 `std::exception` 兜底**：`restapi/job_manager.cpp`、`routes.cpp`、`main.cpp` 均以 `catch (const std::exception&)` 捕获，`job_manager.cpp` 还追加 `catch (...)` 捕获未知异常。
- **部分代码直接抛标准异常**：`cipher/encoder.cpp`、`cipher/encrypt.cpp` 抛 `std::invalid_argument`；`meshmodel/CgalModel.cpp` 抛 `std::runtime_error`，这些由上层 `RuntimeError` 捕获链间接处理。
- **测试中模拟异常**：`tests/Coroutines/coroutines_test.cpp`、`tests/RestApi/job_manager_test.cpp` 等使用 `throw std::runtime_error(...)` 验证异常路径。

## 5. 缺失与不一致

- 并非所有抛错点都使用 `HsBa::Slicer::*Error`：cipher/meshmodel 模块直接使用 `std::invalid_argument` / `std::runtime_error`，与自定义异常体系并存。
- 未发现统一的错误码枚举或 `ErrorCode` 类型；错误信息以人类可读字符串为主。
- 没有全局异常处理器或中间件；错误处理分散在各 try/catch 块中。