---
kind: dependency_management
name: vcpkg Manifest-Driven Dependency Management with Feature-Gated Kernels
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - overlay-ports/README.md
    - triplets/README.md
    - docs/en/vcpkg-dependencies.md
    - CMakeLists.txt
    - 2D/CMakeLists.txt
---

## System / Approach

HsBaSlicer uses **Microsoft vcpkg** as its sole third-party dependency manager for C/C++ libraries. Dependencies are declared in a manifest (`vcpkg.json`) and resolved against the official vcpkg registry plus the Microsoft artifact catalog, with local overrides via `overlay-ports` and `triplets`. The build system (CMake) integrates with vcpkg through the standard toolchain file; there is no vendored copy of third-party sources — the `third_parts_without_vcpkg/` directory exists but is empty.

## Key Files

- `vcpkg.json` — manifest declaring all dependencies, platform constraints, and two features (`copyleft`, `rest-api`).
- `vcpkg-configuration.json` — pins the vcpkg git baseline (`04a9d8e...`), registers the Microsoft artifact catalog registry, and points to `./overlay-ports` and `./triplets`.
- `overlay-ports/README.md` — placeholder for custom ports (empty).
- `triplets/README.md` — placeholder for custom triplets (empty).
- `docs/en/vcpkg-dependencies.md` — documented dependency matrix, licensing rules, Android/iOS exclusions, and troubleshooting.
- Root `CMakeLists.txt` — reads `VCPKG_TARGET_TRIPLET`, detects feature flags from `VCPKG_MANIFEST_FEATURES`, and gates optional targets on them.
- `2D/CMakeLists.txt` — example of per-target `find_package(...)` calls that consume the packages installed by vcpkg.

## Architecture and Conventions

### Manifest structure

`vcpkg.json` declares ~30 dependencies grouped by purpose:
- Geometry/mesh: `clipper2`, `eigen3`, `libigl`, `openvdb`, `cgal` (via `copyleft` feature).
- Boost ecosystem: `boost-locale`, `boost-log`, `boost-dll`, `boost-nowide`, `boost-multiprecision`, `boost-date-time`, `boost-property-tree`, `boost-test`, `boost-units`, `boost-uuid`, `boost-pfr`, `boost-graph`.
- I/O & serialization: `rapidjson`, `tinyxml2`, `yaml-cpp`, `protobuf`, `sqlpp11` (with `sqlite3`/`mysql`/`postgresql` features).
- Imaging: `opencv`, `libjpeg-turbo`, `libpng`, `freetype`, `fontconfig`.
- Utilities: `bit7z`, `miniz`, `lua`, `openssl`, `magic-enum`, `vcpkg-pkgconfig-get-modules`.

Every dependency carries an explicit `platform` constraint (e.g. `windows | linux | osx`, or `android | ios`); some entries omit it to mean "all platforms". Platform-specific overrides use the same package name with different `features` blocks (see `sqlpp11` which lists `sqlite3,mysql,postgresql` on desktop and only `sqlite3` on mobile).

### Feature-gated copyleft kernels

The `copyleft` feature (enabled by default on `windows | linux | osx | android`) pulls in `cgal`, `libigl[cgal]`, and `opencascade`. Its `license` field is set to `GPL-3.0-or-later`, overriding the top-level `MIT` license. Disabling default features (via `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`) prevents CGAL/OpenCascade from being installed and keeps the effective build license MIT. The effective license is generated at configure time into `version.cpp` and exposed via `GetVersionInfo()` / `HsBaGetVersionJson()`.

### Optional REST API feature

The `rest-api` feature adds `cpp-httplib` (desktop-only). In `CMakeLists.txt`, `HSBA_BUILD_REST_API` is checked against `VCPKG_MANIFEST_FEATURES`; if unset, it falls back to probing `find_package(httplib CONFIG QUIET)`. If neither source confirms the dependency, configuration fails with a `FATAL_ERROR` instructing the user to enable the `rest-api` manifest feature.

### Platform exclusion policy

`docs/en/vcpkg-dependencies.md` documents why certain packages are excluded on Android and iOS (compatibility issues, unsuitable complexity, missing pkg-config support). The exclusion list mirrors the `platform` attributes in `vcpkg.json`: `boost-{locale,log,dll,nowide}`, `fontconfig`, `bit7z`, `opencv`, `opencascade`, `sqlpp11` MySQL/PostgreSQL features, `openvdb`, and `vcpkg-pkgconfig-get-modules` are not built on mobile.

### Game console detection

When `VCPKG_TARGET_TRIPLET` matches `xbox`, `switch`, `playstation`, or `stadia`, the root CMake config defines `HSBA_IS_GAME_CONSOLE` and `HSBA_GAME_CONSOLE_<TRIPLET>` compile definitions and forces `HSBA_GAME_CONSOLE=ON`.

### CMake integration pattern

Submodules call `find_package(<pkg> REQUIRED|QUIET)` directly against the names exported by vcpkg ports (e.g. `PNG`, `JPEG`, `Lua`, `Freetype`, `libpng`, `libjpeg-turbo`, `httplib`). There is no custom `Find*.cmake` wrapper — vcpkg's CMake integration is used as-is.

## Conventions and Constraints

- All third-party C/C++ dependencies are declared exclusively in `vcpkg.json`; no vendored sources exist under `third_parts_without_vcpkg/`.
- Every dependency entry specifies an explicit `platform` attribute rather than relying on implicit defaults — this is the observed convention across the manifest.
- Copyleft libraries (`cgal`, `opencascade`) are never direct dependencies; they are gated behind the `copyleft` feature, whose presence determines whether the project's effective license is MIT or GPL-3.0-or-later (enforced by the `license` fields in both the top-level object and the feature block).
- Mobile builds must disable default features (`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1`) to avoid installing copyleft kernels; this is documented in `docs/en/vcpkg-dependencies.md` and enforced by the feature gating in `vcpkg.json`.
- The `rest-api` target requires the `rest-api` vcpkg feature; enabling `HSBA_BUILD_REST_API=ON` without installing `cpp-httplib` causes a hard configure failure (root `CMakeLists.txt` line 198–200).
- The vcpkg installation baseline is pinned to a specific git commit in `vcpkg-configuration.json`, ensuring reproducible dependency resolution.
- Custom ports and triplets are reserved via `overlay-ports/` and `triplets/` directories registered in `vcpkg-configuration.json`, but both are currently empty placeholders.