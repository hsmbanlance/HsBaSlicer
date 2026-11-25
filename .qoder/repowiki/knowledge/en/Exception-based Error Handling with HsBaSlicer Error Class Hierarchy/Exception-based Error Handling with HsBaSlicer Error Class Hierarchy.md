---
kind: error_handling
name: Exception-based Error Handling with HsBa::Slicer Error Class Hierarchy
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/custom_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - restapi/main.cpp
    - restapi/package_reader.cpp
    - LibHsBaSlicer/Extends/LuaCommonTypes.cpp
    - 2D/FloatPolygons.cpp
    - 2D/ImageToPolygons.cpp
---

## Approach

HsBaSlicer uses **C++ exceptions** as its primary error-handling mechanism, centered on a small hierarchy of custom exception classes in `base/error.hpp`. There is no sentinel-error enum system, no `Result<T,E>` type, and no `panic`/`recover` (no C++ `setjmp`/`longjmp` or equivalent). Errors propagate up the call stack via `throw` / `catch`, and are converted to structured results only at process boundaries (the C ABI layer and the REST API).

## Core types

All project-specific exceptions live in namespace `HsBa::Slicer` and derive from `std::runtime_error`:

- `RuntimeError` — base for all project errors.
- `OutOfRangeError`, `InvalidArgumentError`, `IOError`, `NotImplementedError`, `NullValueError`, `NotSupportedError`, `NotFoundError`, `AlreadyExistsError`, `PermissionDeniedError`, `TimeoutError`, `InterruptedError`, `CancelledError`, `OutOfMemoryError`.

Each class exposes the standard constructors plus `using std::runtime_error::what;` so `e.what()` works uniformly. The implementation file `base/error.cpp` exists only as a translation unit that includes the header (all definitions are inline).

## Throwing conventions

- Validation failures throw `InvalidArgumentError` (e.g. `restapi/config_bridge.cpp:73`, `:300`, `:318`).
- I/O failures throw `IOError`.
- Unsupported features throw `NotSupportedError` (e.g. `2D/FloatPolygons.cpp:369`).
- Internal invariant/runtime failures throw `RuntimeError`.
- Cancellation across pipeline stages throws `CancelledError` (see `DllHsBaSlicer/custom_pipeline.cpp:65`, `fdm_pipeline.cpp:103`).
- Lua integration code wraps Lua runtime errors by throwing `RuntimeError` with descriptive messages (`2D/ImageToPolygons.cpp`).

## Catching and conversion patterns

### Pipeline boundary (DllHsBaSlicer)

Every pipeline wrapper (`fdm_pipeline.cpp`, `file_transfer_pipeline.cpp`, `lom_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`) follows the same shape:

```cpp
catch (const CancelledError& e) {
    result.success = false;
    result.cancelled = true;
    result.error_message = "Cancelled: " + e.what();
}
catch (const RuntimeError& e) {
    result.success = false;
    result.error_message = "Pipeline error: " + e.what();
}
```

The distinction between `CancelledError` and other `RuntimeError`s is significant because the C ABI result struct has no status field — callers rely on the `"Cancelled:"` prefix in `error_message` to distinguish cancellation from failure (comment at `fdm_pipeline.cpp:770-772`).

### Preview / non-fatal paths

Thumbnail generation (`fdm_pipeline.cpp:339-383`) catches `std::exception` broadly around mesh projection and deliberately converts failures into preview thumbnails with a `render_failed` / `mesh_too_large` label rather than failing the whole job — the comment states this is intentional: "A boolean over a pathological mesh must stay a preview problem, never a slice failure."

### REST API boundary

- `restapi/main.cpp:114-123`: top-level config parsing catches `std::exception` and prints to stderr, returning exit code 1.
- `restapi/job_manager.cpp:171-177`: ID parsing silently swallows `std::exception` (empty catch body).
- `restapi/package_reader.cpp` uses an **out-parameter error string** pattern (`ReadPackageEntries(..., std::string& error)`) instead of exceptions for archive I/O, returning `bool` success flags. This is the main place where exceptions are avoided in favor of return-value error reporting.

### Lua bridge

Lua-facing code (`LibHsBaSlicer/Extends/LuaCommonTypes.cpp`, `lua_pipeline.cpp`) catches `RuntimeError` and translates it back into Lua error strings, keeping the C++ exception from crossing the Lua boundary.

## Conventions observed

1. **All domain errors go through the `HsBa::Slicer::*Error` hierarchy** — there is no ad-hoc `std::logic_error` / `std::invalid_argument` usage in the slicing core.
2. **Cancellation is a first-class error**: `CancelledError` is thrown at stage checkpoints and caught specially to set `result.cancelled = true`; normal failures do not set this flag.
3. **C ABI functions never throw** — they wrap their coroutine/task bodies in try/catch and populate `success` / `error_message` fields of the returned struct. `HsBaFreePipelineResult` is provided to free the owned `error_message` buffer.
4. **Preview/thumbnail code swallows broad `std::exception`** to keep rendering failures from breaking the slice job.
5. **Lua boundary catches `RuntimeError`** specifically to convert it to a Lua error message.
6. **REST API package reading opts out of exceptions**, using `bool` return + `std::string& error` output parameters instead.
7. **No `try` blocks exist inside pure algorithmic libraries** (`2D/`, `support/`, `paths/`, `pointcloud/`, `cadmodel/`, `meshmodel/`) — they throw and let callers decide whether to treat the error as fatal or recoverable.

## Not present

- No sentinel error codes / enums.
- No `std::expected` / `Result` type.
- No `panic`/`recover` or equivalent mechanism.
- No global error handler or middleware layer beyond the per-pipeline try/catch wrappers.
- No logging-integrated error context (errors carry plain `std::string` messages from `what()`).

## Key files

- `base/error.hpp` — exception class hierarchy definition.
- `base/error.cpp` — translation unit for the header.
- `DllHsBaSlicer/fdm_pipeline.cpp` — canonical pipeline try/catch wrapper (also duplicated across every other pipeline).
- `DllHsBaSlicer/custom_pipeline.cpp` — cancellation throwing point.
- `restapi/main.cpp` — top-level exception-to-exit-code handling.
- `restapi/package_reader.cpp` — alternative return-value error pattern.
- `LibHsBaSlicer/Extends/LuaCommonTypes.cpp` — Lua-boundary `RuntimeError` catching.