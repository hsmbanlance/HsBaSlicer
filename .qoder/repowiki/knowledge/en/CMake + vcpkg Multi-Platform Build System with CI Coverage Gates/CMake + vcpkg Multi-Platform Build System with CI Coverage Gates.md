---
kind: build_system
name: CMake + vcpkg Multi-Platform Build System with CI Coverage Gates
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
    - cmake/coverage.cmake
    - version/CMakeLists.txt
    - static_check/feature_check.cmake
---

# HsBaSlicer Build System

## Approach and Toolchain

The project is a C++20 (C11/C23) multi-target CMake project (`cmake_minimum_required 3.28`) built on top of **vcpkg** as the dependency manager, with **Ninja** as the default generator on Windows/Linux/macOS and **Xcode** for iOS. The root `CMakeLists.txt` orchestrates feature detection, optional subsystem toggles, subdirectory inclusion, install/export rules, and test/doc enablement; each module directory has its own `CMakeLists.txt`.

### Dependency management
- `vcpkg.json` declares all third-party libraries (Boost, Eigen3, libigl, protobuf, RapidJSON, tinyxml2, yaml-cpp, Lua, OpenSSL, sqlpp11, miniz, bit7z, fontconfig, freetype, libjpeg-turbo, libpng, opencv, openvdb, magic-enum, clipper2).
- Two manifest features: `copyleft` (CGAL + libigl[cgal] + OpenCASCADE, switches project license to GPL-3.0-or-later) and `rest-api` (cpp-httplib with openssl). `default-features` enables `copyleft` on `windows | linux | osx | android`.
- Platform-scoped dependencies: e.g. `sqlpp11` uses only `sqlite3` on `android | ios`, full `SQLite3 MySQL PostgreSQL` on desktop.
- `vcpkg-configuration.json` pins the vcpkg baseline; `overlay-ports/` and `triplets/` provide custom ports/triplets.

### Cross-compilation targets
`CMakePresets.json` defines per-host configure presets:
- Windows: `x64-debug`, `x64-release` (MSVC `cl.exe`, Ninja)
- Linux: `linux-debug`, `linux-release`, `linux-coverage` (GCC/Clang via vcpkg toolchain)
- macOS: `macos-debug`, `macos-release`
- Android: `android-release` (`arm64-android`, NDK chainload, API 28)
- iOS: `ios-release` (Xcode generator, arm64, deployment target 26.5)

All presets set `binaryDir = out/build/${presetName}` and `installDir = out/install/${presetName}`, and pass `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` in debug builds.

### Feature gates in root CMake
- `HSBA_COVERAGE` — gcov/gcovr instrumentation and `coverage` target (core ≥45%, other ≥25% thresholds).
- `HSBA_SLICER_MODULE` — optional C++20 module wrapper (`ModuleHsBaSlicer`), gated by compiler support and TU-local exposure check.
- `HSBA_BUILD_REST_API` — optional `HsBaSlicerRestServer` executable; requires `VCPKG_MANIFEST_FEATURES` to include `rest-api` (checked at configure time).
- `HSBA_COPL` (AUTO/ON/OFF) — copyleft kernel linkage; AUTO probes CGAL/OpenCASCADE presence or env vars.
- `HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE` — platform macros derived from `CMAKE_SYSTEM_NAME` and `VCPKG_TARGET_TRIPLET`.
- `BUILD_SHARED_LIBS` — defaults to ON on Windows/Xbox dynamic triplets, OFF on Linux/Android/static triplets.
- `HSBA_USE_DLL_LOADER`, `HSBA_HSBA_USE_BIT7Z`, `HSBA_ENABLE_THREAD_POOL_COROUTINE`, `HSBA_PROTOBUF_OUT`, `HSBA_SLICER_USE_TESTS`, `HSBA_SLICER_RUN_TESTS_AFTER_BUILD`, `HSBA_SLICER_BUILD_SAMPLES`, `HSBA_SLICER_BUILD_DOCS`.

### Output layout
- All binaries/libs go to `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`.
- `utils/logcfg.ini` and (if enabled) `restapi/restapi.ini` are copied into the build output dir at configure time.
- Install tree follows GNUInstallDirs; public headers under `HsBaSlicer/` mirror the source layout; CMake package config files (`HsBaSlicerConfig.cmake`, `HsBaSlicerTargets.cmake`, version file) are installed under `${CMAKE_INSTALL_LIBDIR}/cmake/HsBaSlicer`.

### Versioning
`version/CMakeLists.txt` runs `Generator_Version.ps1` (requires PowerShell) at configure time to generate `version.cpp` embedding build type, VCPKG triplet, manifest features, and timestamp. The generated static library `HsBaVersion` is linked by consumers.

### Static checks
`static_check/feature_check.cmake` runs compile-time probes for C++ concepts, ranges, source_location, NTTP class/float, template-template matching, coroutines, explicit-this, C23 constexpr, and modules TU-local exposure. Missing required features cause `FATAL_ERROR`; missing optional ones define `HSBA_NO_*` fallback macros.

## CI Pipeline

GitHub Actions workflows under `.github/workflows/`:
- `build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml` — per-platform build jobs using `easimon/maximize-build-space@v10`, apt-installed system deps, vcpkg bootstrap, binary cache (`x-gha,readwrite`), and `ctest`.
- `test-coverage.yml` — runs the `linux-coverage` preset (gcov-instrumented Debug), executes tests via `ctest --preset linux-coverage`, then runs `cmake --build --preset linux-coverage-report` which invokes gcovr against two threshold buckets (core ≥45%, other ≥25%). Results are written to the job summary and uploaded as artifacts.
- `docker-rest-api.yml` — builds the Docker image from `docker/Dockerfile`.
- `rest-webui.yml`, `publish-wiki.yml`, `cmake-multi-platform.yml` — auxiliary workflows.

## Containerization

`docker/Dockerfile` is a two-stage Ubuntu-based image building `HsBaSlicerRestServer`:
- Stage 1 (`builder`): installs the same apt dependency set as `build-linux.yml`, bootstraps vcpkg, pre-warms cmake/ninja binaries into `/root/.cache/vcpkg/downloads`, configures via `cmake --preset linux-release -DHSBA_BUILD_REST_API=ON -DVCPKG_MANIFEST_FEATURES=${HSBA_FEATURES} -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON` (with retry loop up to 5 attempts), builds and installs to `/opt/hsba-install`.
- Stage 2 (`runtime`): minimal runtime with only shared-library DT_NEEDED deps (`libssl3t64`, zlib, libpng, fontconfig, freetype, mysqlclient, postgresql client, sqlite3), copies installed artifacts, applies `restapi.ini` (host→0.0.0.0), exposes port 8080, sets `HSBA_REST_HOST`/`HSBA_REST_PORT`/`LD_LIBRARY_PATH`, and includes a HEALTHCHECK against `/api/health`.

Build args: `HSBA_FEATURES` (default `rest-api`, can be `rest-api;copyleft`), `VCPKG_BASELINE`, `JOBS`, `APT_MIRROR`. The Dockerfile enforces `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` so copyleft kernels must be explicitly requested.

## Conventions and Rules

- **Compiler minimum**: C++20 is mandatory; C standard is 11 unless the compiler supports C23 (GCC ≥13, Clang ≥16, MSVC ≥19.36). C++20 modules require GCC ≥16 due to TU-local entity exposure.
- **Required C++ features** (enforced via `message(FATAL_ERROR)`): concepts, ranges, `std::source_location`, class-type NTTP, template-template parameter matching (P0522R0).
- **PowerShell is required** at configure time for `version/Generator_Version.ps1`; absence fails configuration.
- **REST API is desktop-only**: `HSBA_BUILD_REST_API` is forced OFF on non-desktop platforms; enabling it also requires the `rest-api` vcpkg manifest feature (checked both via `VCPKG_MANIFEST_FEATURES` env var and `find_package(httplib)`).
- **Copyleft kernels switch license**: when `HSBA_COPL` is ON, `add_compile_definitions(HSBA_COPL)` is emitted and the vcpkg feature's `license: GPL-3.0-or-later` takes effect.
- **Tests disabled on mobile/game-console**: `HSBA_SLICER_USE_TESTS` is forced OFF for ANDROID/IOS/HSBA_GAME_CONSOLE; `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` is forced OFF when cross-compiling.
- **Coverage thresholds are enforced**: `cmake/coverage.cmake` defines core filters (`base`, `2D`, `utils`, `cipher`, `meshmodel`, `paths`, `preprocess`, `support`, `LibHsBaSlicer`) and excludes (`tests`, `samples`, `static_tests`, `static_check`, `third_parts_without_vcpkg`, `docs`, `out`, `build`). The `coverage` custom target calls gcovr with `--fail-under-line` for both buckets and exits non-zero on failure.
- **Binary caching**: CI caches `vcpkg/downloads` keyed by `hashFiles('vcpkg.json')` and uses `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite` for GitHub-hosted artifact caching.
- **Output directories are unified**: `CMAKE_RUNTIME_OUTPUT_DIRECTORY` and `CMAKE_LIBRARY_OUTPUT_DIRECTORY` both point to `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`.
- **Windows UTF-8 code page**: `cmake/HsBaWindowsUtf8.cmake` provides `hsba_apply_utf8_codepage(<target>)` to embed an activeCodePage UTF-8 manifest for executables owned by this repo.