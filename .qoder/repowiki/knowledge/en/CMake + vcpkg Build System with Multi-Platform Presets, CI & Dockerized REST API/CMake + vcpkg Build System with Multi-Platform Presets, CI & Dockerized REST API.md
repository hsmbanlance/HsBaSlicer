---
kind: build_system
name: CMake + vcpkg Build System with Multi-Platform Presets, CI & Dockerized REST API
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/docker-rest-api.yml
    - docker/Dockerfile
    - version/Generator_Version.ps1
    - cmake/coverage.cmake
    - cmake/HsBaWindowsUtf8.cmake
    - cmake/deploy_dlls.cmake
---

## Overview

HsBaSlicer is a C++20 multi-platform framework built entirely with **CMake ≥ 3.28** and **vcpkg** as the dependency manager. The top-level `CMakeLists.txt` configures global compiler standards (C++20, C11 or C23 depending on toolchain), runs compile-time feature probes via `static_check/feature_check.cmake`, gates optional subsystems behind cache options, and wires together ~15 subprojects plus samples, tests, docs and install/export targets.

## Toolchain & Dependency Management

- **vcpkg manifest** (`vcpkg.json`) declares all third-party libraries; default features include `copyleft` (CGAL, libigl[cgal], OpenCascade) which switches the project license from MIT to GPL-3.0-or-later. Optional `rest-api` feature pulls in `cpp-httplib` (with openssl TLS support).
- Platform scoping is expressed per-dependency via `platform:` expressions (e.g. `windows | linux | osx | android`, `!uwp`).
- A custom PowerShell script `version/Generator_Version.ps1` parses `vcpkg.json` at configure time, evaluates platform expressions and enabled features, and generates `version.cpp` containing runtime version info, build metadata, and a list of third-party libraries with their licenses — used to derive the project's own license string.
- `cmake/HsBaWindowsUtf8.cmake` provides `hsba_apply_utf8_codepage(<target>)` to embed an `<activeCodePage>UTF-8</activeCodePage>` manifest on Windows executables so narrow-char APIs handed to libigl/CGAL/OpenCASCADE/OpenVDB/miniz interpret paths as UTF-8.
- `cmake/deploy_dlls.cmake` copies PDB symbols next to executables (vcpkg applocal handles third-party DLLs).
- `cmake/coverage.cmake` instruments every target with `--coverage` and adds a `coverage` target that enforces line-coverage thresholds (core modules ≥ 45%, other modules ≥ 25%) via gcovr.

## CMake Options & Feature Gating

Key cache variables in the root `CMakeLists.txt`:

| Option | Default | Effect |
|---|---:|---|
| `HSBA_COVERAGE` | OFF | Enable gcov instrumentation + coverage target |
| `HSBA_SLICER_MODULE` | ON | Build C++20 module wrapper (`ModuleHsBaSlicer`) if toolchain supports it |
| `HSBA_BUILD_REST_API` | OFF | Build `HsBaSlicerRestServer`; requires `rest-api` vcpkg feature AND desktop platform |
| `HSBA_COPL` | AUTO | Link CGAL/OpenCascade; can be forced ON/OFF or auto-detected via vcpkg manifest / package probe |
| `HSBA_USE_DLL_LOADER` | ON (desktop) | Disabled on Android/iOS/game consoles |
| `HSBA_HSBA_USE_BIT7Z` | ON (desktop) | Zipper backend |
| `BUILD_SHARED_LIBS` | ON | Driven by `VCPKG_TARGET_TRIPLET`: windows→shared, linux/android/static→static |
| `HSBA_SLICER_USE_TESTS` | ON | Disabled on Android/iOS/game consoles |
| `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` | ON | Skipped during cross-compilation |
| `HSBA_SLICER_BUILD_SAMPLES` | ON | Builds FDM/SLA/SLS/SLM/LOM/TDP/WAAM/Custom/LibHsBaSlicer sample executables |
| `HSBA_SLICER_BUILD_DOCS` | ON | Requires Doxygen |

Compiler capability probing via `check_cxx_feature_*` macros gates code paths through `HSBA_HAS_CXX_COROUTINES`, `HSBA_HAS_CXX_EXPLICIT_THIS`, `HSBA_HAS_C23_CONSTEXPR`, `HSBA_HAS_NTTP_FP`, `HSBA_NO_NTTP_FP`, `HSBA_HAS_C_GENERIC`, `HSBA_NO_C_GENERIC`. Game-console builds are detected from triplets matching `xbox|switch|playstation|stadia` and define `HSBA_IS_GAME_CONSOLE` / `HSBA_GAME_CONSOLE_<TRIPLET>`.

## Output Layout & Install

All artifacts land under `out/build/<preset>/bin/<CONFIG>` (runtime + shared libs). Install layout uses GNUInstallDirs:
- Libraries → `${CMAKE_INSTALL_LIBDIR}`
- Executables → `${CMAKE_INSTALL_BINDIR}`
- Public headers installed under `${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/{DllHsBaSlicer,LibHsBaSlicer,fileoperator,base,logger,meshmodel,support,paths,convert,proto}`
- Generated protobuf outputs (pb.h plus cpp/cs/java/python/php bindings) go under `${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto`
- CMake package config files (`HsBaSlicerConfig.cmake`, `HsBaSlicerConfigVersion.cmake`) exported with namespace `HsBaSlicer::` for downstream `find_package(HsBaSlicer)` usage.

## Cross-Compilation Presets

`CMakePresets.json` defines per-host presets using Ninja (Xcode for iOS):

- `windows-base` → x64 Debug/Release (MSVC)
- `linux-debug/release/coverage` (Ninja, WSL-friendly)
- `android-release` (arm64-android, NDK chainload, api level 28)
- `macos-debug/release` (Darwin)
- `ios-release` (Xcode generator, arm64, deployment target 26.5)

Coverage preset sets `HSBA_COVERAGE=ON`, `HSBA_COVERAGE_THRESHOLD_CORE=45`, `HSBA_COVERAGE_THRESHOLD_OTHER=25`, and disables samples/docs/static-tests/test execution.

## CI Pipelines (GitHub Actions)

Per-platform workflows live under `.github/workflows/`:

- `build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml` — clone vcpkg, bootstrap, set `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`, cache `vcpkg/downloads` keyed by `vcpkg.json`, then `cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=...` and `cmake --build`.
- `test-coverage.yml` — full unit-test suite in the `linux-coverage` preset, runs `ctest --preset linux-coverage`, then `cmake --build --preset linux-coverage-report` to enforce the 45%/25% thresholds; results written to GitHub job summary and uploaded as `coverage-reports` artifact.
- `docker-rest-api.yml` — builds the Docker image described below.
- `rest-webui.yml`, `publish-wiki.yml`, `cmake-multi-platform.yml` — auxiliary flows.

All Linux runners use `easimon/maximize-build-space@v10` to free disk space before building heavy dependencies (OpenCASCADE/CGAL/VTK).

## Containerization

`docker/Dockerfile` is a two-stage Ubuntu-based image producing `HsBaSlicerRestServer`:

- **Stage 1 (builder)**: installs the exact system dependency set mirrored in `build-linux.yml` (gperf, GTK dev headers, OpenGL/X11 Wayland deps, meson, ruby, guile, haskell-stack, golang, bluetooth-dev, etc.), bootstraps vcpkg, pre-warms cmake/ninja binaries into `/root/.cache/vcpkg/downloads`, then runs `cmake --preset linux-release` with `-DHSBA_BUILD_REST_API=ON -DVCPKG_MANIFEST_FEATURES=${HSBA_FEATURES} -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON` and `cmake --install` into `/opt/hsba-install`. Configure retries up to 5 times with 30s backoff to handle transient vcpkg network failures.
- **Stage 2 (runtime)**: minimal Ubuntu image installing only runtime shared-library ABI packages (`libssl3t64`, `zlib1g`, `libpng16-16t64`, `libfontconfig1`, `libfreetype6`, `libmysqlclient24`, `libpq5`, `sqlite3`), copies the installed binary + `restapi.ini` + `logcfg.ini`, sed-replaces `host = 127.0.0.1` → `0.0.0.0`, exposes port 8080, and runs with `ENTRYPOINT ["/bin/sh", "-c", "exec /usr/local/bin/HsBaSlicerRestServer --config ./restapi.ini --host ${HSBA_REST_HOST} --port ${HSBA_REST_PORT} $@"]`.
- Health check hits `/api/health` on loopback (auth bypassed for localhost per `restapi.ini`).

## Versioning Strategy

The project version is declared once in the root `project("HsBaSlicer" VERSION 1.0.0)` and propagated to the generated `version/version.hpp` via `configure_package_config_file` + `write_basic_package_version_file` (SameMajorVersion compatibility). At runtime, `version/Generator_Version.ps1` emits a `VersionInfo` struct populated with build type, host platform, vcpkg triplet, configure timestamp, and a dynamically computed list of third-party libraries parsed from `vcpkg.json` (including their licenses and homepages). The presence of any GPL/LGPL dependency flips the reported project license to `GPL-3.0-or-later`.

## Conventions Observed

- Every subdirectory has its own `CMakeLists.txt` declaring one library/executable target; the root file is purely a configuration aggregator.
- Optional features are exposed as `option(... CACHE BOOL ...)` and guarded both in CMake logic and via `add_compile_definitions(...)`, so source code branches on `#ifdef HSBA_*` flags.
- Desktop-only features (REST API, bit7z, OpenCV, OpenVDB, sqlpp11 MySQL/PostgreSQL, CGAL/OCCT) are disabled on `ANDROID`, `IOS`, and `HSBA_GAME_CONSOLE`.
- Tests are disabled on mobile/game-console platforms because test executables cannot run there; cross-compiling also disables post-build test execution.
- All static libraries linked into `DllHsBaSlicer.so` are built with `-fPIC` (`CMAKE_POSITION_INDEPENDENT_CODE ON`).
- Debug suffixes: `CMAKE_DEBUG_POSTFIX "d"` applied globally.
- vcpkg binary caching is standardized across CI via `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite` and `actions/cache@v5` keyed on `vcpkg.json`.