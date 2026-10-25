# Copilot / AI Code Assistant Instructions for HsBaSlicer ✅

Purpose: help an AI agent become productive quickly by pointing out the repository "big picture", developer workflows, recurring patterns, and concrete examples to reference.

## Big picture (high level) 🔧
- This is a modular C++ project targeting desktop and Android. Top-level modules include: `base`, `utils`, `fileoperator`, `paths`, `meshmodel`, `cadmodel`, `proto`, `HsBaSlicer` (app), `LibHsBaSlicer` (static lib), `DllHsBaSlicer` (DLL entry points) and `restapi` (optional HTTP server).
- Inter-module boundaries are CMake targets and public headers (each folder normally contains a `CMakeLists.txt` and exposed headers). Use CMake targets to understand compile/link boundaries.
- Key runtime integrations:
  - Lua scripting (many `LuaAdapter` classes under `2D/`, `fileoperator/`, `cipher/`); prefer `MakeUniqueLuaState()` and `RegisterLua*` helpers when adding Lua features.
  - Protobuf messages live in `proto/` and are compiled via the `proto/CMakeLists.txt` (optionally emit language outputs via `HSBA_PROTO_OUT_*`). Generated C++ protos are provided as a `HsBaSlicerProto` static target.
  - Native geometry libraries (CGAL, libigl, OpenCASCADE, Eigen) are used heavily in `meshmodel/` and `cadmodel/`.

## Build & test workflows (exact commands) 🏗️
- Recommended: use CMake with a vcpkg toolchain. Example (Linux/Windows with Ninja):
  - Configure: `cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`
  - Build: `cmake --build build --config Release`
  - Run tests: `cd build && ctest --output-on-failure --parallel 2`
- GitHub Actions uses `.github/workflows/cmake-multi-platform.yml` to build on `ubuntu-latest` and `windows-latest`; follow same steps locally (CI shows how to bootstrap vcpkg and Android NDK for `android/` builds).
- Protobuf generation: `proto/CMakeLists.txt` controls `HSBA_PROTO_OUT_*` options; `PROTOBUF_PROTOC_EXECUTABLE` is used to emit various languages. When working with proto files, update CMake if new languages/outputs are required.

## Tests & test conventions ✅
- Tests use Boost.Test and are collected under `tests/`.
- `tests/CMakeLists.txt` defines a `RunAllTests` target (invokes CTest with `-V` and regex filters for specific tests). Use CTest for CI-parity.
- Some tests depend on optional libraries (OCCT/CGAL/IGL). In Debug configuration, boolean-op tests may be disabled via `DISABLE_BOOLEAN_OPERATIONS_TESTS` macro (see `tests/CMakeLists.txt`).

## RESTful API server (optional module) 🌐
- `restapi/` builds `HsBaSlicerRestServer`, a thin HTTP layer over the `DllHsBaSlicer` C ABI. It is desktop-only and OFF by default; enable with both:
  - `-DHSBA_BUILD_REST_API=ON` and `-DVCPKG_MANIFEST_FEATURES=rest-api` (the gate in the root `CMakeLists.txt` aborts configure when the option is ON without the `rest-api` manifest feature installed).
  - HTTP library: cpp-httplib (vcpkg port name `cpp-httplib`, but the CMake package is `httplib` — use `find_package(httplib CONFIG)` and link `httplib::httplib`).
- Endpoints (see `restapi/routes.cpp` for the authoritative list):
  - `GET /api/health`, `GET /api/version`, `GET /api/pipelines` (alias `/api/kinds`)
  - `GET /api/pipelines/{kind}/default-config` (alias `/api/config/default/{kind}`)
  - `POST /api/config/convert` — proto bytes → JSON/XML (default) or, with `?to=proto`, JSON/XML body → protobuf
  - `POST /api/jobs` (submit, async queue) / `POST /api/jobs/{kind}` (override auto-detected kind), `GET /api/jobs` (list, `?state=&offset=&limit=`), `GET /api/jobs/{id}`, `GET /api/jobs/{id}/result`, `DELETE /api/jobs/{id}`
  - `PUT /api/params/{kind}/{name}` / `POST /api/params/{kind}` — ParamStore persistence (requires `[paramstore] backend` in `restapi.ini`)
- JSON + XML both supported: format chosen by `?format=` query, then `Accept` header, then `server.default_format`; response `Content-Type` set accordingly (`content_negotiate.cpp`).
- Runtime configuration comes from `restapi.ini` next to the executable (`server.host/port/default_format/max_concurrent_jobs/max_jobs/result_ttl_seconds`, `[paramstore]` block); missing file falls back to built-in defaults. Listen address must come from the ini — do not hardcode.
- Architecture notes: `job_manager.*` is the async queue (queued/running/success/failed states, TTL cleanup); `pipeline_executor.*` dispatches per-kind C ABI calls (create → run sync on a worker thread → free); `config_bridge.*` reuses `utils/struct_json.hpp` reflection serialization.
- Tests: `tests/RestApi/` (`ctest -R restapi` covers content negotiation, config roundtrip, job manager).
- Pitfalls when editing this module:
  - httplib handlers take only `(Request, Response)`; read regex captures via `req.matches[N]`, not a `std::smatch&` parameter.
  - Every struct passed to `SerializeValue` must live in a *named* namespace — MSVC boost::pfr rejects types in unnamed namespaces and lambdas (error C7631).
  - `std::string_view` does not implicitly convert to `std::string` in copy-init positions (set_content, aggregate init); construct explicitly.

## Project-specific patterns & conventions ✨
- Namespace convention: `HsBa::Slicer` (top-level) and `HsBa::Slicer::Utils` for utilities.
- Lua integration:
  - Provide `RegisterLua*` registration helpers and call `MakeUniqueLuaState()` in tests as examples (see `tests/PathsOut/layers_path_test.cpp`).
  - Look for `LuaAdapter.hpp`/`.cpp` across modules when exposing new functionality to scripts.
- Protobuf conversions live in `convert/` (e.g., `Msg2Eigen.cpp/hpp`, `Eigen2Msg`) — update these if adding new proto messages or Eigen types.
- Error handling: project throws `HsBa::Slicer::RuntimeError` in places (search for it when mapping error paths in tests).
- Static reflection and template helpers are located in `utils/` (e.g., `static_reflect.hpp`, `struct_json.hpp`); new serialization should follow existing utilities.

## Recommended simple tasks for newcomers (good starter PRs) 💡
- Add a small unit test that exercises the Lua registration for a small helper (copy pattern from `tests/FilesOperator/sqlite_test.cpp`).
- Add a `Config` translation for a new Eigen vector type under `utils` and add tests in `tests/ConfigMap/`.
- If adding a proto, add entries to `proto/CMakeLists.txt` and `convert/` helpers.

## Important files to inspect (quick map) 📁
- Build & CI: `CMakeLists.txt`, `vcpkg.json`, `.github/workflows/cmake-multi-platform.yml`
- REST server: `restapi/routes.cpp` (endpoints), `restapi/restapi.ini` (runtime config), `restapi/job_manager.*`, `restapi/pipeline_executor.*`, `restapi/config_bridge.*`
- Protobuf: `proto/` and `proto/CMakeLists.txt` (look for `HSBA_PROTO_OUT_*` options)
- Lua adapters: `fileoperator/LuaAdapter.*`, `2D/LuaAdapter.*`, `cipher/LuaAdapter.*`
- Tests: `tests/CMakeLists.txt`, `tests/*_test.cpp` (Boost.Test)
- Conversion helpers: `convert/Msg2Eigen.*`, `convert/Eigen2Msg.*`
- Static tools: `static_check/cpp_analyzer.py` (project-specific static analysis script)

## What *not* to assume / pitfalls ⚠️
- Do not assume all optional libraries are available locally (CGAL/OCCT may be missing). Use CI logs to reproduce required system packages.
- Android build requires Android NDK and chainloading with vcpkg; refer to `android/` and CI steps for examples.

## Helpful search queries (for AI agents) 🔍
- Find Lua binding examples: `RegisterLua` or `MakeUniqueLuaState`
- Find proto usage: search `HsBaSlicerProto` and `proto/*.proto`
- Find tests and examples: `tests/*_test.cpp`
- Find REST endpoint handlers: `Handle` prefix in `restapi/routes.cpp`; REST tests: `tests/RestApi/`
- Find Feature flags: look in `tests/CMakeLists.txt` and `proto/CMakeLists.txt`

---
If you'd like, I can add a short checklist or inline snippets (e.g., precise CMake options to enable OCCT) or merge any text you prefer — tell me what to expand or clarify. 🙋‍♂️
