---
kind: dependency_management
name: vcpkg Manifest-Driven Dependency Management with Platform-Specific Features
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - docs/en/vcpkg-dependencies.md
    - CMakeLists.txt
    - CMakePresets.json
    - .github/workflows/build-windows.yml
    - .github/workflows/build-linux.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - overlay-ports/README.md
    - triplets/README.md
---

# Dependency Management

## System / Approach

The repository uses **Microsoft vcpkg** as the sole package manager for C/C++ third-party dependencies. Dependencies are declared in a single manifest file (`vcpkg.json`) and resolved through the vcpkg CMake toolchain integration (`CMAKE_TOOLCHAIN_FILE`). There is no vendoring of source code — `third_parts_without_vcpkg/` exists but is empty, and `vcpkg_installed/` is generated at build time.

## Key Files

- `vcpkg.json` — dependency manifest: declares all packages, per-platform constraints, and the optional `copyleft` feature.
- `vcpkg-configuration.json` — pins the vcpkg registry baseline to commit `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`, registers the Microsoft artifact catalog registry, and enables overlay ports/triplets from `./overlay-ports` and `./triplets`.
- `docs/en/vcpkg-dependencies.md` — authoritative documentation of platform compatibility, licensing implications, and the `HSBA_COPL` tri-state option.
- `CMakeLists.txt` (root) — implements the `HSBA_COPL` option resolution logic that reads `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` environment variables and probes for CGAL/OpenCascade installation.
- `CMakePresets.json` — every preset sets `toolchainFile` to `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`.
- `.github/workflows/*.yml` — CI workflows pass `-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` on Windows/Linux/macOS/Android/iOS.
- `overlay-ports/README.md` / `triplets/README.md` — placeholder directories for custom vcpkg ports and triplets.

## Architecture and Conventions

### Package declaration style

Dependencies in `vcpkg.json` follow two patterns:

1. **Simple string entries** for header-only or trivial packages: `boost-multiprecision`, `boost-test`, `boost-units`, `boost-uuid`, `boost-pfr`, `boost-graph`, `clipper2`, `libjpeg-turbo`, `libpng`, `eigen3`, `freetype`, `miniz`, `protobuf`, `rapidjson`, `tinyxml2`, `yaml-cpp`, `lua`, `openssl`, `magic-enum`.
2. **Object entries** when per-platform constraints or features are needed, e.g.:
   - `{ name: "bit7z", platform: "windows | linux | osx" }` — excluded on Android/iOS.
   - `{ name: "opencv", platform: "windows | linux | osx | android" }` — excluded on iOS.
   - `{ name: "sqlpp11", features: ["sqlite3", "mysql", "postgresql"], platform: "windows | linux | osx" }` vs. a second entry with only `sqlite3` for `android | ios`.
   - `{ name: "libigl", platform: "!windows & !linux & !osx & !android" }` — included only on iOS.

### Feature-gated copyleft kernel

A top-level `default-features` array enables the `copyleft` feature on `windows | linux | osx | android`. The `copyleft` feature pulls in `cgal`, `libigl[cgal]`, and `opencascade`, which carry GPL-3.0-or-later / LGPL-2.1-only licenses. When enabled, the project's effective license switches from MIT to GPL-3.0-or-later; when disabled (iOS builds, or by setting `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`), those kernels are neither installed nor linked.

The root `CMakeLists.txt` exposes a tri-state `HSBA_COPL` option:
- `AUTO` (default): resolves from `VCPKG_MANIFEST_FEATURES` env var → falls back to `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` env var → finally probes whether CGAL/OpenCascade are installed (because default-features are not listed in `VCPKG_MANIFEST_FEATURES`).
- `ON`/`OFF`: forces the behavior explicitly via `-DHSBA_COPL=ON/OFF`.

When `HSBA_COPL` is true, it defines `USE_CGAL` / `USE_OCCT` macros and links the copyleft libraries; otherwise they are skipped.

### Registry pinning

`vcpkg-configuration.json` pins the vcpkg registry to a specific git commit (`baseline: "04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4"`) and registers the Microsoft artifact catalog (`microsoft` registry pointing at `https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`). This makes dependency resolution reproducible across machines and CI.

### Overlay mechanism

Both `overlay-ports` and `overlay-triplets` are registered in `vcpkg-configuration.json` but currently contain only README placeholders. They exist for adding custom vcpkg ports and triplets without modifying the upstream vcpkg tree.

### Build-system integration

Every CMake preset in `CMakePresets.json` references `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` as its `toolchainFile`. All GitHub Actions workflows (`build-windows.yml`, `build-linux.yml`, `build-macos.yml`, `build-android.yml`, `build-ios.yml`, `cmake-multi-platform.yml`) pass the same `-DCMAKE_TOOLCHAIN_FILE` flag. The Android README documents the additional flags `DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake`, `DVCPKG_TARGET_TRIPLET=arm64-android`, `DANDROID_ABI=arm64-v8a`, `DANDROID_PLATFORM=android-21`.

## Conventions and Constraints

- **Single manifest**: All C/C++ dependencies are declared exclusively in `vcpkg.json`; there are no `find_package()` calls for third-party libraries outside of the copyleft-kernel probe inside the root `CMakeLists.txt`.
- **Platform constraints in manifest**: Per-package `platform` expressions in `vcpkg.json` are the canonical source of truth for cross-platform availability; the `docs/en/vcpkg-dependencies.md` dependency matrix mirrors this information for human readers.
- **Android exclusions**: `boost-locale`, `boost-log`, `boost-dll`, `boost-nowide`, `fontconfig`, `bit7z`, `opencascade`, `sqlpp11` MySQL/PostgreSQL features, and `vcpkg-pkgconfig-get-modules` are excluded on Android because of known compatibility issues or unsuitability for mobile platforms.
- **iOS exclusions**: `bit7z`, `opencv`, `opencascade`, and `sqlpp11` MySQL/PostgreSQL features are excluded on iOS.
- **Reproducible registry**: The vcpkg registry baseline is pinned to a fixed git commit in `vcpkg-configuration.json`; updating dependencies requires changing this baseline.
- **Copyleft opt-out**: To produce an MIT-licensed build, set `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` before configuring so vcpkg does not install the `copyleft` feature's dependencies; the root `CMakeLists.txt` then detects this and sets `HSBA_COPL=OFF` automatically.
- **Overlay ports/triplets reserved**: `overlay-ports/` and `triplets/` are registered in `vcpkg-configuration.json` and documented as the locations for custom ports and triplets, but are currently empty placeholders.