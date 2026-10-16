---
kind: error_handling
name: Exception-Based Error Handling with HsBa::Slicer Runtime Exceptions
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - LibHsBaSlicer/Preprocess/model_preprocess.cpp
    - LibHsBaSlicer/Slice/mesh_slice.cpp
    - fileoperator/LuaAdapter.cpp
    - cipher/LuaAdapter.cpp
    - support/LuaAdapter.cpp
    - samples/FDM/main.cpp
---

## Overview

HsBaSlicer uses **C++ exception-based error handling** throughout its C++20 codebase. There is no sentinel-error-return pattern, no `Result<T>` type, and no `panic`/`recover` mechanism (this is not Rust). Errors are represented as a hierarchy of custom exception classes derived from `std::runtime_error`, thrown by library code and caught at API boundaries (especially the C ABI layer) where they are converted into structured result objects for cross-language consumers.

## Core Exception Types

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

Each class is minimal — it only exposes constructors taking `const std::string&` / `const char*` and re-exports `what()` via `using std::runtime_error::what;`. The companion `base/error.cpp` is intentionally empty because all definitions are inline in the header.

## Throwing Conventions

- Internal validation failures throw the most specific subclass: e.g. `InvalidArgumentError` for bad parameters, `OutOfRangeError` for index bounds, `NotImplementedError` for unimplemented virtuals, `IOError` for file I/O failures, `NotSupportedError` when optional features (e.g. FreeType) are unavailable.
- Generic operational failures (Lua initialization, script loading, function invocation) throw `RuntimeError` with a human-readable message string.
- Third-party libraries that do not use the custom hierarchy (CGAL, OpenCASCADE, Eigen, OpenSSL) throw `std::exception` or their own types; these are caught and wrapped before crossing module boundaries.

## Catching & Boundary Conversion

The primary boundary is the C ABI export layer in `DllHsBaSlicer/`. Every pipeline entry point (`fdm_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `lom_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`, `file_transfer_pipeline.cpp`) wraps its body in `try { ... } catch (const RuntimeError& e) { ... }` and converts the exception into a `HsBaFdmPipelineResult_t`-style struct whose `success` field is false and `error_message` carries `e.what()`. This is what the sample apps in `samples/*/main.cpp` consume — they never see exceptions directly; they inspect `result.success` and `result.error_message`.

The same pattern appears in Lua adapter layers (`fileoperator/LuaAdapter.cpp`, `cipher/LuaAdapter.cpp`, `support/LuaAdapter.cpp`, `LibHsBaSlicer/Extends/lua_pipeline.cpp`, `LibHsBaSlicer/Path/sls_export.cpp`, `LibHsBaSlicer/Path/waam_export.cpp`), which catch `RuntimeError` and translate it into Lua error strings so Lua scripts can report back to the host.

For third-party exceptions crossing the `extern "C"` boundary, explicit comments document the rule:

- `LibHsBaSlicer/Preprocess/model_preprocess.cpp`: `// Standard_Failure, or even a raw std::runtime_error). Such types must never propagate across the extern "C" Dll boundary.`
- `LibHsBaSlicer/Slice/mesh_slice.cpp`: `// Standard_Failure, or even a raw std::runtime_error). Such types must never propagate across the extern "C" boundary.`
- `meshmodel/CgalModel.cpp`: `// CGAL operations may throw std::exception or derived types`

These catch blocks wrap the third-party exception into a `RuntimeError` (or set an error flag) before returning.

## Sample App Behavior

The FDM sample (`samples/FDM/main.cpp`) demonstrates the expected consumer pattern: call `HsBaRunFdmPipeline(...)`, check `result.success`, and if false log `result.error_message`. It does not use `try/catch` around pipeline calls — exceptions are already swallowed at the C boundary.

## What Is NOT Used

- No sentinel return codes or `errno`-style patterns in C++ code.
- No `std::expected` / `Result<T>` monad.
- No `setjmp`/`longjmp` or `catch(...)` broad catches used as control flow (only narrow `RuntimeError` / `std::exception` handlers).
- No `throw` inside destructors observed in the scanned files.
- No `assert`-based error signaling for runtime paths (assertions are compile-time checks only).

## Key Files

- `base/error.hpp` — exception class hierarchy definition
- `base/error.cpp` — translation unit (empty)
- `DllHsBaSlicer/fdm_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `lom_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`, `file_transfer_pipeline.cpp` — C ABI boundary catch sites
- `DllHsBaSlicer/model_preprocess.cpp` — repeated `catch (const HsBa::Slicer::RuntimeError&)` wrappers
- `LibHsBaSlicer/Preprocess/model_preprocess.cpp`, `LibHsBaSlicer/Slice/mesh_slice.cpp` — third-party exception guards with explanatory comments
- `fileoperator/LuaAdapter.cpp`, `cipher/LuaAdapter.cpp`, `support/LuaAdapter.cpp` — Lua-boundary conversion
- `samples/FDM/main.cpp` — example consumer pattern using `result.success` / `result.error_message`