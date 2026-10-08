---
kind: error_handling
name: 基于 std::runtime_error 派生类的异常体系与 C ABI 边界捕获
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - LibHsBaSlicer/Extends/LuaCommonTypes.cpp
    - LibHsBaSlicer/Extends/lua_pipeline.cpp
    - fileoperator/LuaAdapter.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
---

## 1. 采用的错误处理方案

HsBaSlicer 在 C++ 层统一使用 **C++ 异常**（`throw`/`catch`）作为错误传播机制，没有使用 `std::expected`、`Result<T>`、返回码或 `errno`。核心异常类型集中在 `base/error.hpp`，全部继承自 `std::runtime_error`，通过 `what()` 携带人类可读的错误消息。

跨语言边界（DLL 导出层 `DllHsBaSlicer/`、Lua 绑定 `LibHsBaSlicer/Extends/`、`fileoperator/LuaAdapter.cpp`）则通过 `catch (const RuntimeError&)` 将 C++ 异常转换为结构体字段（`success=false` + `error_message`），再经由 C ABI 返回给宿主语言。

日志子系统（`logger/logger.hpp`）是独立的基础设施，用于记录运行信息，不承载错误语义；错误本身走异常通道。

## 2. 关键文件与包

- `base/error.hpp` / `base/error.cpp`：定义所有自定义异常类。
- `DllHsBaSlicer/*_pipeline.cpp`、`DllHsBaSlicer/model_preprocess.cpp`：C ABI 导出函数中统一 catch `RuntimeError` 并填充 `HsBa*PipelineResult_t.error_message`。
- `LibHsBaSlicer/Extends/LuaCommonTypes.cpp`、`LibHsBaSlicer/Extends/lua_pipeline.cpp`：Lua 桥接层 catch `RuntimeError` 后转成 Lua error。
- `fileoperator/LuaAdapter.cpp`：大量 `catch (const RuntimeError& e)` 包裹 Lua 调用。
- `2D/ImageToPolygons.cpp`、`2D/PolygonFill.cpp`、`2D/FloatPolygons.cpp`：业务代码中抛出 `RuntimeError` / `NotSupportedError`。

## 3. 异常类型体系

`base/error.hpp` 在命名空间 `HsBa::Slicer` 下定义了如下层次：

```
std::runtime_error
└── HsBa::Slicer::RuntimeError          // 根异常
    ├── OutOfRangeError                 // 越界
    ├── InvalidArgumentError            // 非法参数
    ├── IOError                         // I/O 失败
    ├── NotImplementedError             // 未实现
    ├── NullValueError                  // 空值
    ├── NotSupportedError               // 不支持
    ├── NotFoundError                   // 找不到
    ├── AlreadyExistsError              // 已存在
    ├── PermissionDeniedError           // 权限拒绝
    ├── TimeoutError                    // 超时
    ├── InterruptedError                // 中断
    ├── CancelledError                  // 取消
    └── OutOfMemoryError                // 内存不足
```

每个子类仅通过 `using std::runtime_error::runtime_error;` 和 `using std::runtime_error::what;` 复用基类构造与 `what()`，没有额外成员。因此错误分类完全依赖类型区分，而非错误码枚举。

## 4. 抛出的位置与模式

- 业务层（如 `2D/ImageToPolygons.cpp`、`2D/PolygonFill.cpp`、`2D/FloatPolygons.cpp`）直接 `throw RuntimeError("...")` 或 `throw NotSupportedError(...)`。
- 未见对 `InvalidArgumentError`、`NotFoundError`、`TimeoutError` 等其它子类型的广泛使用——当前代码以 `RuntimeError` 为主，辅以 `NotSupportedError`（FreeType 缺失场景）。
- 第三方异常（如 OpenCV 的 `cv::Exception`）在 `2D/ImageToPolygons.cpp` 中被单独 `catch`，未被包装为 `HsBa::Slicer::RuntimeError`。

## 5. 捕获与传播约定

### 5.1 C ABI 边界（`DllHsBaSlicer/`）

每个流水线入口（FDM/SLA/SLS/SLM/LOM/TDP/WAAM）内部均有一个 `try { ... } catch (const RuntimeError& e) { result.success = false; result.error_message = "Pipeline error: " + e.what(); }` 块，把异常吞掉并转为结果结构体的布尔标志 + 字符串消息。这是该仓库中最稳定、最一致的错误收敛点。

### 5.2 Lua 桥接层（`LibHsBaSlicer/Extends/`、`fileoperator/LuaAdapter.cpp`）

Lua 侧同样统一 `catch (const RuntimeError& e)`，将 `e.what()` 传给 Lua 的 `luaL_error` 或类似接口，使脚本能感知 C++ 异常。

### 5.3 上层调用方

测试与示例代码（`samples/`、`tests/`）通常直接让 `RuntimeError` 向上传播，由 Boost.Test 或主程序处理；没有在更高层做统一的异常到日志的转换。

## 6. 观察到的约定与约束

- **所有自定义错误必须继承 `HsBa::Slicer::RuntimeError`**（即间接继承 `std::runtime_error`），并通过 `what()` 提供描述性消息。新增错误类型应在 `base/error.hpp` 中声明，而不是在各模块自行定义新的异常类。
- **C ABI 导出函数不得让 C++ 异常逃逸到非 C++ 调用方**。`DllHsBaSlicer/*_pipeline.cpp` 中的 `catch (const RuntimeError&)` 是强制性的安全网，将异常转换为 `HsBa*PipelineResult_t.success` + `error_message`。
- **Lua 绑定函数需捕获 `RuntimeError` 并转成 Lua error**，避免 C++ 异常穿透到 Lua VM。
- **业务逻辑优先抛具体异常类型**（如 `NotSupportedError` 表示 FreeType 缺失），但当前实际使用仍以 `RuntimeError` 为主，其他子类尚未被广泛采用。
- **第三方库异常（OpenCV 等）不被自动包装**，需要显式 `catch` 后再决定是否包装为 `RuntimeError`。

## 7. 缺失或不一致之处

- 未见全局异常处理器（如 `std::set_uncaught_exception_handler`）或统一的异常→日志转换层。
- 部分 `catch` 块只匹配 `RuntimeError`，未覆盖其子类（如 `IOError`、`TimeoutError`），这意味着这些子类会按多态被 `RuntimeError` 引用捕获，行为正确但语义上不够精确。
- 没有统一的错误码枚举或结构化错误对象；错误诊断完全依赖 `what()` 字符串拼接，不利于机器解析。
- `base/error.cpp` 仅为占位编译单元，所有类型定义都内联在头文件中，便于模板/constexpr 使用但也导致头文件膨胀。

## 8. 总结

HsBaSlicer 的错误处理以 `base/error.hpp` 定义的 `HsBa::Slicer::RuntimeError` 及其派生类为核心，通过 C++ 异常在 C++ 层内传播，并在 C ABI 与 Lua 边界处统一捕获并转换为返回值字段或 Lua error。这是一种简单直接的策略，边界处的 catch 块构成了事实上的错误收敛点，但尚未形成完整的错误分类与结构化错误报告体系。