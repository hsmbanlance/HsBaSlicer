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
    - vcpkg-configuration.json
    - .github/workflows/cmake-multi-platform.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/build-linux.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - cmake/coverage.cmake
    - version/Generator_Version.ps1
    - static_check/feature_check.cmake
    - android/app/build.gradle
---

## Build Approach

HsBaSlicer is a C++20 project built entirely with **CMake ≥ 3.28** and **Ninja**, with dependencies resolved through **vcpkg** (manifest-driven, pinned via `vcpkg-configuration.json` to a specific git baseline). The top-level `CMakeLists.txt` orchestrates ~15 sub-libraries (`base`, `cipher`, `utils`, `2D`, `paths`, `fileoperator`, `meshmodel`, `support`, `preprocess`, `pipelinetypes`, `convert`, `LibHsBaSlicer`, `ModuleHsBaSlicer`, `DllHsBaSlicer`, `HsBaSlicer`) plus samples, tests, docs, and version generation.

## Key Files

- `CMakeLists.txt` — root build configuration, feature detection, dependency discovery, install/export rules.
- `CMakePresets.json` — per-platform configure/build/test presets (Windows x64 Debug/Release, Linux Debug/Release/Coverage, macOS Debug/Release, iOS Release, Android arm64).
- `vcpkg.json` / `vcpkg-configuration.json` — dependency manifest (Boost, Clipper2, Eigen3, libigl, OpenCV, protobuf, RapidJSON, OpenSSL, tinyxml2, yaml-cpp, lua, magic_enum, bit7z, miniz, sqlpp11, openvdb, CGAL/OpenCascade behind `copyleft` feature) and registry/triplet overlay configuration.
- `.github/workflows/*.yml` — GitHub Actions: `build-linux.yml`, `build-windows.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml`, `cmake-multi-platform.yml` (matrix over Windows/Linux/macOS), `test-coverage.yml`, `publish-wiki.yml`.
- `cmake/coverage.cmake` — gcov/gcovr instrumentation, core-vs-other coverage thresholds (45% / 25%), custom `coverage` target.
- `version/Generator_Version.ps1` — PowerShell script generating `version.cpp` at configure time from `vcpkg.json`, embedding build type, platform, triplet, timestamp, and third-party license inventory.
- `static_check/feature_check.cmake` — compile-time feature probes for C++ concepts, ranges, source_location, NTTP class/FP, template-template matching, C `_Generic`, coroutines, explicit-this, C23 constexpr.
- `android/app/build.gradle` / `android/build.gradle` — Gradle wrapper driving the Android example APK; native `.so`s are produced by CMake and copied into `jniLibs/arm64-v8a` by CI.

## Architecture & Conventions

### Platform tri-state
The root CMake detects `HSBA_DESKTOP` (WIN32/LINUX/APPLE-NOT-IOS), `HSBA_MOBILE` (ANDROID/IOS), or `HSBA_GAME_CONSOLE` (triplets containing `xbox`/`switch`/`playstation`/`stadia`). This flag cascades through every subdirectory to gate features:
- Logger disabled on game consoles.
- Dynamic library loader (`HSBA_USE_DLL_LOADER`) disabled on non-desktop.
- bit7z disabled on non-desktop.
- OpenCV, CGAL, OpenCASCADE, OpenVDB, MySQL/PostgreSQL backends disabled on ANDROID/IOS/GAME_CONSOLE.
- Tests disabled on ANDROID/IOS/GAME_CONSOLE.

### Copyleft kernel toggle
`HSBA_COPL` is a three-state cache variable (`AUTO`/`ON`/`OFF`). In `AUTO`, it probes `VCPKG_MANIFEST_FEATURES` env var, then falls back to `find_package(CGAL|OpenCASCADE)` to detect whether the copyleft feature was installed. When enabled, the project license switches from MIT to GPL-3.0-or-later (documented in both `vcpkg.json` and the generator script).

### Shared vs static libraries
`BUILD_SHARED_LIBS` defaults ON but is overridden by `VCPKG_TARGET_TRIPLET`: Windows/Xbox → shared; Linux/Android/static triplets → static. On iOS all libraries are statically linked (verified by CI).

### Output layout
All artifacts land under `out/build/<presetName>/bin/<CONFIG>` and `out/install/<presetName>`. `logcfg.ini` is copied into the binary output directory at configure time.

### Install & export
The project installs targets under `${CMAKE_INSTALL_LIBDIR}/cmake/HsBaSlicer` as an export set (`HsBaSlicerTargets`) with namespace `HsBaSlicer::`, generates `HsBaSlicerConfig.cmake` / `HsBaSlicerConfigVersion.cmake` (SameMajorVersion compatibility), and installs public headers under `include/HsBaSlicer/...` mirroring the source tree.

### Versioning
A PowerShell script runs during CMake configure to generate `version/version.cpp`, which exposes `HsBa::Slicer::Version::GetVersionInfo()` returning library name, license (MIT or GPL-3.0-or-later based on whether any GPL/LGPL dep is present), version string, build type, build platform, configure timestamp, vcpkg triplet, and a list of third-party libraries with their licenses and homepages parsed from `vcpkg.json`.

### Protobuf codegen
`HSBA_PROTOBUF_OUT` toggles generation of multi-language protobuf bindings (cpp/cs/java/python/php) alongside the C++ `.pb.h` files; generated outputs are installed under `share/HsBaSlicer/proto/...`.

### Documentation
Doxygen is optional (`HSBA_SLICER_BUILD_DOCS`); if not found, documentation building is disabled automatically. The `docs/` subtree contains Doxyfile and English/Chinese Markdown guides.

## CI Pipeline

- `cmake-multi-platform.yml` — matrix build over `windows-latest`, `ubuntu-latest`, `macos-latest` (Release only); also builds Android (arm64-v8a, NDK r27d, API 28) and produces an APK artifact, and builds iOS (arm64, deployment target 26.5) with symbol verification.
- `build-linux.yml` / `build-windows.yml` / `build-macos.yml` — single-platform build-and-test workflows.
- `test-coverage.yml` — runs the `linux-coverage` preset (gcov-instrumented Debug build), executes Boost.Test suites, then invokes the `coverage` target to enforce line-coverage gates: core modules (base, 2D, utils, cipher, meshmodel, paths, preprocess, support, LibHsBaSlicer) ≥ 45%, everything else ≥ 25%. Results are written to the job summary and uploaded as artifacts. Parallelism is capped at 2 (`CMAKE_BUILD_PARALLEL_LEVEL`, `VCPKG_MAX_CONCURRENCY`) because CAD-heavy modules can OOM on hosted runners.
- All workflows bootstrap vcpkg, enable the GitHub Actions binary cache backend (`VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`), and cache `vcpkg/downloads` keyed off `vcpkg.json`.

## Conventions & Enforced Rules

1. **C++20 minimum**: `CMAKE_CXX_STANDARD 20` is enforced at the root; C standard is 11 by default, upgraded to 23 when the compiler supports it (GCC ≥ 13, Clang ≥ 16, MSVC ≥ 19.36).
2. **Feature-gated compilation**: Missing required C++ features (concepts, ranges, source_location, template-template parameter matching) cause `message(FATAL_ERROR ...)` and abort configure.
3. **Position-independent code**: `CMAKE_POSITION_INDEPENDENT_CODE ON` is set globally so static libs linkable into shared libraries work everywhere.
4. **Debug postfix**: `CMAKE_DEBUG_POSTFIX "d"` ensures debug binaries/libraries are distinguishable from release ones.
5. **Cross-compilation disables test execution**: `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` is forced OFF when `CMAKE_CROSSCOMPILING` is true.
6. **Copyleft licensing is opt-in**: The `copyleft` vcpkg feature (and the `HSBA_COPL` CMake option) must be explicitly enabled; otherwise the project stays MIT-licensed and CGAL/OpenCASCADE are excluded.
7. **Coverage thresholds are hard gates**: `cmake/coverage.cmake` uses `--fail-under-line` with 45% for core and 25% for other modules; the `coverage` custom target fails the build when either threshold is missed.
8. **Android/iOS toolchains**: Cross-compilation goes through vcpkg's chainload mechanism (`VCPKG_CHAINLOAD_TOOLCHAIN_FILE` pointing at `$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`) with `VCPKG_TARGET_TRIPLET` set to `arm64-android` or `arm64-ios`.
9. **Output directories are unified**: `CMAKE_RUNTIME_OUTPUT_DIRECTORY` and `CMAKE_LIBRARY_OUTPUT_DIRECTORY` are both set to `${CMAKE_BINARY_DIR}/bin/$<CONFIG>` so executables and shared libraries share one location.
10. **Install namespace**: All exported CMake targets use the `HsBaSlicer::` namespace via `install(EXPORT ... NAMESPACE HsBaSlicer::)`.