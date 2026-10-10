---
kind: error_handling
name: 基于 std::runtime_error 派生类的异常体系与 C ABI 边界吞异常模式
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
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - LibHsBaSlicer/Preprocess/model_preprocess.cpp
    - LibHsBaSlicer/Slice/mesh_slice.cpp
    - cipher/LuaAdapter.cpp
    - support/LuaAdapter.cpp
    - utils/LuaAnyObject.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
---

## 1. 采用的错误处理系统

HsBaSlicer 在 C++ 层统一使用 **C++ 异常（std::exception 及其派生类）** 作为错误传播机制，没有引入第三方错误码库、Result<T,E> 类型或 panic/recover 语义。核心定义位于 `base/error.hpp`，所有业务异常均继承自 `std::runtime_error`。

## 2. 关键文件与包

- `base/error.hpp`：定义全部自定义异常类型，位于 `HsBa::Slicer` 命名空间。
- `base/error.cpp`：仅包含头文件的翻译单元，无额外实现。
- `DllHsBaSlicer/model_preprocess.cpp`：C ABI 边界处对 `RuntimeError` 的统一捕获与返回值转换。
- `DllHsBaSlicer/fdm_pipeline.cpp`、`sla_pipeline.cpp`、`slm_pipeline.cpp`、`sls_pipeline.cpp`、`lom_pipeline.cpp`、`file_transfer_pipeline.cpp`：各流水线协程内捕获 `RuntimeError` 并写入 `InternalResult.error_message`。
- `LibHsBaSlicer/Preprocess/model_preprocess.cpp`、`LibHsBaSlicer/Slice/mesh_slice.cpp`：底层库捕获 `std::exception` 作为兜底。
- `cipher/LuaAdapter.cpp`、`support/LuaAdapter.cpp`、`utils/LuaAnyObject.cpp`：Lua 绑定层捕获 `std::exception` 以保护解释器。
- `2D/ImageToPolygons.cpp`、`2D/PolygonFill.cpp`、`2D/FloatPolygons.cpp`：大量抛出 `RuntimeError` 描述 Lua/OpenCV/FreeType 调用失败。

## 3. 异常类型层次

```text
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
```\n
每个子类都通过 `using ...::...` 复用基类构造函数与 `what()`，不添加额外字段。错误信息通过构造时传入的 `std::string` / `const char*` 表达。

## 4. 架构与约定

### 4.1 内部 C++ 层：向上抛异常

几何、切片、路径生成等核心逻辑直接 `throw RuntimeError(...)` 或更具体的派生类（如 `NotImplementedError("Need FreeType support")`）。这些函数签名不返回 `std::optional` / `Result`，也不设置 errno；调用方负责 try/catch。

### 4.2 C ABI 边界：吞异常并转为返回值

`DllHsBaSlicer/model_preprocess.cpp` 中每个 `HSBA_SLICER_API` 函数遵循同一模板：

- 参数为空指针时立即返回默认值（`nullptr` / `0`）。
- 主体逻辑包裹在 `try { ... } catch (const HsBa::Slicer::RuntimeError&) { ... }` 中。
- 返回指针的函数捕获后返回 `nullptr`；返回整型的函数返回 `0`；返回 void 的函数空 catch（注释明确标注 `NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow RuntimeError, do not propagate across C boundary`）。

这保证了 C ABI 不会将 C++ 异常泄漏给外部调用者。

### 4.3 流水线层：捕获并序列化到结果结构

协程内的流水线实现（如 `fdm_pipeline.cpp`）捕获 `RuntimeError`，将 `e.what()` 前缀化为 `"Pipeline error: "` 后写入 `result.error_message`，并将 `result.success = false`。成功路径则设置 `success = true`。最终通过 `ToCResult` 转换为 C 结构体，由 `HsBaFreePipelineResult` 释放 `error_message` 指向的堆内存。

### 4.4 第三方异常兜底

部分底层模块捕获 `std::exception` 作为最后防线：

- `LibHsBaSlicer/Preprocess/model_preprocess.cpp`、`LibHsBaSlicer/Slice/mesh_slice.cpp`：捕获 `std::exception& e` 并记录日志。
- `cipher/LuaAdapter.cpp`、`support/LuaAdapter.cpp`、`utils/LuaAnyObject.cpp`：Lua 绑定层捕获 `std::exception` 避免崩溃解释器。
- `meshmodel/CgalModel.cpp`：捕获 CGAL 抛出的 `std::exception`。
- `pointcloud/OpenVdbModel.cpp`：捕获 OpenVDB 异常。
- `2D/ImageToPolygons.cpp`：单独捕获 `cv::Exception`（OpenCV）。

### 4.5 测试层

测试代码也广泛使用 `catch (const std::exception& e)` 配合 Boost.Test 断言验证异常行为（见 `tests/Coroutines/`、`tests/ParamStore/`、`tests/Models/cgal_model_test.cpp` 等）。

## 5. 观察到的约定与约束

- **所有业务异常必须继承 `HsBa::Slicer::RuntimeError`**：现有 14 个派生类覆盖了范围越界、参数非法、IO、未实现、空值、不支持、不存在、已存在、权限拒绝、超时、中断、取消、内存不足等场景。
- **C ABI 函数不得让 C++ 异常跨越边界**：`model_preprocess.cpp` 中的注释显式声明此规则，且每个导出函数都按相同模式捕获 `RuntimeError`。
- **流水线协程将异常消息序列化为 `error_message` 字符串**：这是 `InternalResult` 结构的既定契约，供上层 C API 暴露。
- **Lua 绑定层统一捕获 `std::exception`**：防止 C++ 异常破坏 Lua 状态机。
- **未发现 panic/recover、错误码枚举、Result<T,E> 或中间件式错误处理**：整个仓库依赖 C++ 异常 + 显式 try/catch 的组合。
- **未找到全局异常处理器或统一的日志化包装器**：异常在捕获点就地处理（返回默认值、填充 error_message 或记录日志），没有集中式 middleware。
