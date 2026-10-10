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
    - cmake/coverage.cmake
    - static_check/feature_check.cmake
    - version/Generator_Version.ps1
    - cmake/HsBaSlicerConfig.cmake.in
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
---

## Build Approach

HsBaSlicer is a C++20 project built entirely with **CMake ≥ 3.28** (top-level `CMakeLists.txt`), using **vcpkg** as the dependency manager and **Ninja** as the default generator. Cross-platform configuration is driven by `CMakePresets.json`, which ships per-host presets for Windows, Linux, macOS, Android (arm64-v8a, android-28) and iOS (Xcode generator, arm64, deployment target 26.5). CI lives under `.github/workflows/` (`build-linux.yml`, `build-macos.yml`, `build-windows.yml`, `build-android.yml`, `build-ios.yml`, `cmake-multi-platform.yml`, `test-coverage.yml`).

## Key Files

- `CMakeLists.txt` — top-level orchestrator: feature detection, dependency discovery, subdirectory wiring, install/export, test/doc toggles.
- `CMakePresets.json` — preconfigured Ninja/Xcode builds per host OS and mobile targets; defines `linux-coverage` preset with coverage thresholds.
- `vcpkg.json` — declares all third-party dependencies (Boost, Eigen, libigl, OpenCV, protobuf, RapidJSON, tinyxml2, yaml-cpp, lua, openssl, sqlpp11, bit7z, miniz, clipper2, magic-enum, fontconfig, freetype, libjpeg-turbo, openvdb) plus the optional `copyleft` feature (CGAL, libigl[cgal], OpenCascade).
- `cmake/coverage.cmake` — adds `--coverage` to every target, defines `coverage` custom target that runs gcovr with two line-coverage gates: core modules ≥ 45 %, other modules ≥ 25 %.
- `static_check/feature_check.cmake` — compile-time probes for C++ concepts, ranges, source_location, NTTP class/fp, template-template matching, coroutines, explicit-this, C23 constexpr, C++20 modules; missing required features cause `FATAL_ERROR`.
- `version/Generator_Version.ps1` — PowerShell script invoked at configure time to generate `version.cpp` from `vcpkg.json`, embedding build type, platform, triplet, timestamp, and third-party library license/homepage metadata into the binary's runtime version info.
- `cmake/HsBaSlicerConfig.cmake.in` — installed package config consumed by downstream projects via `find_package(HsBaSlicer)`.
- `cmake/deploy_dlls.cmake` — helper for bundling DLLs alongside outputs.
- `.github/workflows/build-linux.yml` — canonical CI example: installs system deps, bootstraps vcpkg, caches `vcpkg/downloads` keyed on `vcpkg.json`, configures with `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake`, builds, then runs `ctest --parallel 2`.

## Architecture & Conventions

### Dependency resolution
All external libraries are resolved through vcpkg via `find_package(... CONFIG REQUIRED)` or `pkg_check_modules`. Platform scoping is expressed in `vcpkg.json` using `platform:` constraints (e.g. `windows | linux | osx | android` vs `!windows & !linux & !osx & !android`) so the same manifest drives both native and cross-compilation. The `copyleft` feature is enabled by default (`default-features: [copyleft]`) but can be disabled via `VCPKG_MANIFEST_NO_DEFAULT_FEATURES`; when off, CGAL/OpenCascade are not linked and the project stays MIT rather than GPL-3.0-or-later.

### Feature gating
The root CMake performs mandatory compiler-feature checks first (`check_cxx_feature_concepts`, `ranges`, `source_location`, `nttp_class`, `template_template_matching`); any failure aborts configuration. Optional features (`coroutines`, `explicit_this`, `c23_constexpr`, C++20 modules) set `HSBA_HAS_*` compile definitions. Platform-specific toggles (`HSBA_DESKTOP`, `HSBA_MOBILE`, `HSBA_GAME_CONSOLE`, `HSBA_COPL`, `HSBA_USE_DLL_LOADER`, `HSBA_HSBA_USE_BIT7Z`, `HSBA_POLYGON_DUMP`) control which subdirectories and dependencies are included.

### Output layout
Executables, shared libraries, and resources are unified under `${CMAKE_BINARY_DIR}/bin/$<CONFIG>` via `CMAKE_RUNTIME_OUTPUT_DIRECTORY` / `CMAKE_LIBRARY_OUTPUT_DIRECTORY`. Debug artifacts get a `d` postfix (`CMAKE_DEBUG_POSTFIX "d"`). `logcfg.ini` is copied into the output directory at configure time.

### Install & export
Targets are exported under the `HsBaSlicer::` namespace via `install(EXPORT HsBaSlicerTargets ...)`, accompanied by generated `HsBaSlicerConfig.cmake` and `HsBaSlicerConfigVersion.cmake` (SameMajorVersion compatibility). Public headers are installed under `${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/<subdir>` mirroring the source tree layout.

### Tests
Boost.Test suites live under `tests/`, registered through a single `tests/CMakeLists.txt` and driven by `enable_testing()` at the top level. `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` defaults to ON but is forced OFF during cross-compilation. Static-only tests live under `static_tests/` and are gated by `HSBA_SLICER_USE_STATIC_TESTS`.

### Documentation
Doxygen-driven docs are optional (`HSBA_SLICER_BUILD_DOCS`), skipped automatically if Doxygen is not found.

### Versioning
The version string `1.0.0` is declared in the root `project("HsBaSlicer" VERSION 1.0.0)` and also embedded into the generated `version.cpp` by `Generator_Version.ps1`, which additionally records build type, host/target platform, vcpkg triplet, configure timestamp, and a list of third-party libraries with their licenses and homepages. The effective project license (MIT vs GPL-3.0-or-later) is derived from whether any copyleft dependency is present.

## Conventions & Enforced Rules

- **Compiler minimum**: C++20 is required (`CMAKE_CXX_STANDARD 20`); C standard is 11 by default, upgraded to 23 when the compiler supports it (GCC ≥ 13, Clang ≥ 16, MSVC ≥ 19.36).
- **Required C++ features**: concepts, ranges, `std::source_location`, class-type NTTP, and P0522R0 template-template matching must compile — otherwise configuration fails with `FATAL_ERROR` (enforced by `static_check/feature_check.cmake` invocations in the root `CMakeLists.txt`).
- **Dependency policy**: all third-party packages are pulled through vcpkg; `find_package(... CONFIG REQUIRED)` is used uniformly. Platform availability is declared in `vcpkg.json` rather than scattered `if(ANDROID)` blocks.
- **Copyleft gate**: enabling the `copyleft` vcpkg feature (or auto-detecting CGAL/OpenCascade) switches the project license to GPL-3.0-or-later and enables CGAL/OpenCascade/OpenVDB/CAD backends; disabling it keeps the project MIT and excludes those kernels.
- **Coverage baseline**: the `coverage` target enforces ≥ 45 % line coverage for core directories (`base`, `2D`, `utils`, `cipher`, `meshmodel`, `paths`, `preprocess`, `support`, `LibHsBaSlicer`) and ≥ 25 % for everything else; failing either threshold aborts the build (enforced by `cmake/coverage.cmake` via `--fail-under-line`).
- **Cross-compilation behavior**: when `CMAKE_CROSSCOMPILING` is true, tests are still built but not executed (`HSBA_SLICER_RUN_TESTS_AFTER_BUILD` forced OFF).
- **Android/iOS/game-console restrictions**: dynamic library loader, bit7z, OpenGL-dependent OpenCV, CAD/point-cloud backends, MySQL/PostgreSQL SQL backends, and the test harness are all disabled on these platforms via `set(... FORCE)` in the root CMake.
- **Preset convention**: every host/platform has paired debug/release presets in `CMakePresets.json` sharing a common `binaryDir`/`installDir` pattern under `out/build/${presetName}` / `out/install/${presetName}`, and all use the vcpkg toolchain file.