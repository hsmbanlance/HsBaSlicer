---
kind: dependency_management
name: vcpkg Manifest + CMake Integration with Platform-Gated Features
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - CMakeLists.txt
    - docs/en/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - third_parts_without_vcpkg/
    - webui/package.json
    - android/build.gradle
---

## Approach

HsBaSlicer uses **Microsoft vcpkg** as the sole dependency manager for its C++ codebase. Dependencies are declared in a single manifest file (`vcpkg.json`) and resolved at configure time via `find_package(... CONFIG REQUIRED)` calls scattered through the top-level `CMakeLists.txt` and per-module `CMakeLists.txt` files. The project also ships two small Node.js/Gradle subprojects (the embedded web console under `webui/` and an Android example under `android/`) that use their own package managers (`package.json`, Gradle), but these are not part of the main build.

## Key Files

- `vcpkg.json` — declares all C/C++ dependencies, platform gates, and optional features (`copyleft`, `rest-api`).
- `vcpkg-configuration.json` — pins the vcpkg toolchain to a specific git baseline (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4` from `github.com/microsoft/vcpkg`), registers the Microsoft artifact registry, and points to local `overlay-ports/` and `triplets/` directories.
- `docs/en/vcpkg-dependencies.md` — authoritative documentation of the licensing model, platform exclusion matrix, and the `HSBA_COPL` / `VCPKG_MANIFEST_FEATURES` mechanism.
- Top-level `CMakeLists.txt` — performs `find_package` lookups for every vcpkg-managed library (Boost, Eigen3, OpenCV, libigl, CGAL, OpenCASCADE, protobuf, rapidjson, yaml-cpp, openssl, bit7z, miniz, freetype, etc.) and wires them into targets via `target_link_libraries`.
- `overlay-ports/README.md` — documents where custom vcpkg ports should be placed.
- `triplets/README.md` — documents where custom triplets go.
- `third_parts_without_vcpkg/` — empty directory reserved for third-party sources that cannot be pulled via vcpkg.

## Architecture and Conventions

### Feature-gated dependencies

Dependencies are split into three layers:

1. **Always-installed dependencies** listed directly under `dependencies` (e.g. `boost-multiprecision`, `clipper2`, `libjpeg-turbo`, `libpng`, `eigen3`, `protobuf`, `rapidjson`, `yaml-cpp`, `lua`, `openssl`, `magic-enum`).
2. **Platform-scoped dependencies** using the `{ name, platform }` form (e.g. `bit7z`, `boost-locale`, `opencv`, `openvdb`, `sqlpp11`) to exclude packages on Android/iOS where they are incompatible or unnecessary.
3. **Optional feature dependencies** under `features:` — `copyleft` pulls in `cgal`, `libigl[cgal]`, and `opencascade`; `rest-api` pulls in `cpp-httplib[openssl]`. The `copyleft` feature is enabled by default on `windows | linux | osx | android` via `default-features`, while `rest-api` is opt-in.

The effective license of a build is derived from whether the `copyleft` feature is active: MIT when disabled, GPL-3.0-or-later when any copyleft kernel is linked. This is surfaced at runtime via version info generated at configure time.

### CMake integration pattern

The root `CMakeLists.txt` centralizes all `find_package` calls (lines ~260–362) and mirrors the vcpkg feature model:

- `HSBA_BUILD_REST_API` is forced OFF on non-desktop platforms and additionally checks that the `rest-api` vcpkg feature is active (via `VCPKG_MANIFEST_FEATURES` env var or cache variable).
- `HSBA_COPL` is tri-state (`AUTO` by default): it resolves from `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` environment variables, then probes whether CGAL/OpenCascade are installed; `-DHSBA_COPL=ON/OFF` forces it explicitly. When ON, `USE_CGAL` and `USE_OCCT` macros are defined and CGAL/OpenCASCADE are found and linked.
- Game-console triplets (`xbox`, `switch`, `playstation`, `stadia`) are detected via `VCPKG_TARGET_TRIPLET` and define `HSBA_IS_GAME_CONSOLE` plus `HSBA_GAME_CONSOLE_<TRIPLET>`.

### Platform exclusions (documented)

Per `docs/en/vcpkg-dependencies.md`, the following packages are excluded on mobile:

- Android: `boost-locale`, `boost-log`, `boost-dll`, `boost-nowide`, `fontconfig`, `bit7z`, `opencascade`, `sqlpp11` MySQL/PostgreSQL features, `vcpkg-pkgconfig-get-modules`.
- iOS: `bit7z`, `opencv`, `opencascade`, `sqlpp11` MySQL/PostgreSQL features.

A full dependency matrix (Windows/Linux/macOS/Android/iOS) is maintained in that document.

### Registries and overlay ports

`vcpkg-configuration.json` pins the vcpkg toolchain to a fixed commit and registers the Microsoft artifact catalog registry. Custom ports live under `overlay-ports/` and custom triplets under `triplets/`, both referenced via `overlay-ports` and `overlay-triplets` entries. The directories currently only contain README placeholders.

### Non-vcpkg subprojects

- `webui/package.json` — Node.js module (`hsba-rest-console`, private, requires Node ≥20). Uses the built-in `node --test` runner; no lockfile is committed.
- `android/build.gradle` — Declares the Android Gradle Plugin version `8.3.2` (applied false); the actual app module lives under `android/app/`.

## Conventions and Constraints

- All C/C++ third-party libraries are declared exclusively in `vcpkg.json`; there is no vendored source tree for dependencies (the `third_parts_without_vcpkg/` directory is empty).
- Dependency versions are pinned indirectly by pinning the vcpkg baseline commit in `vcpkg-configuration.json` rather than by explicit version strings in `vcpkg.json`.
- Platform-specific dependency selection is done via the `platform` field in `vcpkg.json` (e.g. `"platform": "windows | linux | osx"`), not via CMake conditionals alone.
- Optional capabilities are modeled as vcpkg features (`copyleft`, `rest-api`) and mirrored as CMake options (`HSBA_COPL`, `HSBA_BUILD_REST_API`) that gate both installation and linking.
- The `rest-api` feature additionally requires enabling the `openssl` feature of `cpp-httplib` for HTTPS support, as documented in `vcpkg.json`.
- To produce an MIT-licensed build (no copyleft kernels), the documented procedure is to set `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` before configuring so vcpkg does not install the `copyleft` feature's dependencies.
- The repository does not commit a `vcpkg.lock` or `vcpkg-binary-cache` configuration; reproducibility relies on the pinned vcpkg baseline and the artifact registry.