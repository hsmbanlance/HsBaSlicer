---
kind: build_system
name: CMake + vcpkg Build System with Multi-Platform Presets and Coverage Gates
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
    - cmake/HsBaWindowsUtf8.cmake
    - version/Generator_Version.ps1
    - android/app/build.gradle
---

## Approach

HsBaSlicer is a C++20 project built entirely with **CMake ≥ 3.28** (top-level `CMakeLists.txt`) and managed through **vcpkg** (`vcpkg.json`, `vcpkg-configuration.json`). There are no Makefiles, Dockerfiles, or shell build scripts at the repo root — all orchestration goes through CMake presets, GitHub Actions workflows, and a few helper CMake modules.

## Key Files

- `CMakeLists.txt` — top-level project: feature detection, dependency discovery, subdirectory aggregation, install/export rules.
- `CMakePresets.json` — per-platform configure/build/test presets (Windows x64 Debug/Release, Linux Debug/Release/Coverage, macOS Debug/Release, iOS Release, Android arm64 Release).
- `vcpkg.json` — dependency manifest with two features: `copyleft` (CGAL + libigl[cgal] + OpenCascade) and `rest-api` (cpp-httplib).
- `.github/workflows/*.yml` — CI pipelines for Windows/Linux/macOS/iOS/Android builds plus a coverage-gating workflow on PRs to `master`/`dev`.
- `cmake/coverage.cmake` — gcov/gcovr instrumentation and threshold gates (core ≥ 45%, other ≥ 25%).
- `cmake/HsBaWindowsUtf8.cmake` — UTF-8 active code page manifest helper for MSVC executables.
- `version/Generator_Version.ps1` — PowerShell script invoked at configure time to generate `version.cpp` from `vcpkg.json`, embedding build type, platform, triplet, timestamp, and third-party license metadata.
- `android/app/build.gradle` / `android/gradlew` — Gradle wrapper that consumes the prebuilt native libraries produced by the CMake build.

## Architecture & Conventions

### Dependency management
All third-party libraries are resolved via vcpkg; the top-level CMake uses `find_package(... CONFIG REQUIRED)` for each dependency (Boost, Clipper2, miniz, bit7z, Protobuf, RapidJSON, Eigen3, OpenSSL, OpenCV, libigl, CGAL/OpenCASCADE when `HSBA_COPL=ON`, OpenVDB, sqlpp11, Lua, tinyxml2, yaml-cpp, magic_enum). Platform gating is done both in `vcpkg.json` (`platform:` expressions like `windows | linux | osx | android`, negated `!windows & !linux & !osx & !android`) and in CMake `if(NOT ANDROID AND NOT IOS AND NOT HSBA_GAME_CONSOLE ...)` blocks.

### Feature flags
The root CMake defines cache options that gate entire subsystems:
- `HSBA_COPL` (AUTO/ON/OFF) — enables CGAL + OpenCascade and switches the project license to GPL-3.0-or-later.
- `HSBA_BUILD_REST_API` (OFF) — desktop-only REST server backed by cpp-httplib; requires the `rest-api` vcpkg feature.
- `HSBA_SLICER_MODULE` (ON) — optional C++20 module wrapper (`ModuleHsBaSlicer`).
- `HSBA_USE_DLL_LOADER` — disabled on non-desktop platforms.
- `HSBA_HSBA_USE_BIT7Z` — disabled on mobile/console.
- `HSBA_SLICER_USE_TESTS`, `HSBA_SLICER_BUILD_SAMPLES`, `HSBA_SLICER_BUILD_DOCS`, `HSBA_SLICER_USE_STATIC_TESTS`.

### Cross-compilation
Cross-compilation is driven by `VCPKG_TARGET_TRIPLET`. The root CMake maps triplets containing `xbox|switch|playstation|stadia` to `HSBA_IS_GAME_CONSOLE`; triplets matching `windows|xbox` default to shared libs, while `linux|android|static` default to static. Presets pass explicit toolchain files (`$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`) and platform-specific variables (e.g. `ANDROID_NDK`, `CMAKE_OSX_ARCHITECTURES`, `CMAKE_SYSTEM_NAME=iOS`).

### Output layout
All artifacts land under `out/build/<presetName>/bin/<CONFIG>` (runtime and library output directories unified via `CMAKE_RUNTIME_OUTPUT_DIRECTORY` / `CMAKE_LIBRARY_OUTPUT_DIRECTORY`). `logcfg.ini` and optionally `restapi.ini` are copied into the binary dir at configure time.

### Install & export
Targets are installed via `GNUInstallDirs` and exported as a CMake package under namespace `HsBaSlicer::` using `install(EXPORT HsBaSlicerTargets ...)`. Config/Version files (`HsBaSlicerConfig.cmake`, `HsBaSlicerConfigVersion.cmake`) are generated via `configure_package_config_file` / `write_basic_package_version_file` with `SameMajorVersion` compatibility. Public headers are explicitly enumerated per-subsystem.

### Versioning
Version info is not hard-coded: `version/Generator_Version.ps1` parses `vcpkg.json` at configure time, determines whether copyleft dependencies are present, and emits `version.cpp` declaring `GetVersionInfo()` with fields `librariesName`, `license` (MIT vs GPL-3.0-or-later), `version`, `buildType`, `buildPlatform`, `configureTime`, `vcpkgTargetTriplet`, and a list of third-party libraries with their licenses/homepages.

### Testing & coverage
Tests use Boost.Test (`tests/`) and are registered via `enable_testing()` at the top level. Static template checks live under `static_tests/`. Coverage is opt-in via `HSBA_COVERAGE` (or the `linux-coverage` preset); it injects `--coverage` compile/link flags and adds a `coverage` custom target that runs gcovr twice — once filtering core dirs (`base`, `2D`, `utils`, `cipher`, `meshmodel`, `paths`, `preprocess`, `support`, `LibHsBaSlicer`) against `HSBA_COVERAGE_THRESHOLD_CORE` (default 45%), and once excluding those dirs against `HSBA_COVERAGE_THRESHOLD_OTHER` (default 25%). The thresholds are documented as anti-regression baselines measured on the current suite.

### CI
GitHub Actions provide one workflow per host OS (`build-windows.yml`, `build-linux.yml`, `build-macos.yml`, `build-ios.yml`, `build-android.yml`) plus `test-coverage.yml` (PRs to `master`/`dev`). All workflows bootstrap vcpkg, cache `vcpkg/downloads` keyed on `vcpkg.json`, set `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`, then invoke `cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` (or `cmake --preset <name>` for coverage). The coverage workflow caps parallelism (`CMAKE_BUILD_PARALLEL_LEVEL=2`, `VCPKG_MAX_CONCURRENCY=2`) because gcov-instrumented Debug TUs of CAD-heavy modules can exhaust runner RAM.

## Conventions & Constraints

- **Compiler requirements**: C++ concepts, ranges, `std::source_location`, class-type NTTPs, and P0522R0 template-template matching are required; missing any triggers `message(FATAL_ERROR ...)` in the root CMake.
- **C++ standard**: `CMAKE_CXX_STANDARD = 20` (required); C standard is 23 when the compiler supports it (GCC ≥ 13, Clang ≥ 16, MSVC ≥ 19.36), otherwise 11.
- **Position-independent code**: `CMAKE_POSITION_INDEPENDENT_CODE ON` is forced globally so static libraries linked into shared ones are relocatable.
- **Feature detection**: Optional capabilities (coroutines, `explicit this`, C23 constexpr, C++20 modules) are probed via `static_check/feature_check.cmake` and exposed as `HSBA_HAS_*` compile definitions rather than failing the build.
- **Desktop/mobile/console gating**: Android/iOS/game-console builds disable tests, dynamic loader, bit7z, OpenCV, OpenVDB, SQL backends, and logger; these are enforced by `set(... FORCE CACHE BOOL ...)` blocks in the root CMake.
- **REST API gated by two knobs**: `HSBA_BUILD_REST_API=ON` alone is insufficient — the `rest-api` vcpkg feature must also be enabled (checked via `VCPKG_MANIFEST_FEATURES` env var or `find_package(httplib CONFIG QUIET)`), otherwise configuration fails with `FATAL_ERROR`.
- **Copyleft licensing**: Enabling `HSBA_COPL` (AUTO/ON) adds `USE_CGAL`, `USE_OCCT`, `USE_CAD_KERNEL`, `USE_OPENVDB`, `USE_IGL` definitions and changes the project license to GPL-3.0-or-later; the version generator mirrors this logic by inspecting `vcpkg.json` for GPL/LGPL dependencies.
- **Coverage thresholds are enforced**: `cmake/coverage.cmake` sets `--fail-under-line` for both core and other buckets; the `coverage` custom target fails the build if either threshold is missed.
- **Output directory convention**: All binaries and libraries go to `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`; install destinations follow `GNUInstallDirs` (`${CMAKE_INSTALL_BINDIR}`, `${CMAKE_INSTALL_LIBDIR}`).
- **UTF-8 on Windows**: Executables get an embedded `<activeCodePage>UTF-8</activeCodePage>` manifest via `hsba_apply_utf8_codepage(<target>)` so narrow-char APIs used by libigl/CGAL/OpenCASCADE/OpenVDB/miniz receive UTF-8 paths.