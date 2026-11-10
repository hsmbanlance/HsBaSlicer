---
kind: error_handling
name: Exception-Based Error Handling with HsBa::Slicer Custom Exceptions
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - restapi/routes.cpp
    - restapi/job_manager.cpp
    - restapi/config_bridge.cpp
    - restapi/pipeline_executor.cpp
    - LibHsBaSlicer/Floor/sla_floor.cpp
    - LibHsBaSlicer/Preprocess/model_preprocess.cpp
    - LibHsBaSlicer/Slice/mesh_slice.cpp
    - 2D/FloatPolygons.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
---

# Error Handling in HsBaSlicer

## Approach

The codebase uses **C++ exceptions** as its primary error propagation mechanism. There is no `Result<T,E>` type, no sentinel return codes for control flow, and no `panic`/`recover` pattern. Errors are represented by a small hierarchy of exception classes defined in the `base` module.

## Core Types

All custom errors live in `base/error.hpp` under namespace `HsBa::Slicer`. The hierarchy is:

```
std::runtime_error
└── RuntimeError (base)
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

Every class forwards its constructors to `std::runtime_error` and re-exports `what()` via `using std::runtime_error::what;`, so all messages are plain strings. There is no error-code enum, no structured payload, and no nested cause chain — the message string is the only diagnostic data.

## Throwing Conventions

- Domain-specific failures throw the most specific subclass: e.g. `InvalidArgumentError` for bad pipeline kind / null config in `restapi/config_bridge.cpp` and `restapi/pipeline_executor.cpp`; `NotSupportedError` when FreeType is unavailable (`2D/FloatPolygons.cpp`); `IOError` / `NotFoundError` / `AlreadyExistsError` / `PermissionDeniedError` for file-system operations.
- Generic failure points (Lua initialization, script loading, function invocation) throw `RuntimeError` with a descriptive message.
- Validation failures that are not argument-related (e.g. unsupported pipeline kind) still use `InvalidArgumentError` rather than a dedicated type, indicating the hierarchy is used more as a flat set of named categories than a deep taxonomy.

## Catching Conventions

Catch sites fall into two patterns:

1. **Per-type catches at library boundaries.** In `LibHsBaSlicer`, callers catch `const RuntimeError&` around calls into geometry/slicing/preprocess code and translate them into pipeline-level status values. Examples: `LibHsBaSlicer/Floor/sla_floor.cpp` (lines 526, 602), `LibHsBaSlicer/Preprocess/model_preprocess.cpp`, `LibHsBaSlicer/Slice/mesh_slice.cpp`. This is where the exception-based core is insulated from the C ABI consumers.

2. **Generic `catch(const std::exception&)` at HTTP route boundaries.** In `restapi/routes.cpp`, every request handler wraps deserialization and job submission in a try/catch that converts any thrown exception into an HTTP 400 response via `ReplyError(res, 400, "invalid config body", e.what(), outfmt)`. Similarly, `restapi/job_manager.cpp` has a bare `catch(...)` around the worker loop and a `catch(const std::exception& e)` around `stoull` parsing. `restapi/main.cpp` also has a top-level catch around server startup.

3. **Silent swallow in non-fatal paths.** `restapi/job_manager.cpp` line 175 catches `std::exception` from `std::stoull` on an externally-provided job id and simply ignores it, keeping `next_id_` unchanged. This is a deliberate fallback, not an error path.

## REST API Error Surface

The REST layer translates internal exceptions into HTTP responses using four helpers (`ReplyError`, `ReplyValue`) and a fixed mapping:

| Internal condition | HTTP status | Meaning |
|---|---:|---|
| Invalid config body / parse error | 400 | Client sent malformed input |
| Unknown or missing pipeline kind | 400 | Bad routing parameter |
| Job queue full | 429 | `max_jobs` reached |
| Upload pool full | 503 | Too many pending uploads |
| Job not found | 404 | No such job id |
| Job cancelled | 409 | Terminal state but not success |
| Job not finished successfully | 409 | Terminal failure |
| Output file not readable | 404 | Result disappeared between completion and download |

There is no global exception-to-HTTP middleware; each route handler performs its own try/catch around the relevant operation.

## What Is NOT Used

- No `std::expected` / `Result` types.
- No sentinel integer error codes for control flow (return-value error handling is limited to boolean flags like `JobManager::Status` returning `false`).
- No `try/finally` RAII wrappers around resources — resource cleanup is done explicitly (e.g. `DestroyConfig` before returning after a caught exception).
- No logging integration inside the exception classes themselves; diagnostics come from the `logger` module separately.
- No `set_terminate` / custom unhandled-exception hook observed.

## Key Files

- `base/error.hpp`, `base/error.cpp` — exception class definitions.
- `restapi/routes.cpp` — HTTP boundary catch-all converting exceptions to 4xx/5xx responses.
- `restapi/job_manager.cpp` — worker loop catch-all and per-operation error returns.
- `restapi/config_bridge.cpp`, `restapi/pipeline_executor.cpp` — throwing sites for invalid arguments and protocol errors.
- `LibHsBaSlicer/Floor/sla_floor.cpp`, `LibHsBaSlicer/Preprocess/model_preprocess.cpp`, `LibHsBaSlicer/Slice/mesh_slice.cpp` — catching `RuntimeError` at the C ABI boundary.
- `2D/FloatPolygons.cpp`, `2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp` — throwing sites for Lua/FreeType integration failures.