---
kind: dependency_management
name: vcpkg Manifest-Driven Dependency Management with Platform-Gated Features
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
---

## Approach

HsBaSlicer uses **Microsoft vcpkg** as its sole third-party dependency manager. Dependencies are declared centrally in `vcpkg.json`, resolved against the vcpkg registry baseline, and consumed by CMake via `find_package(... CONFIG REQUIRED)`. There is no vendoring of source code; the `third_parts_without_vcpkg/` directory exists but is empty.

## Key Files

- `vcpkg.json` — single manifest declaring all dependencies, platform gates, and two optional features (`copyleft`, `rest-api`).
- `vcpkg-configuration.json` — pins the vcpkg toolchain to a git SHA (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`) and registers the Microsoft artifact catalog registry plus local `overlay-ports/` and `triplets/` directories.
- Root `CMakeLists.txt` — translates vcpkg feature flags into CMake options (`HSBA_COPL`, `HSBA_BUILD_REST_API`) and calls `find_package` for every dependency.
- `docs/en/vcpkg-dependencies.md` (and mirrored `docs/zh/`) — documents the licensing model, platform exclusion matrix, and build recommendations.
- `overlay-ports/README.md`, `triplets/README.md` — placeholders for custom ports/triplets.

## Architecture & Conventions

### Feature-gated dependencies

Two top-level features extend the base dependency set:

- `copyleft` — adds CGAL, libigl[cgal], OpenCascade. It is listed in `default-features` for `windows | linux | osx | android`. Enabling it switches the project license from MIT to GPL-3.0-or-later (declared via the feature's own `license` field). The root CMake detects this feature through `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` env vars or by probing for installed CGAL/OpenCASCADE, then sets the `HSBA_COPL` cache option (tri-state: AUTO/ON/OFF).
- `rest-api` — adds cpp-httplib. It is NOT a default feature; enabling it requires setting `VCPKG_MANIFEST_FEATURES=rest-api` (or appending to an existing list). The CMake option `HSBA_BUILD_REST_API` alone is insufficient — CMake explicitly checks whether the feature was active and emits `FATAL_ERROR` if not.

### Platform gating

Platform restrictions are expressed in two complementary layers:

1. **Manifest layer** (`vcpkg.json`): each dependency carries a `platform` field using vcpkg's expression syntax (e.g. `"windows | linux | osx"`, `"!windows & !linux & !osx & !android"`, `"android | ios"`). This controls which packages vcpkg installs per target platform.
2. **CMake layer**: conditional `find_package` calls and compile definitions mirror the same exclusions (e.g. OpenCV skipped on iOS/game-console, bit7z disabled on non-desktop, sqlpp11 MySQL/PostgreSQL features only on desktop, Boost.Log components absent on Android/iOS/game-console).

The documented platform matrix in `docs/en/vcpkg-dependencies.md` enumerates the effective compatibility per component across Windows/Linux/macOS/Android/iOS.

### Registry & reproducibility

- Default registry: the official vcpkg git repo at a pinned commit SHA.
- Additional registry: `microsoft` artifact catalog (`https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`).
- Local extensions: `overlay-ports/` (for custom vcpkg ports) and `triplets/` (for custom triplets such as Nintendo Switch2/RISC-V), both registered in `vcpkg-configuration.json`.

### Consumption pattern

Every dependency is consumed via `find_package(<Name> CONFIG REQUIRED)` (or `pkg_check_modules` for pkg-config-based deps like Clipper2). No `target_link_libraries` names are hard-coded; consumers rely on the targets produced by vcpkg's CMake config files. Conditional compilation macros (`USE_CGAL`, `USE_OCCT`, `HAS_OPENCV`, `HSBA_USE_BIT7Z`, `HSBA_COPL`, `HSBA_NO_DLL_LOADER`) gate code paths that depend on optional libraries.

## Conventions & Constraints

- All third-party library declarations live in `vcpkg.json`; adding a new dependency means editing this file and adding a corresponding `find_package(... CONFIG REQUIRED)` call in the root `CMakeLists.txt` (enforced by the CMake build failing when a package is missing).
- Optional capabilities are modeled as vcpkg features (`copyleft`, `rest-api`); CMake enforces that the matching feature is active before enabling the associated build target (the rest-api path emits `FATAL_ERROR` if cpp-httplib is not found).
- Platform-specific dependencies use the `platform` expression in `vcpkg.json` rather than ad-hoc shell conditions; the same platform logic is duplicated in CMake `find_package` guards so builds fail fast when a dependency is unavailable on a given platform.
- The `copyleft` feature is enabled by default on desktop/mobile platforms; disabling it (via `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`) removes CGAL/OpenCascade from the install and produces an MIT-licensed build — this behavior is documented and enforced by the tri-state `HSBA_COPL` detection logic.
- Custom ports and triplets belong under `overlay-ports/` and `triplets/` respectively, as indicated by their READMEs.