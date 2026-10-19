---
kind: error_handling
name: Exception-Based Error Handling with RuntimeError Hierarchy and C-ABI Boundary Wrapping
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
    - fileoperator/LuaAdapter.cpp
---

## Approach

HsBaSlicer uses **C++ exceptions** as its primary error-handling mechanism, centered on a small hierarchy of custom exception classes defined in `base/error.hpp`. There is no `std::expected`, `Result<T>`, or sentinel-error enum pattern used for internal code paths; instead, domain-specific failures propagate via thrown exceptions that are caught at C/FFI boundaries.

## Core Types

`base/error.hpp` defines a single base class:

```
class RuntimeError : public std::runtime_error { ... };
```

All other error types derive from it (one-level inheritance):

| Type | Meaning |
|---|---|
| `OutOfRangeError` | Index/bounds violations |
| `InvalidArgumentError` | Bad function arguments |
| `IOError` | File I/O failures |
| `NotImplementedError` | Stub/unimplemented features |
| `NullValueError` | Null-pointer / missing value |
| `NotSupportedError` | Optional feature not available (e.g. FreeType) |
| `NotFoundError` | Missing resource |
| `AlreadyExistsError` | Duplicate registration |
| `PermissionDeniedError` | Access control failure |
| `TimeoutError` | Timed-out operation |
| `InterruptedError` | Interrupted operation |
| `CancelledError` | Cancelled operation |
| `OutOfMemoryError` | Allocation failure |

Each subclass forwards all constructors and `what()` through `using` declarations — there is no additional state attached to any error type. The only implementation file (`base/error.cpp`) is an empty translation unit whose sole purpose is to satisfy the build system's expectation of a `.cpp` companion.

## Throwing Conventions

Throw sites observed across the codebase follow two patterns:

1. **Domain-specific subclasses** for clearly categorized failures, e.g. `throw NotSupportedError("Need FreeType support")` in `2D/FloatPolygons.cpp`.
2. **Generic `RuntimeError`** carrying a human-readable message string, especially for Lua interop failures (`"Failed to create Lua state"`, `"Lua function not found: ..."`, `"Error calling Lua function: ..."`) in `2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp`, and `fileoperator/LuaAdapter.cpp`.

Third-party exceptions are also caught directly where relevant — notably `cv::Exception` from OpenCV in `2D/ImageToPolygons.cpp`.

## C-ABI Boundary Strategy

The `DllHsBaSlicer/` layer is the explicit boundary between the C++ core and the exported C API. Every exported function wraps its body in `try { ... } catch (const HsBa::Slicer::RuntimeError&) { ... }` and converts the exception into a C-friendly return value:

- Functions returning pointers return `nullptr` on error (`HsBaLoadModel`, `HsBaGetModel`).
- Functions returning `int` return `0` on error (`HsBaContainsModel`, `HsBaTranslateModel`, `HsBaRotateModel`, `HsBaScaleModel*`, `HsBaGetModelInfo`).
- Coroutine-based pipeline functions (`fdm_pipeline.cpp`, `file_transfer_pipeline.cpp`, `lom_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`) set `result.success = false` and populate `result.error_message` with `"Pipeline error: " + e.what()`.

A comment in `model_preprocess.cpp` explicitly documents this rule:

> `// NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow RuntimeError, do not propagate across C boundary`

This means the C ABI is guaranteed never to throw across the boundary — callers in C/C#/Python/Java must treat the returned struct fields as the sole error signal.

## Pipeline-Level Error Propagation

Inside the async pipeline coroutine (e.g. `RunPipelineAsync` in `fdm_pipeline.cpp`), the entire pipeline body is wrapped in one `catch (const RuntimeError& e)` block that sets `result.success = false` and `result.error_message = std::string("Pipeline error: ") + e.what()`. This centralizes error capture so that any stage throwing a `RuntimeError` (or derived type) short-circuits the pipeline and surfaces the message through the same `InternalResult` → `ToCResult` path.

## What Is NOT Used

- No `std::expected`, `std::variant`, or `Result<T>` type for error propagation inside the C++ core.
- No `setjmp`/`longjmp` or `std::terminate` overrides.
- No global error code registry or errno-style pattern.
- No structured logging integration tied to the error types themselves (logging is separate via the `logger/` module).
- No `panic!`-style macro or recover mechanism — pure C++ exceptions.

## Key Files

- `base/error.hpp` — complete exception hierarchy definition.
- `base/error.cpp` — empty translation unit for the header-only types.
- `DllHsBaSlicer/model_preprocess.cpp` — canonical example of per-`HSBA_SLICER_API` try/catch wrapping.
- `DllHsBaSlicer/fdm_pipeline.cpp` — pipeline coroutine catching `RuntimeError` and populating `result.error_message`.
- `2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp`, `fileoperator/LuaAdapter.cpp` — Lua interop throw sites.
- `2D/FloatPolygons.cpp` — example of using a domain-specific subclass (`NotSupportedError`).
- `2D/ImageToPolygons.cpp` — example of catching third-party `cv::Exception`.