---
kind: error_handling
name: HsBaSlicer Exception Hierarchy and C-ABI Error Boundary
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
    - DllHsBaSlicer/tdp_pipeline.cpp
    - DllHsBaSlicer/waam_pipeline.cpp
    - DllHsBaSlicer/lom_pipeline.cpp
    - DllHsBaSlicer/file_transfer_pipeline.cpp
    - 2D/ImageToPolygons.cpp
    - 2D/PolygonFill.cpp
    - 2D/FloatPolygons.cpp
    - cipher/LuaAdapter.cpp
    - support/LuaAdapter.cpp
    - utils/LuaAnyObject.cpp
---

## Approach

HsBaSlicer uses a **hierarchical C++ exception hierarchy** rooted at `std::runtime_error`, plus an explicit **C ABI error boundary** that swallows exceptions and returns null/zero/error-message fields instead of propagating across language boundaries. There is no `panic`/`recover` (not applicable to C++), no middleware framework, and no global error-code enum — errors are typed exceptions.

## Core Definition

All custom error types live in `base/error.hpp` under namespace `HsBa::Slicer`. The hierarchy is:

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

Each class forwards its constructors to `std::runtime_error` via `using std::runtime_error::runtime_error;` and re-exports `what()` through `using std::runtime_error::what;`. The `.cpp` file (`base/error.cpp`) is intentionally empty — all definitions are inline in the header so they can be used from Lua bindings without extra linking.

## Throwing Conventions

Call sites throw the most specific subclass available:
- `NotSupportedError("Need FreeType support")` in `2D/FloatPolygons.cpp` for optional-feature gates.
- `InvalidArgumentError`, `OutOfRangeError`, `NotFoundError`, `AlreadyExistsError`, `PermissionDeniedError`, etc. are defined but not heavily used in the sampled code — the dominant pattern is throwing plain `RuntimeError` with a human-readable message string.
- Lua integration points (`2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp`) wrap Lua state creation / script loading / function invocation failures as `RuntimeError` with messages like `"Failed to create Lua state"`, `"Lua function not found: ..."`, `"Lua function did not return a table"`.
- Third-party library exceptions (OpenCV's `cv::Exception`, CGAL's `std::exception`) are caught and typically re-thrown as `RuntimeError` or logged before propagation.

## C ABI Boundary — the Central Error Boundary

The `DllHsBaSlicer/` layer is the single place where C++ exceptions cross into C/C#/Java/Python consumers. Every exported `HSBA_SLICER_API` function follows one of two patterns:

1. **Return-value functions** (`model_preprocess.cpp`): wrap the call in `try { ... } catch (const HsBa::Slicer::RuntimeError&) { return nullptr; }` (or `return 0;`). Success is indicated by a non-null handle or a non-zero integer; failure is indicated by `nullptr` / `0`. An explicit comment marks intentional empty catches: `// NOLINTNEXTLINE(bugprone-empty-catch) C ABI: swallow RuntimeError, do not propagate across C boundary`.

2. **Result-struct functions** (e.g. `fdm_pipeline.cpp`, `sla_pipeline.cpp`, `slm_pipeline.cpp`, `sls_pipeline.cpp`, `tdp_pipeline.cpp`, `waam_pipeline.cpp`, `lom_pipeline.cpp`, `file_transfer_pipeline.cpp`): wrap the pipeline body in `try { ... } catch (const RuntimeError& e) { result.success = false; result.error_message = std::string("Pipeline error: ") + e.what(); }`. The caller inspects `result.success` and reads `result.error_message`.

This boundary is enforced consistently across every per-technology pipeline export file in `DllHsBaSlicer/`.

## Coroutine / Async Layer

The async pipeline entry points (`HsBaRunFdmPipelineAsync`, etc.) in `DllHsBaSlicer/fdm_pipeline.cpp` construct a coroutine task and pass the resulting `InternalResult` through `ToCResult(ir)` — which already contains `success` / `error_message` populated by the inner try/catch. The outer async callback does not add another catch layer; it assumes the inner coroutine has already converted exceptions into the result struct.

## Catching Patterns Outside the C Boundary

- `LibHsBaSlicer/Preprocess/model_preprocess.cpp` and `LibHsBaSlicer/Slice/mesh_slice.cpp` catch `const std::exception&` broadly, indicating that lower-level libraries may throw standard exceptions that bubble up until reaching the C ABI layer.
- `cipher/LuaAdapter.cpp`, `support/LuaAdapter.cpp`, `utils/LuaAnyObject.cpp` catch `const std::exception&` around Lua bridge calls to prevent uncaught exceptions from escaping into the Lua VM.
- Tests (`tests/Coroutines/...`, `tests/ParamStore/...`, `tests/Models/cgal_model_test.cpp`) use broad `catch (const std::exception&)` blocks to assert on expected failures.

## Key Files

- `base/error.hpp` — complete exception type hierarchy definition.
- `base/error.cpp` — translation unit (empty, forces header-only linkage).
- `DllHsBaSlicer/model_preprocess.cpp` — canonical example of the C-ABI try/catch → null/zero conversion pattern.
- `DllHsBaSlicer/fdm_pipeline.cpp` — canonical example of the C-ABI try/catch → `result.success=false` + `error_message` pattern.
- `DllHsBaSlicer/{sla,slm,sls,tdp,waam,lom,file_transfer}_pipeline.cpp` — same pattern repeated for each technology pipeline.
- `2D/ImageToPolygons.cpp`, `2D/PolygonFill.cpp` — primary throw sites for Lua integration errors.
- `2D/FloatPolygons.cpp` — example of domain-specific `NotSupportedError` usage.
- `cipher/LuaAdapter.cpp`, `support/LuaAdapter.cpp`, `utils/LuaAnyObject.cpp` — Lua bridge exception guards.

## Observed Conventions

1. All user-facing errors derive from `HsBa::Slicer::RuntimeError` (which derives from `std::runtime_error`); new error categories extend this hierarchy rather than inventing independent exception types.
2. Exceptions are thrown with a descriptive `std::string` message consumed by `what()` — there is no separate error-code enumeration.
3. No code path crosses the DllHsBaSlicer C ABI with an unhandled exception; every exported function wraps its body in `try/catch(const HsBa::Slicer::RuntimeError&)`.
4. At the C boundary, errors are represented either as null/zero return values or as `success=false` + `error_message` on a result struct — never as a propagated C++ exception.
5. Lua-boundary code catches `std::exception` broadly to keep the Lua VM alive; internal C++ code prefers catching the specific `RuntimeError` subclasses when possible.