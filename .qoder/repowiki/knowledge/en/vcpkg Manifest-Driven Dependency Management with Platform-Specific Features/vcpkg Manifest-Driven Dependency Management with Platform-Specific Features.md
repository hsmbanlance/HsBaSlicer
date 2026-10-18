---
kind: dependency_management
name: vcpkg Manifest-Driven Dependency Management with Platform-Specific Features
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - CMakePresets.json
    - docs/en/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - android/app/build.gradle
---

## Approach

HsBaSlicer uses **Microsoft vcpkg** as the sole third-party dependency manager for its C++/CMake build. Dependencies are declared in a single manifest file (`vcpkg.json`) and resolved through the vcpkg CMake toolchain integrated via `CMakePresets.json`. The Android example app is built separately with Gradle but does not declare native dependencies — it consumes prebuilt `.so` files under `android/app/src/main/jniLibs/arm64-v8a/` produced by the CMake/vcpkg build.

## Key Files

- `vcpkg.json` — canonical dependency manifest (name, platform constraints, features).
- `vcpkg-configuration.json` — pins the vcpkg registry baseline commit and registers the Microsoft artifact catalog; declares `overlay-ports` and `triplets` directories.
- `CMakePresets.json` — per-host configure/build/test presets that set `toolchainFile` to `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, define `VCPKG_TARGET_TRIPLET`, `ANDROID_NDK_HOME`, and iOS deployment target.
- `docs/en/vcpkg-dependencies.md` — documented platform compatibility matrix, licensing rules, and troubleshooting guidance.
- `overlay-ports/README.md` / `triplets/README.md` — placeholders for custom ports/triplets (currently empty).
- `third_parts_without_vcpkg/` — directory reserved for non-vcpkg vendored code (empty in this tree).
- `android/app/build.gradle` — Gradle wrapper around the prebuilt native library; no Gradle-managed native deps.

## Architecture & Conventions

### Registry pinning
`vcpkg-configuration.json` pins the default registry to a specific git commit of the official vcpkg repo (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`) and adds the Microsoft artifact catalog (`microsoft` registry) as an overlay. This makes dependency resolution deterministic across machines.

### Feature-gated copyleft kernels
The `copyleft` feature (enabled by default on `windows | linux | osx | android`) pulls in CGAL, libigl[cgal], and OpenCASCADE. When disabled, those libraries are neither installed nor linked, and the effective project license stays MIT; enabling it switches the build license to GPL-3.0-or-later. The CMake option `HSBA_COPL` (default `AUTO`) resolves from `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` env vars or probes whether CGAL/OpenCASCADE are installed, then defines `USE_CGAL`/`USE_OCCT` macros.

### Per-package platform constraints
Dependencies use inline `platform` selectors in `vcpkg.json`:
- Desktop-only: `bit7z`, `boost-log`, `boost-dll`, `boost-nowide`, `fontconfig`, `openvdb`, `sqlpp11` (MySQL/PostgreSQL), `vcpkg-pkgconfig-get-modules`, `opencv` (Android only), `libigl` (only when NOT windows/linux/osx/android — i.e. game consoles).
- Android exclusions: boost-locale/log/dll/nowide, fontconfig, bit7z, opencascade, sqlpp11 MySQL/PG, vcpkg-pkgconfig-get-modules.
- iOS exclusions: bit7z, opencv, opencascade, sqlpp11 MySQL/PG.
- Cross-platform: eigen3, protobuf, rapidjson, yaml-cpp, lua, openssl, miniz, tinyxml2, clipper2, magic-enum, boost-property-tree/date-time/uuid/pfr/graph/units/test/multiprecision.

### CMake integration
Every consumer calls `find_package(<Name> CONFIG REQUIRED)` against the names vcpkg exposes (e.g. `miniz`, `Protobuf`, `RapidJSON`, `Eigen3`, `OpenSSL`, `libigl`, `OpenCV`, `CGAL`, `OpenCASCADE`, `OpenVDB`, `bit7z`). The root `CMakeLists.txt` centralizes these lookups after `find_package(PkgConfig REQUIRED)` and conditional blocks for optional components (OpenCV, CGAL, OpenCASCADE, OpenVDB). Boost packages are found both via `find_package(Boost REQUIRED log)` and individual component names.

### Android/iOS cross-compilation
`CMakePresets.json` defines `android-release` (arm64-android triplet, `ANDROID_NDK_HOME`, `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM=android-28`) and `ios-release` (Xcode generator, `CMAKE_OSX_SYSROOT=iphoneos`, `CMAKE_OSX_DEPLOYMENT_TARGET=26.5`). The Android Gradle app simply bundles the resulting `.so` under `jniLibs.srcDirs = ['src/main/jniLibs']`.

### Custom ports & triplets
`overlay-ports/` and `triplets/` are registered in `vcpkg-configuration.json` but currently contain only README placeholders. They exist for adding custom vcpkg ports or custom triplets (e.g. Nintendo Switch2, RISC-V embedded Linux) without modifying the upstream registry.

## Conventions & Constraints

- All third-party C/C++ dependencies are declared in `vcpkg.json`; there is no vendoring of source code into the repository (the `third_parts_without_vcpkg/` directory exists but is empty).
- Dependency versions are pinned indirectly via the vcpkg registry baseline commit in `vcpkg-configuration.json`; individual package version pins are not used in `vcpkg.json`.
- Platform availability is expressed declaratively in `vcpkg.json` using the `platform` field rather than ad-hoc CMake `if()` checks for each package.
- Copyleft dependencies (CGAL, OpenCASCADE) are isolated behind the `copyleft` feature so consumers can produce an MIT-licensed build by disabling default features (`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`).
- Mobile platforms receive a reduced dependency set: Android excludes logging/locale/DLL/fontconfig/7z/opencascade/MySQL-PG/sqlpp11 pkg-config modules; iOS additionally excludes OpenCV and OpenCASCADE.
- The dependency matrix and licensing rules are documented in `docs/en/vcpkg-dependencies.md` and mirrored in Chinese under `docs/zh/vcpkg-dependencies.md`.
- CMake find_package calls must match the names exposed by vcpkg's CMake config files (e.g. `miniz`, `Protobuf`, `RapidJSON`, `Eigen3`, `OpenSSL`, `libigl`, `OpenCV`, `CGAL`, `OpenCASCADE`, `OpenVDB`, `bit7z`); mismatches cause configure failures because every dependency is `REQUIRED`.