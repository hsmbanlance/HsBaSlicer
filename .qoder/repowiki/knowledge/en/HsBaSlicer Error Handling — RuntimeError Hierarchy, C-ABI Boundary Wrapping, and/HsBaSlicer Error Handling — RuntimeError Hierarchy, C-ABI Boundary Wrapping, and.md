---
kind: error_handling
name: HsBaSlicer Error Handling — RuntimeError Hierarchy, C-ABI Boundary Wrapping, and Lua/Third-Party Exception Bridging
category: error_handling
scope:
    - '**'
source_files:
    - base/error.hpp
    - base/error.cpp
    - DllHsBaSlicer/model_preprocess.cpp
    - DllHsBaSlicer/fdm_pipeline.cpp
    - DllHsBaSlicer/sla_pipeline.cpp
    - DllHsBaSlicer/slm_pipeline.cpp
    - DllHsBaSlicer/sls_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - LibHsBaSlicer/Extends/LuaCommonTypes.cpp
    - fileoperator/LuaAdapter.cpp
    - cipher/LuaAdapter.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
    - meshmodel/CgalModel.cpp
    - cipher/encrypt.cpp
    - static_tests/base_static_tests/base_static_tests.cpp
---

## Approach

HsBaSlicer uses a **C++ exception-based error model** built on a small hierarchy rooted at `std::runtime_error`, plus explicit bridging of third-party exceptions (OpenCV, CGAL, OpenSSL) and Lua runtime errors into that hierarchy. There is no sentinel-error / Result-returning pattern in the core libraries; instead, functions throw, and callers catch.

The hierarchy lives in `base/error.hpp` under namespace `HsBa::Slicer`:

```
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

Each subclass forwards its constructors to `std::runtime_error` via `using std::runtime_error::runtime_error;` and re-exports `what()` via `using std::runtime_error::what;`. The `.cpp` file (`base/error.cpp`) is intentionally empty — all definitions are inline in the header.

## Propagation Strategy

### Core library code
Most slicing / geometry code throws the custom types directly:
- `2D/FloatPolygons.cpp` throws `NotSupportedError("Need FreeType support")` and `RuntimeError(...)` for font/Lua failures.
- `2D/ImageToPolygons.cpp` and `2D/PolygonFill.cpp` throw `RuntimeError` around every Lua interop failure (state creation, script load, function lookup, return-value validation).
- `cipher/encrypt.cpp` throws `std::invalid_argument` for bad IV lengths.
- `meshmodel/CgalModel.cpp` catches CGAL's `std::exception` and re-throws as `std::runtime_error`.

### C ABI boundary (`DllHsBaSlicer/`)
The stable C-export layer wraps every internal call in `try { ... } catch (const HsBa::Slicer::RuntimeError&) { ... }` so that C consumers never see C++ exceptions:
- Scalar-returning APIs (`HsBaLoadModel`, `HsBaGetModel`, `HsBaTranslateModel`, `HsBaRotateModel`, `HsBaScaleModel*`, `HsBaContainsModel`, `HsBaModelCount`, `HsBaCleanupModels`, `HsBaGetModelInfo`, …) return `nullptr` or `0` on error.
- Pipeline entry points (`fdm_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `lom_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`, `file_transfer_pipeline.cpp`) catch `RuntimeError` and populate `result.success = false` + `result.error_message = "Pipeline error: " + e.what()`.
- A comment in `model_preprocess.cpp` explicitly documents the rule: `// NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow RuntimeError, do not propagate across C boundary`.

### Lua bridge layer
Lua adapters convert between Lua state errors and the C++ hierarchy:
- `LibHsBaSlicer/Extends/LuaCommonTypes.cpp` catches `RuntimeError` and converts it to a Lua error via `luaL_error`.
- `fileoperator/LuaAdapter.cpp` catches `RuntimeError` from ParamStore operations and raises them back to Lua.
- `cipher/LuaAdapter.cpp` catches `std::exception` broadly when wrapping OpenSSL calls.

### Third-party exception bridging
- OpenCV exceptions (`cv::Exception`) are caught in `2D/ImageToPolygons.cpp` (lines 69, 89, 483) and converted to `RuntimeError`.
- CGAL operations in `meshmodel/CgalModel.cpp` are wrapped with a comment noting they may throw `std::exception` or derived types; failures are turned into `std::runtime_error`.
- OpenSSL parameter validation in `cipher/encrypt.cpp` throws `std::invalid_argument` (a standard-library type, not part of the hierarchy).

## Conventions Observed

1. **Domain errors use the `HsBa::Slicer::*Error` subclasses**, while generic failures use bare `RuntimeError` with a human-readable message string.
2. **Cross-language boundaries (C ABI, Lua) never let C++ exceptions escape.** Every exported function either swallows `RuntimeError` and returns an error indicator, or translates it into a result struct field / Lua error.
3. **Pipeline-level error aggregation**: async pipeline coroutines (`RunPipelineAsync`) catch `RuntimeError` and attach `e.what()` to `result.error_message`; success/failure is communicated through a `success` boolean rather than exception propagation.
4. **Third-party exceptions are localized**: only thin adapter layers (`ImageToPolygons`, `CgalModel`, `cipher/LuaAdapter`) translate external exceptions; business logic throws the project's own types.
5. **No `panic`/`recover` equivalent exists** — this is C++, not Rust; there is no global panic handler. Errors are propagated via exceptions up to the nearest boundary wrapper.
6. **Tests exercise the exception machinery**: `static_tests/base_static_tests/base_static_tests.cpp` contains `throw std::runtime_error("test exception")` lines used by static assertions to verify compile-time behavior of the coroutine/exception utilities.

## Constraints Enforced by Code

- Exceptions thrown inside `DllHsBaSlicer/*_pipeline.cpp` coroutine bodies are caught by `catch (const RuntimeError& e)` blocks and converted into `InternalResult` fields — they do not bubble out of the C-exported `HsBaRun*Pipeline` functions.
- `DllHsBaSlicer/model_preprocess.cpp` enforces a zero-return convention for scalar C API functions: any `RuntimeError` path returns `0` (and `nullptr` for pointer-returning functions), documented by an `NOLINTNEXTLINE(bugprone-empty-catch)` suppression.
- Lua-facing adapters consistently catch `RuntimeError` (or `std::exception` where the source is unknown) and raise Lua errors, ensuring Lua scripts cannot crash the host process.