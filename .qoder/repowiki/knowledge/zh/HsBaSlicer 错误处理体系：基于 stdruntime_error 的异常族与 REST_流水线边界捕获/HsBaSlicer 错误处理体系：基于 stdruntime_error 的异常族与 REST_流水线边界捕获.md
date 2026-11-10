---
kind: error_handling
name: HsBaSlicer 错误处理体系：基于 std::runtime_error 的异常族与 REST/流水线边界捕获
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - restapi/serialize_value.hpp
    - restapi/routes.cpp
    - restapi/job_manager.cpp
    - restapi/main.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/geometry_store.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - DllHsBaSlicer/job_control.cpp
    - utils/LuaNewObject.hpp
    - utils/LuaAnyObject.cpp
    - 2D/LuaAdapter.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
    - fileoperator/LuaAdapter.cpp
    - support/LuaAdapter.cpp
---

## 1. 采用的方案

HsBaSlicer 在 C++ 层统一使用 C++ 异常作为错误传播机制，没有引入第三方错误码库。核心异常族定义在 `base/error.hpp`，全部继承自 `std::runtime_error`；业务代码通过 `throw` 抛出这些类型，调用方用 `try/catch` 捕获。

REST API 层（`restapi/`）将底层异常转换为统一的 JSON/XML 错误体 `ErrorView { code, message, detail }`，由 `ReplyError` 辅助函数序列化输出。

Lua 互操作层由于 `lua_error` 走 `longjmp` 而非 C++ 栈展开，项目内多处注释明确区分了 MSVC x64（RtlUnwind 触发析构）与 POSIX longjmp（不触发析构）的差异，并通过 RAII 包装对象（如 `utils/LuaNewObject.hpp`、`utils/LuaAnyObject.cpp`、`fileoperator/LuaAdapter.cpp`）保证跨平台安全。

## 2. 关键文件与包

- `base/error.hpp` / `base/error.cpp` — 自定义异常族定义与空翻译单元
- `DllHsBaSlicer/fdm_pipeline.cpp` — 流水线协程中 `catch (const CancelledError&)`、`catch (const RuntimeError&)`、`catch (const std::exception&)` 的典型用法
- `DllHsBaSlicer/file_transfer_pipeline.cpp`、`lom_pipeline.cpp`、`geometry_store.cpp`、`model_preprocess.cpp`、`job_control.cpp` — 其他流水线对同一异常族的捕获模式
- `restapi/routes.cpp`、`restapi/job_manager.cpp`、`restapi/main.cpp` — HTTP 端点层的 `catch (const std::exception&)` + `catch (...)` 兜底
- `restapi/serialize_value.hpp` — `ErrorView` 结构体与 `SerializeError` / `ReplyError`
- `utils/LuaNewObject.hpp`、`utils/LuaAnyObject.cpp`、`2D/LuaAdapter.cpp`、`fileoperator/LuaAdapter.cpp`、`support/LuaAdapter.cpp` — Lua longjmp 安全封装

## 3. 架构与约定

### 3.1 异常族层次

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
```

所有派生类仅通过 `using` 复用基类构造函数与 `what()`，未添加额外字段。这是仓库中唯一的自定义异常定义位置。

### 3.2 抛出位置

- 2D 几何与 Lua 脚本交互路径广泛使用 `throw RuntimeError("Failed to ...")`（见 `2D/ImageToPolygons.cpp`、`2D/PolygonFill.cpp`、`2D/FloatPolygons.cpp`），用于报告 FreeType/Lua 初始化失败、函数不存在、返回类型不符等。
- `2D/FloatPolygons.cpp:369` 使用 `throw NotSupportedError("Need FreeType support")` 表达可选依赖缺失。
- 流水线内部更多采用「设置 `result.success = false; result.error_message = ...; co_return`」的方式把可恢复错误转为返回值，仅在真正不可恢复时抛异常。

### 3.3 捕获与转换

- DllHsBaSlicer 层：对 `CancelledError` 单独分支以支持作业取消语义；对 `RuntimeError` 分支记录人类可读消息；其余 `std::exception` 作为兜底。
- restapi 层：每个路由处理器包裹 `try/catch (const std::exception& e)`，最终再 `catch (...)` 兜底，统一调用 `ReplyError(res, status, message, detail, fmt)` 生成 `ErrorView`。
- 预览渲染（`fdm_pipeline.cpp`）故意吞掉 `std::exception`，把三角化/投影失败降级为缩略图标记（"render_failed" / "mesh_too_large"），避免预览问题污染切片结果。

### 3.4 REST 错误响应格式

`restapi/serialize_value.hpp` 中的 `ErrorView` 是 JSON/XML 双格式共享视图：

```cpp
struct ErrorView {
    int code;           // HTTP 状态码
    std::string message; // 人类可读消息
    std::string detail;  // 可选上下文
};
```

`ReplyError` 将其序列化为 `{code,message,detail}` JSON 或 `<Error><code>...</code><message>...</message><detail>...</detail></Error>` XML。

### 3.5 Lua longjmp 特殊处理

多个文件注释明确指出 `lua_error` 走 `longjmp`，MSVC x64 会经 RtlUnwind 运行已注册析构而 POSIX 不会，因此不能简单依赖普通 RAII。典型做法：

- `utils/LuaNewObject.hpp`：构造后立刻 `raise`，确保 longjmp 前拥有对象已就绪。
- `2D/LuaAdapter.cpp`、`fileoperator/LuaAdapter.cpp`、`support/LuaAdapter.cpp`：在 `lua_error` 可能跳出的路径之前释放 polygon 容器等原生资源。
- `2D/ImageToPolygons.cpp:249`：直接使用 `setjmp(png_jmpbuf(png))` 配合 libpng 的错误跳转缓冲区。

## 4. 观察到的约定与约束

- 自定义异常必须继承 `HsBa::Slicer::RuntimeError`：`base/error.hpp` 是唯一扩展点，所有派生类都通过 `using std::runtime_error::runtime_error` 复用基类构造。
- Lua 互操作路径禁止依赖 C++ 栈展开：多处注释（`utils/LuaNewObject.hpp`、`utils/LuaAnyObject.cpp`、`2D/LuaAdapter.cpp`、`fileoperator/LuaAdapter.cpp`、`support/LuaAdapter.cpp`）显式说明 `lua_error` 走 `longjmp`，需要显式 RAII 包装。
- clang-tidy 禁用 setjmp/longjmp：`.clang-tidy` 与 `.clangd` 配置中包含 `-modernize-avoid-setjmp-longjmp`，但 `2D/ImageToPolygons.cpp` 中对 libpng 的 `setjmp` 属于绕过该规则的例外场景。
- REST 层统一错误视图：所有 HTTP 错误经由 `ReplyError` → `ErrorView` 序列化，客户端可通过 `code/message/detail` 三字段解析。
- 流水线层优先使用返回值表示可恢复错误：`RunPipelineAsync` 协程中常见 `result.success = false; result.error_message = ...; co_return`，异常仅用于真正故障路径（如模型加载失败、参数非法）。
- 预览渲染容错：`fdm_pipeline.cpp` 中三角化与轮廓投影的异常被捕获并降级为缩略图标签，不影响切片主流程。

## 5. 未覆盖领域

- 未发现全局 panic/recover 机制（无 `std::set_terminate` / `std::set_unexpected` 注册）。
- 未发现统一的错误码枚举或 `Result<T,E>` 类型（除流水线协程局部使用的 `InternalResult` 结构外）。
- 单元测试中未见针对自定义异常的断言用例（测试目录中未见 `*_test.cpp` 直接 `EXPECT_THROW(RuntimeError,...)`）。