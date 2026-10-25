---
kind: error_handling
name: HsBa::Slicer Exception Hierarchy and Pipeline Error Propagation
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - restapi/job_manager.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
    - fileoperator/LuaAdapter.cpp
---

## Approach

HsBaSlicer uses **C++ exceptions** as its primary error-propagation mechanism. There is no `Result<T>`/`std::expected` pattern, no sentinel error codes returned from core slicing functions, and no `panic`/`recover` (the project is C++, not Rust). The only non-exception path is the REST API's async job queue, which converts thrown exceptions into a `JobState::Failed` status with an `error_message` string.

## Core exception hierarchy

All custom errors live in `base/error.hpp` under namespace `HsBa::Slicer`. The hierarchy is:

```
std::runtime_error
└── RuntimeError
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

Each class is a thin wrapper around `std::runtime_error`, exposing constructors for `const std::string&` / `const char*` and re-exporting `what()` via `using`. The file `base/error.cpp` is present but empty — all definitions are inline in the header.

## Where errors are thrown

Throw sites are concentrated in two areas:

1. **Lua bridge code** (`2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp`, `fileoperator/LuaAdapter.cpp`) throws `RuntimeError` when Lua state creation, script loading, function lookup, or function invocation fails. These are user-scriptable entry points, so the error messages carry the underlying Lua error text.
2. **Optional-feature guards**: `2D/FloatPolygons.cpp` throws `NotSupportedError("Need FreeType support")` and `RuntimeError("Failed to initialize FreeType library")` when optional dependencies are missing.
3. **OpenCV interop**: `2D/ImageToPolygons.cpp` catches `cv::Exception` at three call sites and wraps it in `RuntimeError` before rethrowing.

The deeper slicing algorithms (mesh slicing, path generation, support generation) generally do not throw; they return boolean success flags on their result structs (e.g. `success` + `error_message` fields), leaving callers to decide whether to propagate or convert to an exception.

## Catch-and-convert boundary: DllHsBaSlicer pipelines

Every pipeline implementation in `DllHsBaSlicer/` (`fdm_pipeline.cpp`, `lom_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`, `file_transfer_pipeline.cpp`) wraps its coroutine body in a single `catch (const RuntimeError& e)` block that sets `result.success = false` and prefixes the message with `"Pipeline error: "`. This is the C ABI boundary: the exported `HsBaRunXxxPipeline` functions never let exceptions cross into C callers; instead they populate the `HsBaFdmPipelineResult_t` (and analogous structs) with `success = false` and an `error_message` allocated by the pipeline and later freed via `HsBaFreePipelineResult`.

## REST API job queue isolation

`restapi/job_manager.cpp::Execute` is the outermost safety net. It calls the runner (which invokes the C ABI pipeline) inside:

```cpp
try { outcome = runner_(...); }
catch (const std::exception& e) { failed_exception = true; thrown = e.what(); }
catch (...) { failed_exception = true; thrown = "unknown pipeline error"; }
```

Any exception — including uncaught ones from the pipeline layer — is captured, the job is marked `JobState::Failed`, and `job->error_message` is set. The worker thread itself never crashes; the job simply becomes observable via `JobManager::Status`.

## Conventions observed

- Custom errors derive from `std::runtime_error`; there is no parallel `ErrorCode` enum or `Result<T>` type used in the slicing core.
- Error classes are grouped in a single header `base/error.hpp` under `namespace HsBa::Slicer`.
- Lua-facing code throws `RuntimeError` with human-readable messages that include the underlying Lua/OpenCV error text.
- Optional third-party features are signaled via `NotSupportedError` rather than returning a special value.
- The C ABI layer (`DllHsBaSlicer/*_pipeline.cpp`) is the sole place where `RuntimeError` is caught and converted to a structured `success`/`error_message` result; internal modules do not catch these exceptions themselves.
- The REST API adds one more catch-all layer around the entire async execution, converting any exception into a failed job with a stringified message.
- No `try`/`catch` blocks appear in the pure algorithmic layers (e.g. `support/`, `paths/`, `meshmodel/`); errors either return booleans or bubble up as exceptions.
- No `setjmp`/`longjmp`, no `std::terminate` overrides, no global `std::set_unhandled_exception_handler` was found.