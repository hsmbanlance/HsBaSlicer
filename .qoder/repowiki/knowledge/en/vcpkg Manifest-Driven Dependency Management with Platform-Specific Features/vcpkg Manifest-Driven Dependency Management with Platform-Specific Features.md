---
kind: dependency_management
name: vcpkg Manifest-Driven Dependency Management with Platform-Specific Features
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

## System Used

HsBaSlicer uses **Microsoft vcpkg** as its sole third-party dependency manager for C/C++ libraries. Dependencies are declared in a manifest (`vcpkg.json`) and resolved against the official vcpkg registry plus the Microsoft artifact catalog, with local overlay ports and triplets supported.

There is no vendored copy of third-party sources under `third_parts_without_vcpkg/` (the directory exists but is empty). Android/iOS apps use Gradle only for their own app packaging; native dependencies flow through vcpkg into the shared libraries.

## Key Files

- `vcpkg.json` — single source of truth for all third-party packages, features, platform constraints, and default features.
- `vcpkg-configuration.json` — pins the vcpkg baseline commit (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`), registers the Microsoft artifact catalog registry, and points to local `overlay-ports/` and `triplets/` directories.
- `CMakeLists.txt` (root) — reads `VCPKG_TARGET_TRIPLET`, detects copyleft feature via `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES`, then calls `find_package(... CONFIG REQUIRED)` for every dependency.
- `docs/en/vcpkg-dependencies.md` — documented matrix of which packages are excluded on Android/iOS and how the copyleft license switch works.
- `overlay-ports/README.md` and `triplets/README.md` — placeholders for custom ports/triplets (currently empty).

## Architecture and Conventions

### Manifest-driven declarations

All dependencies live in `vcpkg.json`. The root `CMakeLists.txt` does not list individual package names; it only invokes `find_package(<Name> CONFIG REQUIRED)` after vcpkg has populated the CMake config paths. This keeps the dependency surface in one place.

### Feature-gated copyleft kernels

The `copyleft` feature (enabled by default on `windows | linux | osx | android`) pulls in CGAL, libigl[cgal], and OpenCASCADE. When disabled (iOS, game consoles, or when `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`), these packages are neither installed nor linked, and the project stays MIT-licensed. The effective license is generated at configure time into `version.cpp` and exposed via `GetVersionInfo()` / `HsBaGetVersionJson()`.

### Platform-specific dependency scoping

Packages are gated using vcpkg's `platform` expressions:

- Desktop-only: `bit7z`, `boost-locale`, `boost-log`, `boost-dll`, `boost-nowide`, `fontconfig`, `opencv`, `openvdb`, `sqlpp11` (MySQL/PostgreSQL features), `vcpkg-pkgconfig-get-modules`, `opencascade`.
- Mobile-only: `libigl` is explicitly excluded from Windows/Linux/macOS/Android (`!windows & !linux & !osx & !android`).
- Android excludes: boost-locale/log/dll/nowide, fontconfig, bit7z, opencascade, sqlpp11 MySQL/PostgreSQL, vcpkg-pkgconfig-get-modules.
- iOS excludes: bit7z, opencv, opencascade, sqlpp11 MySQL/PostgreSQL.

The `sqlpp11` package appears twice in `vcpkg.json`: once with `sqlite3+mysql+postgresql` features for desktop, once with only `sqlite3` for `android | ios`.

### Registry and version pinning

`vcpkg-configuration.json` pins the vcpkg toolchain to a specific git commit baseline rather than a rolling tag, ensuring reproducible dependency resolution across CI and developer machines. The Microsoft artifact catalog is registered as an additional registry for prebuilt artifacts.

### Overlay ports and triplets

`overlay-ports/` and `triplets/` are registered in `vcpkg-configuration.json` so that custom ports and cross-compilation triplets can be added without modifying the upstream vcpkg tree. Both directories currently contain only README placeholders.

### CMake integration pattern

The root `CMakeLists.txt` follows this pattern for each dependency:

```cmake
find_package(<Package> CONFIG REQUIRED)
```

and links via CMake targets (e.g., `Boost::log`, `Eigen3::Eigen`, `Protobuf::protobuf`). Conditional blocks gate optional dependencies like `OpenCV`, `CGAL`, `OpenCASCADE`, and `OpenVDB` behind `HSBA_COPL` or platform checks.

## Conventions and Constraints

- **Single manifest**: All third-party C/C++ dependencies are declared exclusively in `vcpkg.json`; there is no `package-lock.json`, `go.mod`, or other language-level lockfile for C++ code.
- **Baseline pinning**: The vcpkg baseline commit is pinned in `vcpkg-configuration.json`, not left to resolve to HEAD.
- **Platform gating via manifests**: Per-platform exclusion is expressed declaratively in `vcpkg.json` `platform` fields rather than ad-hoc CMake `if()` checks alone.
- **Copyleft opt-in**: Building with CGAL/OpenCascade switches the project license to GPL-3.0-or-later; building without them keeps MIT. This is enforced by the `copyleft` feature's `license` field and by the `HSBA_COPL` tri-state option in CMake.
- **No vendoring**: Third-party sources are not checked in; `third_parts_without_vcpkg/` is empty.
- **Mobile reduced set**: Android and iOS builds intentionally exclude heavy desktop-only libraries (OpenCV, OpenCASCADE, Boost logging/locale/DLL, fontconfig, 7z, pkg-config bridge) as documented in `docs/en/vcpkg-dependencies.md`.
- **Custom extension points**: Local overrides go under `overlay-ports/` (custom library ports) and `triplets/` (cross-compilation triplets), both referenced from `vcpkg-configuration.json`.