---
kind: build_system
name: CMake + vcpkg Cross-Platform Build System with GitHub Actions CI
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
    - cmake/coverage.cmake
    - version/Generator_Version.ps1
---

## Overview

HsBaSlicer is a C++20 framework built entirely with **CMake ≥ 3.28** and **vcpkg** as its dependency manager. The top-level `CMakeLists.txt` composes ~15 sub-libraries (base, cipher, utils, logger, 2D, paths, fileoperator, meshmodel, cadmodel, pointcloud, preprocess, support, pipelinetypes, convert, LibHsBaSlicer, DllHsBaSlicer, HsBaSlicer) plus samples, tests, static_tests, docs, and version generation. There are no Makefiles or shell build scripts — all entry points go through CMake.

## Dependency Management: vcpkg

`vcpkg.json` declares the full dependency graph:
- Core: Boost (locale, log, dll, multiprecision, nowide, date-time, property-tree, test, units, uuid, pfr, graph), Clipper2, libjpeg-turbo, libpng, OpenCV, Eigen3, libigl, FreeType, miniz, OpenVDB, Protobuf, RapidJSON, tinyxml2, yaml-cpp, Lua, OpenSSL, magic_enum.
- Platform-scoped dependencies via the `platform:` key (e.g. `windows | linux | osx`, `android | ios`).
- A `copyleft` feature that pulls in CGAL, libigl[cgal], and OpenCASCADE; enabling it switches the project license from MIT to GPL-3.0-or-later.
- `vcpkg-configuration.json` pins the vcpkg toolchain; `overlay-ports/` and `triplets/` allow local overrides.

The root CMake applies `find_package(... CONFIG REQUIRED)` for every dependency after vcpkg bootstraps them via the toolchain file (`$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`).

## CMake Configuration & Feature Gating

Key global settings in `CMakeLists.txt`:
- `CMAKE_CXX_STANDARD = 20`, `CMAKE_POSITION_INDEPENDENT_CODE = ON`, `CMAKE_DEBUG_POSTFIX = "d"`.
- Compiler-specific C standard: C11 by default, upgraded to C23 when GCC ≥ 13 / Clang ≥ 16 / MSVC ≥ 19.36.
- UTF-8 source encoding forced on MSVC (`/utf-8`).
- Static feature detection via `static_check/feature_check.cmake` runs at configure time; missing concepts, ranges, source_location, NTTP class, or template-template matching aborts the build (`message(FATAL_ERROR ...)`).
- Optional features gated by compile definitions: `HSBA_HAS_CXX_COROUTINES`, `HSBA_HAS_CXX_EXPLICIT_THIS`, `HSBA_HAS_C23_CONSTEXPR`, `HSBA_HAS_CXX_MODULES`, `HSBA_HAS_NTTP_FP`, `HSBA_HAS_C_GENERIC`.

Platform triage sets cache booleans `HSBA_DESKTOP`, `HSBA_MOBILE`, `HSBA_GAME_CONSOLE` based on `CMAKE_SYSTEM_NAME` and `VCPKG_TARGET_TRIPLET`. Game-console triplets (`xbox`, `switch`, `playstation`, `stadia`) also define `HSBA_IS_GAME_CONSOLE` and `HSBA_GAME_CONSOLE_<TRIPLET>`.

Feature toggles controlled by `option()` variables:
- `HSBA_COPL` (AUTO|ON|OFF): copyleft kernels (CGAL/OpenCascade); AUTO probes `VCPKG_MANIFEST_FEATURES`, `VCPKG_MANIFEST_NO_DEFAULT_FEATURES`, then `find_package(CGAL|OpenCASCADE)`.
- `HSBA_SLICER_MODULE`: builds the C++20 module wrapper only if the toolchain supports modules.
- `HSBA_USE_DLL_LOADER`: disabled on Android/iOS/non-desktop.
- `HSBA_HSBA_USE_BIT7Z`: disabled on non-desktop.
- `HSBA_ENABLE_THREAD_POOL_COROUTINE`, `HSBA_POLYGON_DUMP`, `HSBA_PROTOBUF_OUT`, `BUILD_SHARED_LIBS`.
- `BUILD_SHARED_LIBS` defaults are overridden by `VCPKG_TARGET_TRIPLET` patterns: Windows/Xbox → shared; Linux/Android/static → static.

## Output Layout & Install

All artifacts land under a unified layout:
```
CMAKE_RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
CMAKE_LIBRARY_OUTPUT_DIRECTORY = ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
```
The `utils/logcfg.ini` is copied into the output directory at configure time.

`install()` targets expose:
- Public headers under `${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/{DllHsBaSlicer,LibHsBaSlicer,base,fileoperator,logger,pipelinetypes,2D,meshmodel,support,paths,convert,proto}`.
- Generated protobuf headers from `${CMAKE_CURRENT_BINARY_DIR}/proto/`.
- Multi-language protobuf outputs (`cpp/cs/java/python/php`) under `${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/<lang>` when `HSBA_PROTOBUF_OUT=ON`.
- CMake package config: `HsBaSlicerTargets.cmake` with namespace `HsBaSlicer::`, plus `HsBaSlicerConfig.cmake` and `HsBaSlicerConfigVersion.cmake` (SameMajorVersion compatibility).

## Presets & Cross-Compilation

`CMakePresets.json` defines platform presets using Ninja (Xcode on iOS):
- `windows-base` → `x64-debug` / `x64-release` (MSVC `cl.exe`).
- `linux-debug` / `linux-release` / `linux-coverage` (GCC/Clang via vcpkg).
- `macos-debug` / `macos-release`.
- `ios-release` (Xcode generator, `arm64`, deployment target `iphoneos`, `CMAKE_OSX_DEPLOYMENT_TARGET=26.5`).
- `android-release` (`arm64-android`, NDK chainload, API level 28).

Every preset uses `${sourceDir}/out/build/${presetName}` and `${sourceDir}/out/install/${presetName}` as binary/install directories. Coverage preset additionally enables `HSBA_COVERAGE`, disables samples/docs/static_tests/test execution, and sets thresholds `HSBA_COVERAGE_THRESHOLD_CORE=45`, `HSBA_COVERAGE_THRESHOLD_OTHER=25`.

## Testing

- Runtime tests: Boost.Test suite under `tests/`, enabled via `HSBA_SLICER_USE_TESTS` (off on Android/iOS/game consoles). Executed via `ctest`; disabled automatically during cross-compilation (`CMAKE_CROSSCOMPILING`).
- Compile-time tests: `static_tests/` exercising constexpr/template helpers via `static_assert`.
- Coverage gating: `cmake/coverage.cmake` adds `--coverage` to all targets and creates a `coverage` custom target that runs gcovr twice — once filtering core dirs (`base`, `2D`, `utils`, `cipher`, `meshmodel`, `paths`, `preprocess`, `support`, `LibHsBaSlicer`) against a 45% threshold, once excluding core dirs against a 25% threshold. Excludes `tests/samples/static_tests/static_check/third_parts_without_vcpkg/docs/out/build/vcpkg_installed/usr`.

## Version Generation

`version/Generator_Version.ps1` is invoked at configure time to produce `version/version.cpp` from `version/version.hpp.in`. It reads `vcpkg.json`, resolves the target platform from `VCPKG_TARGET_TRIPLET`, enumerates active dependencies (including `copyleft` feature deps), looks up each library's license/homepage from an embedded map, and emits a `VersionInfo` struct containing `librariesName`, `license` (MIT vs GPL-3.0-or-later derived from whether any GPL/LGPL dep was found), `version`, `buildType`, `buildPlatform`, `configureTime`, and `vcpkgTargetTriplet`.

## CI Pipeline (GitHub Actions)

Workflows under `.github/workflows/`:
- `build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml`: per-platform build jobs.
- `test-coverage.yml`: PR-triggered coverage job on Ubuntu, running the `linux-coverage` preset, executing tests, then building the `coverage` target to enforce thresholds. Parallelism is capped to `CMAKE_BUILD_PARALLEL_LEVEL=2` and `VCPKG_MAX_CONCURRENCY=2` because gcov-instrumented Debug TUs of CAD-heavy modules (OCCT/CGAL/VTK) can consume several GB RAM.
- `cmake-multi-platform.yml`: multi-platform matrix build.
- `publish-wiki.yml`: publishes documentation.

Each workflow installs a large apt dependency set (X11/GL dev libs, meson, nasm, ruby, guile, wayland protocols, etc.), clones and bootstraps vcpkg, caches `vcpkg/downloads` keyed by `hashFiles('vcpkg.json')`, and uses the GitHub Actions binary cache backend (`VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`).

## Conventions Observed

- Every source tree directory has its own `CMakeLists.txt` declaring one library/executable target; the root aggregates them via `add_subdirectory()`.
- Dependencies are discovered exclusively through `find_package(... CONFIG REQUIRED)` (or `pkg_check_modules` for Clipper2), never via manual path manipulation.
- Platform-specific code is selected via CMake cache booleans (`HSBA_DESKTOP`, `HSBA_MOBILE`, `HSBA_GAME_CONSOLE`, `HSBA_COPL`) rather than preprocessor checks alone.
- Optional third-party backends (bit7z, CGAL/OpenCASCADE, OpenVDB, sqlpp11 MySQL/PostgreSQL) are guarded by both `if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE ...)` blocks and corresponding `#ifdef` compile definitions.
- Tests are disabled on mobile/console platforms and during cross-compilation; they are always run after build unless explicitly turned off.
- The install tree follows GNUInstallDirs conventions and exposes a consumable CMake package (`HsBaSlicer::` namespace).
- All generated artifacts (protobuf, version.cpp) live under `${CMAKE_CURRENT_BINARY_DIR}` and are installed separately from source headers.