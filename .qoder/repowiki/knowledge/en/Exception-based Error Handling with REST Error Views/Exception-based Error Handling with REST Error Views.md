---
kind: error_handling
name: Exception-based Error Handling with REST Error Views
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - restapi/routes.cpp
    - restapi/job_manager.cpp
    - restapi/main.cpp
    - restapi/restapi.hpp
---

## Approach

HsBaSlicer uses C++ exceptions (not `std::expected`, not return codes) as its primary error-propagation mechanism. The core library defines a small hierarchy of custom exception types under `namespace HsBa::Slicer` that all derive from `std::runtime_error`. At the HTTP boundary (`restapi/`), exceptions are caught and translated into structured JSON/XML error responses via a shared `ErrorView` shape.

There is no middleware layer in cpp-httplib; each route handler catches exceptions locally, and the worker thread in `JobManager::Execute` has a top-level try/catch that converts any pipeline exception into a terminal job state (`Failed` or `Cancelled`).

## Core exception hierarchy

All custom errors live in `base/error.hpp`:

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

Each class only forwards constructors to `std::runtime_error` and re-exports `what()` — there is no additional payload beyond the message string. `error.cpp` is a translation unit placeholder since everything is inline in the header.

These types are used throughout the slicing engine (e.g. `2D/FloatPolygons.cpp` throws `NotSupportedError("Need FreeType support")` / `RuntimeError("Failed to initialize FreeType library")`; `DllHsBaSlicer/custom_pipeline.cpp` throws `CancelledError("pipeline cancelled")`).

## REST-layer error presentation

The HTTP server does not expose raw exception types. In `restapi/routes.cpp` a helper `ReplyError(Response&, int status, const std::string& message, const std::string& detail, DataFormat fmt)` serializes an `ErrorView{status, message, detail}` through `SerializeValue` into either JSON or XML (the format negotiated per request).

Every route follows the same pattern: validate input → call into `DllHsBaSlicer` / `JobManager` / `UploadStore` → on failure call `ReplyError` with a human-readable `message` and a machine-oriented `detail` (often the offending value, e.g. the bad kind name, layer index, or file path). Examples:

| Situation | Status | Message | Detail |
|---|---:|---|---|
| Missing/malformed multipart parts | 400 | `"missing config part"` / `"missing model part"` | expected-part description |
| Model exceeds `max_file_mb` | 413 | `"model file too large"` | `"exceeds [upload] max_file_mb"` |
| Upload pool full | 503 | `"upload pool full"` | `"raise [upload] max_files or delete old uploads"` |
| Job queue full | 429 | `"job queue full"` | `"max_jobs reached"` |
| Unknown pipeline kind | 400 | `"missing or unknown pipeline kind"` | the raw kind string |
| Job not found | 404 | `"job not found"` | the id |
| Job cancelled | 409 | `"job was cancelled"` | the id |
| Job not finished successfully | 409 | `"job not finished successfully"` | the id |
| Layer index out of range | 404 | `"layer index out of range"` | `"layer_count=N"` |
| Package entry not found | 404 | `PACKAGE_ENTRY_NOT_FOUND` | entry name |
| Package entry too large | 409 | `PACKAGE_ENTRY_TOO_LARGE` | entry name |
| Config parse/conversion failure | 400 | `"invalid config body"` / `"config conversion failed"` | `e.what()` |

The `ErrorView` struct is defined in `restapi/restapi.hpp` and is the single canonical response envelope for errors across both JSON and XML formats.

## Exception-to-state mapping in the worker

`JobManager::Execute` (in `restapi/job_manager.cpp`) wraps the pipeline runner in:

```cpp
try {
    outcome = runner_(...);
} catch (const std::exception& e) {
    failed_exception = true;
    thrown = e.what();
} catch (...) {
    failed_exception = true;
    thrown = "unknown pipeline error";
}
```

It then decides the final `JobState`:
- If the thrown message starts with `"Cancelled"` (checked by `LooksCancelled`), the job becomes `Cancelled`.
- Otherwise a caught exception becomes `Failed`.
- A non-exception result is `Success` if `outcome.success` is true, else `Failed`.

A bare `catch (...)` is also present at line 417 as a safety net for non-`std::exception` throws; it records `"unknown pipeline error"` rather than crashing the worker thread.

## Top-level process error handling

`restapi/main.cpp` catches configuration-load failures at the process entry point:

```cpp
try {
    cfg = LoadServerConfig(config_path);
} catch (const std::exception& e) {
    std::fprintf(stderr, "Failed to parse %s: %s\n", ...);
    return 1;
}
```

Unrecognized CLI arguments print usage and return exit code 2. There is no global httplib exception handler; individual handlers catch `std::exception` around their parsing/deserialization calls.

## Conventions observed

1. **Custom error types live in `base/error.hpp`** and inherit from `std::runtime_error`; they carry no extra data beyond the message.
2. **Engine code throws typed exceptions** (`RuntimeError`, `NotSupportedError`, `CancelledError`, etc.) to signal domain failures.
3. **REST handlers never propagate exceptions to the client**: every route catches `std::exception` and returns an `ErrorView` with an appropriate HTTP status code.
4. **Worker threads swallow all exceptions** so a failing pipeline does not kill the server; the failure is recorded as a terminal job state.
5. **Error responses are dual-format**: `ReplyError` serializes through `SerializeValue` so the same `ErrorView` works for both JSON and XML clients.
6. **Status-code semantics are consistent**: 400 for malformed input, 404 for missing resources, 409 for conflicting state (cancelled/not finished), 413 for oversized uploads, 429 for queue overflow, 503 for disabled features, 500 for internal I/O failures.
7. **Detail fields contain machine-parseable values** (raw inputs, counts, filenames) while messages are user-facing strings.
8. **No `panic`/`recover` equivalent exists** — this is C++, not Go; the closest analogue is the `catch (...)` fallback in `JobManager::Execute`.