---
kind: build_system
name: CMake + vcpkg Build System with Multi-Platform CI
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/build-windows.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - version/Generator_Version.ps1
    - cmake/coverage.cmake
    - cmake/deploy_dlls.cmake
    - cmake/HsBaWindowsUtf8.cmake
    - static_check/feature_check.cmake
---

## Overview

HsBaSlicer is a C++20 project built entirely with **CMake ≥ 3.28** (top-level `CMakeLists.txt`) and managed through **vcpkg** (`vcpkg.json`, `vcpkg-configuration.json`). There are no Makefiles, autotools, or custom shell build scripts — all build logic lives in CMake plus a small PowerShell helper for version generation.

## Toolchain & Language

- C++ standard: `CMAKE_CXX_STANDARD = 20` (enforced via `CMAKE_C_STANDARD_REQUIRED ON`).
- C standard: auto-selected between C11 and C23 depending on compiler version (GCC ≥ 13, Clang ≥ 16, MSVC ≥ 19.36).
- All targets get `-fPIC` (`CMAKE_POSITION_INDEPENDENT_CODE ON`) because static libraries are linked into shared libraries.
- Windows UTF-8 active code page is forced via `/utf-8` and an embedded `<activeCodePage>UTF-8</activeCodePage>` manifest (`cmake/HsBaWindowsUtf8.cmake`).
- Output layout is unified: `bin/<CONFIG>/` for both executables and shared libraries (`CMAKE_RUNTIME_OUTPUT_DIRECTORY`, `CMAKE_LIBRARY_OUTPUT_DIRECTORY`).

## Dependency Management (vcpkg)

`vcpkg.json` declares all dependencies with per-platform constraints:
- Core deps: Boost (locale/log/dll/multiprecision/nowide/date-time/property-tree/test/units/uuid/pfr/graph), Clipper2, libjpeg-turbo, libpng, OpenCV, Eigen3, FreeType, miniz, OpenVDB, Protobuf, RapidJSON, tinyxml2, yaml-cpp, Lua, OpenSSL, sqlpp11 (SQLite3+MySQL+PostgreSQL on desktop; SQLite3-only on Android/iOS), magic-enum, bit7z, fontconfig.
- Two optional features:
  - `copyleft` (default feature): pulls CGAL, libigl[cgal], OpenCascade → switches project license to GPL-3.0-or-later.
  - `rest-api`: pulls cpp-httplib → enables optional `HsBaSlicerRestServer` executable.
- Triplet-driven binary vs. static linking: `VCPKG_TARGET_TRIPLET` matching `windows|xbox|dynamic` forces `BUILD_SHARED_LIBS=ON`; `linux|android|static` forces it OFF.
- Game-console triplets (`xbox|switch|playstation|stadia`) define `HSBA_IS_GAME_CONSOLE` / `HSBA_GAME_CONSOLE_*` and disable logging, DLL loader, bit7z, etc.

## Top-Level CMake Architecture

The root `CMakeLists.txt` performs feature detection via `static_check/feature_check.cmake` (concepts, ranges, source_location, NTTP class/fp, template-template matching, coroutines, explicit-this, C23 constexpr) and fails fast if required features are missing. It then:

1. Detects platform (`HSBA_DESKTOP` / `HSBA_MOBILE` / game console) from `CMAKE_SYSTEM_NAME` and `VCPKG_TARGET_TRIPLET`.
2. Gates optional subsystems: `HSBA_BUILD_REST_API` (desktop-only, requires `rest-api` vcpkg feature), `HSBA_COPL` (AUTO/ON/OFF for copyleft kernels), `HSBA_SLICER_MODULE` (C++20 modules), `HSBA_POLYGON_DUMP`, `HSBA_USE_DLL_LOADER`, `HSBA_HSBA_USE_BIT7Z`, `HSBA_ENABLE_THREAD_POOL_COROUTINE`, `HSBA_PROTOBUF_OUT`.
3. Adds subdirectories in dependency order: `third_parts_without_vcpkg` → `proto` → `base` → `cipher` → `utils` → `logger` → `2D` → `paths` → `fileoperator` → `meshmodel` → `cadmodel` (if COPL) → `pointcloud` → `preprocess` → `support` → `pipelinetypes` → `convert` → `LibHsBaSlicer` → `ModuleHsBaSlicer` → `DllHsBaSlicer` → `HsBaSlicer` → `version` → optional `restapi` → samples → tests → static_tests → docs.
4. Installs targets under `HsBaSlicer::` namespace with generated `HsBaSlicerConfig.cmake` / `HsBaSlicerConfigVersion.cmake` (SameMajorVersion compatibility).
5. Copies `utils/logcfg.ini` and (optionally) `restapi/restapi.ini` into the build output directory.

## Presets & IDE Integration

`CMakePresets.json` defines platform-specific configure presets using Ninja (Xcode for iOS):
- `windows-base` → `x64-debug` / `x64-release`
- `linux-debug` / `linux-release` / `linux-coverage` (gcov-instrumented, thresholds core 45% / other 25%)
- `macos-debug` / `macos-release`
- `ios-release` (arm64, deployment target 26.5)
- `android-release` (arm64-android, android-28, uses `ANDROID_NDK_HOME`)
All presets point `binaryDir` to `out/build/${presetName}` and `installDir` to `out/install/${presetName}`, and load the vcpkg toolchain via `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`. Visual Studio Remote Settings vendor metadata is included for remote builds.

## CI (GitHub Actions)

Per-platform workflows live in `.github/workflows/`:
- `build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml` — each clones vcpkg, bootstraps it, caches `vcpkg/downloads` keyed by `vcpkg.json`, installs system deps, configures with Ninja + vcpkg toolchain, builds Release, and runs `ctest`.
- `test-coverage.yml` — full coverage pipeline: builds with `linux-coverage` preset (parallelism capped to 2 for gcov RAM safety), runs tests, executes `cmake --build --preset linux-coverage-report` to enforce the 45%/25% line-coverage gates, writes summary to GitHub job summary, uploads HTML/XML reports as artifacts.
- `cmake-multi-platform.yml` — matrix build across platforms.
- `publish-wiki.yml` — documentation publishing.

## Versioning

`version/Generator_Version.ps1` is invoked during CMake configure to generate `version/version.cpp` from `version/version.cpp.in`. It parses `vcpkg.json`, enumerates enabled dependencies (including `copyleft` feature deps), infers the project license (MIT unless any GPL/LGPL dep is present), and emits runtime `VersionInfo` containing library name, license, version string, build type, host/target platform, vcpkg triplet, configure timestamp, and third-party library list with licenses and homepages.

## Coverage & Static Checks

- `cmake/coverage.cmake` instruments all targets with `--coverage` and adds a `coverage` target that runs gcovr against the two-tier thresholds (core ≥ 45%, other ≥ 25%).
- `static_check/feature_check.cmake` provides `check_cxx_feature_*` / `check_c_feature_*` macros used at configure time to fail-fast on unsupported compiler features.
- `static_check/cpp_analyzer.py`, `static_check/clang-format-batch.py`, `static_check/feature_check.cmake` provide pre-commit-style static analysis hooks.

## Install & Packaging

- Targets installed under `GNUInstallDirs` paths (`bin/`, `lib/`).
- Public headers installed per module (`DllHsBaSlicer/`, `LibHsBaSlicer/`, `base/`, `logger/`, `paths/`, `convert/`, `pipelinetypes/`, `2D/`, `meshmodel/`, `support/`).
- Generated protobuf headers and multi-language outputs (`cpp/cs/java/python/php`) installed under `share/HsBaSlicer/proto/` when `HSBA_PROTOBUF_OUT` is ON.
- `cmake/deploy_dlls.cmake` helper copies PDB symbols next to executables (vcpkg's `applocal.ps1` handles third-party DLLs).

## Conventions Observed

- Every subdirectory has its own `CMakeLists.txt`; the top-level file only composes them.
- Optional features are exposed as CMake cache options (`HSBA_*`) AND gated behind vcpkg manifest features (`copyleft`, `rest-api`); flipping the option alone is insufficient — the corresponding vcpkg feature must be installed.
- Platform gating follows a consistent pattern: `if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE)` before enabling heavy desktop-only dependencies (OpenCASCADE, OpenVDB, sqlpp11 MySQL/PostgreSQL, OpenCV, CGAL).
- Debug builds add `DISABLE_BOOLEAN_OPERATIONS` and reduce `HSBA_MODEL_POOL_SIZE` to 1 to avoid slow/slow-memory CGAL/IGL operations.
- Tests are disabled on Android/iOS/game consoles (`HSBA_SLICER_USE_TESTS OFF`) and automatically disabled when cross-compiling (`HSBA_SLICER_RUN_TESTS_AFTER_BUILD OFF`).
- The REST API server is explicitly documented as not part of default features and requires both `HSBA_BUILD_REST_API=ON` and the `rest-api` vcpkg feature.