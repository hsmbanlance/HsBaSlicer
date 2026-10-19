---
kind: error_handling
name: 基于 HsBa::Slicer::RuntimeError 异常体系的跨边界错误处理
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - fileoperator/RemoteExecutor.cpp
    - fileoperator/bit7z_zipper.cpp
    - fileoperator/bit7z_unzipper.cpp
    - fileoperator/LuaAdapter.cpp
    - 2D/ImageToPolygons.cpp
---

## 1. 采用的体系

HsBa Slicer 采用 **C++ 标准异常 + 自定义异常类** 的错误处理模式，核心定义在 `base/error.hpp`：

- 根类型 `HsBa::Slicer::RuntimeError` 继承自 `std::runtime_error`。
- 所有业务异常均派生自 `RuntimeError`，包括：`OutOfRangeError`、`InvalidArgumentError`、`IOError`、`NotImplementedError`、`NullValueError`、`NotSupportedError`、`NotFoundError`、`AlreadyExistsError`、`PermissionDeniedError`、`TimeoutError`、`InterruptedError`、`CancelledError`、`OutOfMemoryError`。
- 每个子类仅通过 `using` 复用基类构造函数与 `what()`，不引入额外状态；错误信息以字符串形式携带（例如 `IOError(std::format("..."))`）。
- 该头文件位于 `base/` 基础库中，被切片核心、2D 几何、文件操作等上层模块直接 include。

此外，`fileoperator/param_codec.cpp`、`param_convert.cpp`、`param_reflect.cpp` 中还使用第三方 SQL 适配层抛出的 `SQL::SQLAdapterInvalidArgumentError`，属于跨库异常混用场景。

## 2. 关键文件与位置

| 文件 | 角色 |
|---|---|
| `base/error.hpp` / `base/error.cpp` | 自定义异常类定义与空实现单元 |
| `DllHsBaSlicer/model_preprocess.cpp` | C ABI 边界统一捕获 `HsBa::Slicer::RuntimeError` 并返回空指针/0 值 |
| `DllHsBaSlicer/{fdm,sla,slm,sls,lom,tdp,waam,file_transfer}_pipeline.cpp` | 各工艺管线入口捕获 `RuntimeError` 并转为失败码 |
| `fileoperator/RemoteExecutor.cpp` | 网络/文件 I/O 抛出 `IOError` / `InvalidArgumentError` |
| `fileoperator/bit7z_zipper.cpp` / `bit7z_unzipper.cpp` | ZIP/7z 封装抛出 `IOError` / `InvalidArgumentError` / `NotSupportedError` |
| `2D/ImageToPolygons.cpp` / `PolygonFill.cpp` | Lua 脚本调用路径抛出 `RuntimeError` |

## 3. 架构与约定

### 3.1 分层传播策略

- **核心库层（LibHsBaSlicer / base）**：正常抛出具体异常类型（如 `IOError`、`InvalidArgumentError`），由调用方决定如何处理。
- **C ABI 动态库层（DllHsBaSlicer）**：所有 `HSBA_SLICER_API` 函数在 try/catch 中包裹底层调用，捕获 `HsBa::Slicer::RuntimeError` 后不向 C 调用方传播，而是返回空指针或 0 表示失败。典型模式见 `model_preprocess.cpp` 中 `HsBaLoadModel`、`HsBaTranslateModel`、`HsBaGetModelInfo` 等函数。
- **Lua 绑定层**：部分 Lua 适配器直接抛出 `std::runtime_error`（如 `fileoperator/LuaAdapter.cpp` 中的 ParamStore 未知表错误），未走自定义异常体系，属于遗留差异点。

### 3.2 C ABI 边界的“吞异常”约定

`model_preprocess.cpp` 中多处出现注释 `// NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow RuntimeError, do not propagate across C boundary`，明确表达了规则：**C ABI 函数不得让 C++ 异常跨越边界**，一律转换为返回值语义（`nullptr` / `0` / `false`）。这是本仓库对 C ABI 错误传播的强约束来源。

### 3.3 异常 vs 返回值
n
- 纯 C++ API：优先抛异常，调用方显式 catch。
- C ABI API：禁止抛异常，使用返回值 + 可选 out 参数。
- 测试代码（`tests/Preprocess/model_loader_test.cpp`）中对 `catch (...)` 的使用表明，测试侧也会兜底捕获任意异常以保证用例继续执行。

### 3.4 第三方异常混合

- OpenCV 异常 `cv::Exception` 在 `2D/ImageToPolygons.cpp` 中被直接捕获。
- SQL 适配层异常 `SQL::SQLAdapterInvalidArgumentError` 在 `fileoperator/` 下被抛出。
- 这些不属于 `HsBa::Slicer` 命名空间，说明自定义异常体系并未完全覆盖所有依赖库的异常。

## 4. 观察到的约定与约束

- **异常层次单一**：所有业务异常继承自 `RuntimeError`，没有更细粒度的领域异常树（如 `SliceError`、`PathError` 等），分类靠异常子类名区分。
- **消息即 what()**：异常对象不保存结构化字段，错误上下文全部编码进 `std::string` 传给基类构造。
- **C ABI 边界必须捕获**：`DllHsBaSlicer` 下每个导出函数都包裹 try/catch `HsBa::Slicer::RuntimeError`，并通过注释标注“不要跨 C 边界传播”。
- **Lua 层例外**：`fileoperator/LuaAdapter.cpp` 仍直接 throw `std::runtime_error`，未包装为 `HsBa::Slicer::RuntimeError`，是现有不一致之处。
- **无全局错误码/Result 类型**：未发现类似 Rust `Result<T,E>` 或 Go error 值的替代结构；错误传播主要依赖异常。
- **无 panic/recover 机制**：C++ 项目不使用 `setjmp/longjmp` 或类似 recover 模式；异常一旦未被捕获将导致进程终止。
- **测试侧宽松捕获**：单元测试中使用 `catch (...)` 作为兜底，保证单个测试失败不影响整个套件运行。