---
kind: error_handling
name: HsBaSlicer Exception Hierarchy and Error Propagation
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - fileoperator/LuaAdapter.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
    - restapi/routes.cpp
---

## Approach

The codebase uses a **C++ exception-based** error model built on a small custom hierarchy rooted at `std::runtime_error`. There is no sentinel-error enum, no `Result<T,E>` type, and no `panic`/`recover` (the project is C++). Errors are thrown as exceptions and caught at API boundaries (C ABI wrappers, Lua adapters, REST handlers) where they are converted into status fields or HTTP responses.

## Core types — `base/error.hpp`

All domain errors live in namespace `HsBa::Slicer` and derive from `RuntimeError`, which itself derives from `std::runtime_error`:

- `RuntimeError` — base for all runtime failures
- `OutOfRangeError` — index / bounds violations
- `InvalidArgumentError` — invalid function arguments
- `IOError` — file / I/O failures
- `NotImplementedError` — stub / unimplemented paths
- `NullValueError` — null-pointer / empty-value checks
- `NotSupportedError` — optional feature unavailable (e.g. FreeType)
- `NotFoundError` — missing resource
- `AlreadyExistsError` — duplicate key
- `PermissionDeniedError` — access denied
- `TimeoutError` — timed-out operation
- `InterruptedError` — interrupted operation
- `CancelledError` — cancellation
- `OutOfMemoryError` — allocation failure

Each class exposes constructors forwarding to `std::runtime_error` and re-exports `what()` via `using std::runtime_error::what;`. The implementation file `base/error.cpp` is intentionally empty — it exists only as a translation unit so the header can be included without pulling in `<stdexcept>` into every TLU.

## Throwing conventions

Throw sites are concentrated in two areas:

1. **Lua bridge layer** (`2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp`) — when initializing Lua states, loading scripts, or invoking user functions, failures raise `RuntimeError` with descriptive messages (e.g. `"Failed to create Lua state"`, `"Lua function not found: ..."`).
2. **Feature-gated code** — e.g. `2D/FloatPolygons.cpp` throws `NotSupportedError("Need FreeType support")` when the optional FreeType backend is absent.

Third-party exceptions are also caught directly where relevant: OpenCV's `cv::Exception` is caught in `2D/ImageToPolygons.cpp` around image-processing calls.

## Catching / propagation boundaries

Catch sites follow a consistent pattern: wrap a pipeline call in `try { ... } catch (const RuntimeError& e) { ... }` and translate the exception into a non-exceptional result.

### C ABI boundary (`DllHsBaSlicer/*_pipeline.cpp`)

Every pipeline entry point (`fdm_pipeline.cpp`, `file_transfer_pipeline.cpp`, `lom_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`) wraps its async coroutine body in:

```cpp
catch (const RuntimeError& e)
{
    result.success = false;
    result.error_message = std::string("Pipeline error: ") + e.what();
}
```

This ensures the C ABI never propagates C++ exceptions across the DLL boundary — callers receive `success=false` plus an `error_message` string that must be freed via `HsBaFreePipelineResult`.

### Lua adapter boundary (`fileoperator/LuaAdapter.cpp`)

The Lua adapter catches `RuntimeError` at many call sites and converts them into Lua error strings returned to the script, keeping Lua code insulated from C++ exceptions.

### REST API boundary (`restapi/routes.cpp`)

The REST server does **not** install a global exception handler. Instead, individual handlers catch `std::exception` around config deserialization/conversion and map it to a JSON/XML `ErrorView` with HTTP 400:

```cpp
catch (const std::exception& e)
{
    ReplyError(res, 400, "invalid config body", e.what(), outfmt);
    return;
}
```

Validation failures (unknown pipeline kind, missing parameters) use `ReplyError` with appropriate 4xx codes (400, 404, 409, 429, 503) rather than throwing. Uncaught exceptions would propagate to httplib's default error handling — there is no top-level try/catch around route dispatch.

## Conventions observed

- All application errors are thrown as members of the `HsBa::Slicer` exception hierarchy derived from `RuntimeError`.
- Exceptions are **not** propagated across the DllHsBaSlicer C ABI; every exported pipeline function catches `RuntimeError` and serializes it into the `error_message` field of the C struct.
- Lua-callable C++ functions catch `RuntimeError` and convert it to a Lua-friendly error value.
- The REST API uses structured error responses (`ErrorView` with `status`, `message`, `detail`) produced by `ReplyError`, with HTTP status codes chosen per endpoint semantics (400 bad request, 404 not found, 409 conflict, 429 queue full, 503 service unavailable).
- Optional-feature failures use `NotSupportedError`; argument validation uses `InvalidArgumentError`; resource-not-found uses `NotFoundError`.
- Third-party exceptions (`cv::Exception`, generic `std::exception`) are caught at integration points but are not part of the application's own error taxonomy.