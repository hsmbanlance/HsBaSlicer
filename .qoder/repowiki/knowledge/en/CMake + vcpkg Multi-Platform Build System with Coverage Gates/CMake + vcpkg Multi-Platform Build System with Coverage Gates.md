---
kind: build_system
name: CMake + vcpkg Multi-Platform Build System with Coverage Gates
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-windows.yml
    - cmake/coverage.cmake
    - version/CMakeLists.txt
    - version/Generator_Version.ps1
    - cmake/HsBaWindowsUtf8.cmake
    - cmake/deploy_dlls.cmake
    - cmake/HsBaSlicerConfig.cmake.in
    - static_check/feature_check.cmake
---

# HsBaSlicer Build System

## Approach and Toolchain

HsBaSlicer is a C++20 project built entirely with **CMake ≥ 3.28** (top-level `CMakeLists.txt`) and managed through **vcpkg** (`vcpkg.json`, `vcpkg-configuration.json`). The build targets Windows, Linux, macOS, Android (arm64), and iOS via CMake presets in `CMakePresets.json`. All static libraries are compiled position-independent (`CMAKE_POSITION_INDEPENDENT_CODE ON`) so they can be linked into the shared `DllHsBaSlicer` artifact.

### Language and compiler requirements

- C++ standard: `CMAKE_CXX_STANDARD 20` (required).
- C standard: auto-detected per toolchain — C23 when available (GCC ≥ 13, Clang ≥ 16, MSVC ≥ 19.36), otherwise C11.
- UTF-8 source encoding enforced on MSVC via `/utf-8`.
- Feature detection runs at configure time through `static_check/feature_check.cmake`; missing required features (C++ concepts, ranges, `std::source_location`, template-template matching) cause `message(FATAL_ERROR ...)` and abort configuration.
- Optional feature macros (`HSBA_HAS_CXX_COROUTINES`, `HSBA_HAS_CXX_EXPLICIT_THIS`, `HSBA_HAS_C23_CONSTEXPR`, `HSBA_HAS_NTTP_FP`, `HSBA_HAS_C_GENERIC`, `HSBA_NO_NTTP_FP`, `HSBA_NO_C_GENERIC`) gate conditional code paths.

### Dependency management

All third-party libraries are declared in `vcpkg.json` and pulled in via the vcpkg CMake toolchain (`$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`). Dependencies include Boost (log, locale, dll, multiprecision, nowide, date-time, property-tree, test, units, uuid, pfr, graph), Clipper2, libigl, OpenCV, Eigen3, OpenSSL, Protobuf, RapidJSON, miniz, bit7z (optional), OpenVDB, sqlpp11 (SQLite3 / MySQL / PostgreSQL), tinyxml2, yaml-cpp, Lua, magic_enum, fontconfig, freetype, libjpeg-turbo, libpng.

The `copyleft` vcpkg feature (default for `windows | linux | osx | android`) pulls CGAL, libigl[cgal], and OpenCASCADE; enabling it switches the project license from MIT to GPL-3.0-or-later. The top-level CMake honors `HSBA_COPL` (AUTO/ON/OFF) and detects copyleft kernels by probing `VCPKG_MANIFEST_FEATURES`, `VCPKG_MANIFEST_NO_DEFAULT_FEATURES`, or whether CGAL/OpenCASCADE are already installed.

### Platform and target selection

- Desktop vs mobile vs game-console flags (`HSBA_DESKTOP`, `HSBA_MOBILE`, `HSBA_GAME_CONSOLE`) are derived from `CMAKE_SYSTEM_NAME` and `VCPKG_TARGET_TRIPLET`.
- Game-console triplets (`xbox`, `switch`, `playstation`, `stadia`) define `HSBA_IS_GAME_CONSOLE` and `HSBA_GAME_CONSOLE_<TRIPLET>`.
- Many optional subsystems are disabled on non-desktop platforms: dynamic library loader (`HSBA_USE_DLL_LOADER`), bit7z (`HSBA_HSBA_USE_BIT7Z`), OpenCV, CGAL/OCCT, OpenVDB, multi-database sqlpp11 backends.
- Static vs shared linking is driven by `BUILD_SHARED_LIBS`, which is forced based on `VCPKG_TARGET_TRIPLET`: Windows/Xbox → shared unless triplet contains `static`; Linux/Android/static triplets → static; everything else defaults to static.

### Output layout

All binaries, shared libraries, and resources land under `${CMAKE_BINARY_DIR}/bin/$<CONFIG>` (unified output directory). Debug artifacts get a `d` postfix (`CMAKE_DEBUG_POSTFIX "d"`). On Windows, an embedded `<activeCodePage>UTF-8</activeCodePage>` manifest makes narrow-char file APIs treat paths as UTF-8.

### Subtree composition

The root `CMakeLists.txt` adds subdirectories in dependency order: `third_parts_without_vcpkg` → `proto` → `base` → `cipher` → `utils` → `logger` (desktop only) → `2D` → `paths` → `fileoperator` → `meshmodel` → `cadmodel` (conditional) → `pointcloud` (conditional) → `preprocess` → `support` → `pipelinetypes` → `convert` → `LibHsBaSlicer` → `ModuleHsBaSlicer` (optional C++20 module) → `DllHsBaSlicer` → `HsBaSlicer` → `version` → samples → tests → static_tests → docs.

### Versioning

The `version/` subtree generates `version.cpp` at configure time by invoking `Generator_Version.ps1` (PowerShell required; build fails if `pwsh`/`powershell` is absent). The generated file embeds build type, timestamp, VCPKG_TARGET_TRIPLET, and copyleft status. It is compiled into a static `HsBaVersion` library linked against `HsBaSlicerBase`.

### Installation and packaging

Targets are exported via `GNUInstallDirs` and `CMakePackageConfigHelpers`. A `HsBaSlicerTargets.cmake` export set plus `HsBaSlicerConfig.cmake`/`HsBaSlicerConfigVersion.cmake` (SameMajorVersion compatibility) are installed under `${CMAKE_INSTALL_LIBDIR}/cmake/HsBaSlicer`. Public headers (C API, LibHsBaSlicer API, base, logger, pipelinetypes, convert, proto-generated files) are installed under `${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/<subdir>`. The C++20 module wrapper installs its `.cppm` via `FILE_SET hsba_slicer_modules`.

### Testing

- Runtime tests use Boost.Test via the `tests/` tree, enabled by `HSBA_SLICER_USE_TESTS` (off on Android/iOS/game consoles).
- `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` runs `ctest` after building, but is forced OFF during cross-compilation.
- Compile-time-only template checks live in `static_tests/` and are gated by `HSBA_SLICER_USE_STATIC_TESTS`.

### Code coverage

Coverage is opt-in via `HSBA_COVERAGE` (or the `linux-coverage` preset). When enabled:
- Every target is compiled/linked with `--coverage` (GCC/Clang only).
- A custom `coverage` target runs gcovr twice: core modules (`base`, `2D`, `utils`, `cipher`, `meshmodel`, `paths`, `preprocess`, `support`, `LibHsBaSlicer`) must reach ≥ 45% line coverage; all other sources must reach ≥ 25%.
- Thresholds are cache variables `HSBA_COVERAGE_THRESHOLD_CORE` / `HSBA_COVERAGE_THRESHOLD_OTHER`; exclusions cover `tests`, `samples`, `static_tests`, `static_check`, `third_parts_without_vcpkg`, `docs`, `out`, `build`, `vcpkg_installed`, `vcpkg/`, `/usr/`.
- Parallelism of gcov parsing is controlled by `HSBA_COVERAGE_JOBS` (auto-detected from CPU count when > 1, capped by CI env vars `CMAKE_BUILD_PARALLEL_LEVEL=2` and `VCPKG_MAX_CONCURRENCY=2`).

### CI pipelines

GitHub Actions workflows under `.github/workflows/` provide one per platform:
- `build-linux.yml`, `build-macos.yml`, `build-windows.yml`, `build-android.yml`, `build-ios.yml` — each bootstraps vcpkg, configures via Ninja (Xcode on iOS), builds Release, and runs tests.
- `test-coverage.yml` — PR-gated on `master`/`dev`; uses the `linux-coverage` preset, runs ctest, then invokes `cmake --build --preset linux-coverage-report` to enforce the coverage gates and uploads HTML/XML reports as artifacts.
- `cmake-multi-platform.yml` and `publish-wiki.yml` complement the matrix.

### Key conventions observed

- Every source subtree ships its own `CMakeLists.txt` declaring its library/executable and linking against the appropriate parent libraries.
- Optional features are exposed as CMake `option()` cache variables with sensible defaults that depend on the detected platform.
- Copyleft dependencies are isolated behind `HSBA_COPL` / the `copyleft` vcpkg feature rather than being hard-required.
- Cross-compilation disables test execution automatically.
- The build requires PowerShell on all platforms because version generation calls `Generator_Version.ps1`.