---
kind: build_system
name: CMake + vcpkg Multi-Platform Build System with CI Gates
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - vcpkg-configuration.json
    - .github/workflows/cmake-multi-platform.yml
    - .github/workflows/build-linux.yml
    - cmake/coverage.cmake
    - version/CMakeLists.txt
    - version/Generator_Version.ps1
    - static_check/feature_check.cmake
---

## Overview

HsBaSlicer is a C++20 project built entirely with **CMake (≥3.28)** and **vcpkg** as the dependency manager. The build targets Windows, Linux, macOS, Android (arm64), and iOS (arm64) through a single top-level `CMakeLists.txt` plus per-subdirectory CMake files. CI runs on GitHub Actions across all platforms.

## Core Build Files

- `CMakeLists.txt` — root project definition, feature detection, dependency discovery, subdirectory wiring, install/export rules.
- `CMakePresets.json` — named configure/build/test presets for each platform (windows/linux/macos/android/ios, debug/release/coverage).
- `vcpkg.json` / `vcpkg-configuration.json` — dependency manifest and registry configuration (baseline commit pinned to `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`, overlay ports/triplets under `./overlay-ports` and `./triplets`).
- `.github/workflows/*.yml` — CI pipelines: `cmake-multi-platform.yml` (matrix build + android + ios), `build-linux.yml`, `test-coverage.yml`, `publish-wiki.yml`, plus per-platform helpers.
- `cmake/coverage.cmake` — gcov/gcovr instrumentation and threshold gating.
- `version/CMakeLists.txt` + `version/Generator_Version.ps1` — PowerShell-driven version.cpp generation at configure time.
- `static_check/feature_check.cmake` — compile-time feature probes (concepts, ranges, source_location, NTTP, template-template matching, coroutines, explicit-this, C23 constexpr).

## Architecture & Conventions

### Compiler & Language
- `CMAKE_CXX_STANDARD = 20`, `CMAKE_C_STANDARD` auto-detected (C23 when compiler supports it, else C11). `CMAKE_POSITION_INDEPENDENT_CODE = ON` globally so static libs can be linked into shared libraries.
- MSVC forced to UTF-8 via `/utf-8`; Windows executables get an embedded activeCodePage=65001 manifest (`cmake/HsBaWindowsUtf8.cmake`).

### Feature Detection Gate
The root CMake runs `check_cxx_feature_*` functions from `static_check/feature_check.cmake`. Missing required features (concepts, ranges, source_location, template-template matching) cause `message(FATAL_ERROR ...)`. Optional features set compile definitions like `HSBA_HAS_CXX_COROUTINES`, `HSBA_HAS_NTTP_FP`, `HSBA_HAS_C_GENERIC`, `HSBA_HAS_CXX_MODULES`.

### Platform Switching
- `VCPKG_TARGET_TRIPLET` drives major switches: game console triplets (`xbox`, `switch`, `playstation`, `stadia`) set `HSBA_IS_GAME_CONSOLE` / `HSBA_GAME_CONSOLE_<TRIPLET>`; otherwise `HSBA_DESKTOP` or `HSBA_MOBILE` are set.
- Per-feature toggles: `HSBA_COPL` (copyleft kernels CGAL/OpenCascade, tri-state AUTO/ON/OFF — AUTO probes installed packages or env vars), `HSBA_USE_DLL_LOADER` (desktop only), `HSBA_HSBA_USE_BIT7Z` (desktop only), `HSBA_SLICER_MODULE` (C++20 modules, optional), `HSBA_POLYGON_DUMP` (desktop-only SVG dump utilities).

### Dependency Discovery
All third-party deps are found via `find_package(... CONFIG REQUIRED)` after vcpkg toolchain injection:
- Required: Boost (log/log_setup/dll/locale/...), Clipper2 (pkg-config), miniz, protobuf, rapidjson, eigen3, openssl, libigl, lua, tinyxml2, yaml-cpp, magic_enum.
- Conditional: OpenCV (not iOS/game console), CGAL+OpenCASCADE (only when `HSBA_COPL`), OpenVDB (not mobile/console), sqlpp11 (SQLite3+MySQL+PostgreSQL on desktop, SQLite3-only on mobile).

### Output Layout
- `CMAKE_RUNTIME_OUTPUT_DIRECTORY` and `CMAKE_LIBRARY_OUTPUT_DIRECTORY` both point to `${CMAKE_BINARY_DIR}/bin/$<CONFIG>` so binaries and shared libs land side-by-side.
- `CMAKE_DEBUG_POSTFIX = "d"`.
- `logcfg.ini` is copied into the output dir at configure time.

### Shared vs Static Libraries
`BUILD_SHARED_LIBS` defaults ON but is overridden by vcpkg triplet heuristics: Windows/Xbox → dynamic; Linux/Android/static-triplet → static; iOS always static. All internal static libraries are added to `_HSBA_INSTALL_TARGETS` for export.

### Install & Export
- Targets exported under namespace `HsBaSlicer::` via `install(EXPORT HsBaSlicerTargets ...)`.
- `HsBaSlicerConfig.cmake` and `HsBaSlicerConfigVersion.cmake` generated with `SameMajorVersion` compatibility.
- Public headers installed under `${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/<subdir>` mirroring the library layout (DllHsBaSlicer C API, LibHsBaSlicer C++ API, base, logger, paths, meshmodel, support, convert, pipelinetypes, proto generated files).
- Proto multi-language outputs (cpp/cs/java/python/php) installed under `${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/<lang>` when `HSBA_PROTOBUF_OUT` is ON.

### Tests
- Runtime tests: `tests/` directory, enabled by `HSBA_SLICER_USE_TESTS` (OFF on Android/iOS/game consoles). Executed via `ctest` after build unless cross-compiling.
- Static tests: `static_tests/` directory, enabled by `HSBA_SLICER_USE_STATIC_TESTS`.
- Coverage: `HSBA_COVERAGE` enables `--coverage` on all targets and adds a `coverage` custom target that runs gcovr with two thresholds: `HSBA_COVERAGE_THRESHOLD_CORE=45%` (base, 2D, utils, cipher, meshmodel, paths, preprocess, support, LibHsBaSlicer) and `HSBA_COVERAGE_THRESHOLD_OTHER=25%` for everything else. Excludes `tests/samples/static_tests/static_check/third_parts_without_vcpkg/docs/out/build/vcpkg_installed/vcpkg/usr/`.

### Version Generation
At configure time, `version/CMakeLists.txt` invokes `Generator_Version.ps1` (requires PowerShell) to generate `version.cpp` embedding build type, timestamp, VCPKG_TARGET_TRIPLET, and copyleft status. Failure to find PowerShell is a fatal error.

## CI Pipeline (GitHub Actions)

- `cmake-multi-platform.yml`: matrix over `windows-latest`, `ubuntu-latest`, `macos-latest` with Release builds; separate `build-android` job (NDK r27d, arm64-v8a, android-28, Gradle 8.7, APK artifact); separate `build-ios` job (arm64, deployment target 26.5, verifies static .a artifacts and Swift bridge header).
- All jobs bootstrap vcpkg, set `VCPKG_ROOT`, enable `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`, and cache `vcpkg/downloads` keyed by `hashFiles('vcpkg.json')`.
- Linux uses gcc, macOS uses clang; Windows uses MSVC via `vcvarsall.bat x64`.
- `test-coverage.yml` triggers the `linux-coverage` preset which compiles with `HSBA_COVERAGE=ON`, runs tests, then executes the `coverage` target to enforce the 45%/25% thresholds.
- `build-linux.yml` is a manual-dispatch pipeline mirroring the matrix's Linux path.

## Key Constraints Enforced by the Build

1. **C++20 concepts, ranges, std::source_location, and P0522R0 template-template matching are mandatory** — missing any causes `FATAL_ERROR` in the root CMake.
2. **PowerShell is required** — `version/CMakeLists.txt` fails if neither `pwsh` nor `powershell` is found.
3. **Copyleft license switch** — enabling `HSBA_COPL` (AUTO/ON) switches the project license to GPL-3.0-or-later; disabling keeps MIT.
4. **Coverage gates fail the build** — `cmake/coverage.cmake` uses `--fail-under-line` with 45% core / 25% other thresholds; the `coverage` custom target exits non-zero on regression.
5. **Tests disabled on mobile/console** — `HSBA_SLICER_USE_TESTS` is forced OFF for ANDROID, IOS, HSBA_GAME_CONSOLE.
6. **Cross-compilation disables test execution** — `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` is forced OFF when `CMAKE_CROSSCOMPILING` is true.