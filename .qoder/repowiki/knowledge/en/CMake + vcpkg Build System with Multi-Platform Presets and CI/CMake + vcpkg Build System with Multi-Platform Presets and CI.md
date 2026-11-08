---
kind: build_system
name: CMake + vcpkg Build System with Multi-Platform Presets and CI
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - cmake/coverage.cmake
    - cmake/deploy_dlls.cmake
    - cmake/HsBaWindowsUtf8.cmake
    - version/Generator_Version.ps1
    - docker/Dockerfile
    - .github/workflows/build-linux.yml
    - .github/workflows/build-windows.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/docker-rest-api.yml
---

## Build Approach

HsBaSlicer is a C++20 project built entirely through **CMake (minimum 3.28)** with **vcpkg** as the dependency manager. There are no Makefiles, autotools, or custom shell build scripts at the repository root — all orchestration lives in `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, and `.github/workflows/*.yml`.

The top-level `CMakeLists.txt` sets `CMAKE_CXX_STANDARD=20`, enables `-fPIC` globally, forces UTF-8 on MSVC (`/utf-8`), and runs feature probes from `static_check/feature_check.cmake` that gate compilation on C++ concepts, ranges, `std::source_location`, NTTP class/float parameters, template-template matching, coroutines, explicit-this, and C `_Generic`. Missing required features cause `message(FATAL_ERROR ...)`.

## Dependency Management

`vcpkg.json` declares all third-party libraries (Boost, Clipper2, OpenCV, Eigen3, libigl, OpenVDB, protobuf, RapidJSON, tinyxml2, yaml-cpp, Lua, OpenSSL, sqlpp11, magic_enum, bit7z, fontconfig, freetype, miniz). Two manifest features control major build variants:

- `copyleft` (default-features): pulls CGAL, libigl[cgal], OpenCascade; when enabled the project license switches to GPL-3.0-or-later via `HSBA_COPL`.
- `rest-api`: pulls cpp-httplib (with openssl) for the optional `HsBaSlicerRestServer` executable.

Platform-scoped dependencies use `platform:` filters (e.g. `!windows & !linux & !osx & !android` for libigl's non-desktop variant, Android/iOS get only sqlite3 for sqlpp11).

## Platform & Variant Options

Key cache variables in the top-level CMake:

| Variable | Meaning |
|---|---|
| `HSBA_BUILD_REST_API` | Build the REST server (desktop-only; requires `rest-api` vcpkg feature) |
| `HSBA_COPL` | AUTO/ON/OFF — link copyleft kernels (CGAL/OpenCascade); AUTO probes installed packages |
| `HSBA_SLICER_MODULE` | Optional C++20 module wrapper (`ModuleHsBaSlicer`) |
| `HSBA_USE_DLL_LOADER` | Dynamic library loader (disabled on Android/iOS/game consoles) |
| `HSBA_HSBA_USE_BIT7Z` | Use bit7z (disabled on mobile) |
| `HSBA_SLICER_USE_TESTS` | Enable Boost.Test suite |
| `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` | Auto-run tests after build (off during cross-compilation) |
| `HSBA_SLICER_BUILD_SAMPLES` | Build sample executables per slicing technology |
| `HSBA_SLICER_BUILD_DOCS` | Doxygen docs (auto-disabled if Doxygen not found) |
| `HSBA_COVERAGE` | Instrument with gcov/gcovr and add `coverage` target |
| `BUILD_SHARED_LIBS` | Driven by `VCPKG_TARGET_TRIPLET` (Windows/dynamic → ON; Linux/Android/static → OFF) |

Game console builds are detected from triplets matching `xbox|switch|playstation|stadia`, which set `HSBA_IS_GAME_CONSOLE` / `HSBA_GAME_CONSOLE_<TRIPLET>` and disable logging, OpenGL-heavy deps, etc.

## Output Layout

All artifacts land under `${CMAKE_BINARY_DIR}/bin/$<CONFIG>` (runtime and shared libraries unified). Debug binaries get a `d` postfix via `CMAKE_DEBUG_POSTFIX`. The install tree uses GNUInstallDirs: runtime→`bin`, library/archive→`lib`, headers→`include/HsBaSlicer/*`, config→`etc/HsBaSlicer/restapi.ini`, proto multi-language outputs→`share/HsBaSlicer/proto/<lang>`.

## Versioning

Version info is generated at configure time by `version/Generator_Version.ps1`, which parses `vcpkg.json`, resolves platform-scoped dependencies, computes a project license (MIT vs GPL-3.0-or-later based on whether any GPL/LGPL dep is present), and emits `version.cpp` containing build type, platform, vcpkg triplet, configure timestamp, and a list of third-party libraries with license/homepage metadata. The version string itself is hard-coded as `"1.0.0"` in the generator.

## Coverage

`cmake/coverage.cmake` adds `--coverage` compile/link flags to every target and defines a `coverage` CMake target that runs gcovr with two thresholds: core modules (base, 2D, utils, cipher, meshmodel, paths, preprocess, support, LibHsBaSlicer) must reach ≥45% line coverage; everything else ≥25%. Thresholds are enforced via `--fail-under-line` so CI gates fail on regression. Excluded dirs include tests, samples, static_tests, static_check, third_parts_without_vcpkg, docs, out, build, vcpkg_installed, vcpkg/, /usr/.

## CMake Presets

`CMakePresets.json` provides named presets for Windows (x64 Debug/Release, cl.exe), Linux (Debug/Release/Coverage), macOS (Debug/Release), Android (arm64, NDK chainload), and iOS (Xcode, arm64, deployment target 26.5). All presets point binary/install dirs under `out/build/<presetName>` and `out/install/<presetName>`, and set `CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/scripts/vcpkg.cmake`. The `linux-coverage` preset additionally sets `HSBA_COVERAGE=ON`, disables samples/docs/static-tests/test execution, and pins core/other thresholds at 45%/25%.

## CI Pipelines

GitHub Actions workflows live under `.github/workflows/`:

- `build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml` — per-platform build jobs using `actions/checkout@v6`, system package installation, vcpkg bootstrap, `cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=...`, `cmake --build`, then `ctest`.
- `test-coverage.yml` — runs the coverage preset and the `coverage` target.
- `docker-rest-api.yml` — builds the Docker image defined in `docker/Dockerfile`.
- `cmake-multi-platform.yml` — matrix build across platforms.
- `rest-webui.yml` — builds the Node.js web UI under `webui/`.
- `publish-wiki.yml` — publishes documentation.

Linux CI installs an extensive apt dependency set (OpenGL/X11 dev libs, mesa, wayland, ruby, haskell-stack, golang, meson, nasm, etc.) plus PowerShell for the version generator script. It uses `easimon/maximize-build-space@v10` to free runner disk space and caches vcpkg downloads via `actions/cache@v5` keyed on `vcpkg.json`.

## Containerization

`docker/Dockerfile` is a two-stage build (builder + runtime) targeting `ubuntu:24.04`. The builder stage clones vcpkg, sets `VCPKG_MANIFEST_FEATURES=${HSBA_FEATURES}` (default `rest-api`) and `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`, then runs `cmake --preset linux-release` with `HSBA_BUILD_REST_API=ON` and installs into `/opt/hsba-install`. The runtime stage copies only the installed artifacts plus `restapi.ini` and `logcfg.ini`, changes `host = 127.0.0.1` to `0.0.0.0`, creates a non-root `hsba` user, exposes port 8080, and defines a HEALTHCHECK against `/api/health`. Entrypoint passes `--host`/`--port` CLI args and falls back to `HSBA_REST_HOST`/`HSBA_REST_PORT` environment variables; TLS credentials come from `HSBA_REST_TLS_CERT`/`HSBA_REST_TLS_KEY` env vars (not baked into the image).

## Static Checks

`static_check/` contains `clang-format-batch.py`, `cpp_analyzer.py`, and `feature_check.cmake` (the latter providing the `check_cxx_feature_*` macros used by the top-level CMake). `.clang-format`, `.clang-tidy`, and `.clangd` provide editor integration.

## Key Files

- `CMakeLists.txt` — top-level build configuration, options, subdirectory inclusion, install/export rules
- `CMakePresets.json` — named configure/build/test presets for all supported platforms
- `vcpkg.json` — dependency manifest with `copyleft` and `rest-api` features
- `cmake/coverage.cmake` — gcov/gcovr instrumentation and threshold gating
- `cmake/deploy_dlls.cmake` — helper to copy PDB symbols next to executables
- `cmake/HsBaWindowsUtf8.cmake` — Windows UTF-8 active code page manifest helper
- `version/Generator_Version.ps1` — generates `version.cpp` with build metadata and third-party license table
- `docker/Dockerfile` — two-stage container image for HsBaSlicerRestServer
- `.github/workflows/build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml`, `test-coverage.yml`, `docker-rest-api.yml` — CI pipelines