---
kind: dependency_management
name: Vcpkg Manifest + CMake Dependency Management
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - docs/en/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - cmake/deploy_dlls.cmake
    - cmake/coverage.cmake
    - webui/package.json
---

# Dependency Management in HsBaSlicer

## System Used

The repository uses **vcpkg** as the sole package manager for C/C++ dependencies, declared via a manifest (`vcpkg.json`) and pinned to a specific vcpkg commit through `vcpkg-configuration.json`. A small Node.js web console under `webui/` is managed separately with npm (no lockfile committed).

## Key Files

- `vcpkg.json` — dependency manifest: declares all third-party libraries, per-platform constraints, and optional features.
- `vcpkg-configuration.json` — pins the vcpkg registry baseline to commit `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`, registers the Microsoft artifact catalog registry, and enables overlay directories.
- `overlay-ports/README.md` — placeholder for custom vcpkg ports.
- `triplets/README.md` — placeholder for custom triplets.
- `docs/en/vcpkg-dependencies.md` — authoritative documentation of platform matrix, licensing implications, and build recommendations.
- `cmake/deploy_dlls.cmake` — notes that vcpkg's `applocal.ps1` handles DLL deployment on Windows.
- `cmake/coverage.cmake` — excludes `third_parts_without_vcpkg` and `vcpkg_installed` from coverage reporting.
- `webui/package.json` — npm manifest for the standalone browser console / same-origin proxy server.

## Architecture and Conventions

### Manifest structure

`vcpkg.json` lists ~30 dependencies grouped by purpose:

- Compression/archives: `bit7z`, `miniz`
- Boost ecosystem: `boost-locale`, `boost-log`, `boost-dll`, `boost-nowide`, `boost-date-time`, `boost-property-tree`, `boost-test`, `boost-units`, `boost-uuid`, `boost-pfr`, `boost-graph`, `boost-multiprecision`
- Geometry/mesh: `clipper2`, `libigl`, `eigen3`, `openvdb`, `cgal` (via feature)
- CAD: `opencascade` (via feature)
- Image/font: `opencv`, `libjpeg-turbo`, `libpng`, `freetype`, `fontconfig`
- Serialization/config: `protobuf`, `rapidjson`, `tinyxml2`, `yaml-cpp`, `sqlpp11`
- Scripting/crypto: `lua`, `openssl`, `magic-enum`
- Tooling: `vcpkg-pkgconfig-get-modules`

### Platform scoping

Every dependency carries an explicit `platform` constraint. The documented exclusion rules are:

| Category | Android excluded | iOS excluded |
|---|---|---|
| Boost subsystems (`boost-locale`, `boost-log`, `boost-dll`, `boost-nowide`) | Yes | Yes |
| `fontconfig` | Yes | — |
| `bit7z` | Yes | Yes |
| `opencv` | — | Yes |
| `opencascade` | Yes | Yes |
| `sqlpp11` MySQL/PostgreSQL features | Yes | Yes |
| `vcpkg-pkgconfig-get-modules` | Yes | — |
| `openvdb` | Yes | Yes |

Android additionally gets only the `sqlite3` feature of `sqlpp11`; desktop platforms get `sqlite3`, `mysql`, `postgresql`. `libigl` is always included except on UWP; its `cgal` feature is gated behind the project-level `copyleft` feature.

### Optional features

Two top-level vcpkg features gate additional dependencies:

- `copyleft` (enabled by default on `windows | linux | osx | android`): pulls in `cgal`, `libigl[cgal]`, and `opencascade`. Enabling it switches the effective project license from MIT to GPL-3.0-or-later.
- `rest-api`: pulls in `cpp-httplib[openssl]` and builds the optional `HsBaSlicerRestServer` executable. Desktop-only.

The `default-features` list includes `copyleft`, so a plain `vcpkg install` installs copyleft kernels unless the consumer sets `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` before configuring.

### Registry and pinning

`vcpkg-configuration.json` pins the vcpkg toolchain to a fixed git commit (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`) against the official `microsoft/vcpkg` repository, and adds the Microsoft artifact catalog registry at `https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`. Overlay ports and triplets are loaded from `./overlay-ports` and `./triplets` respectively.

### Licensing integration

Licensing is not just declared in `vcpkg.json` but propagated into the build: enabling the `copyleft` feature changes the project license to GPL-3.0-or-later, and the effective license is generated at configure time into `version.cpp` and exposed via `GetVersionInfo()` / `HsBaGetVersionJson()`. The CMake option `HSBA_COPL` (tri-state, default `AUTO`) controls whether CGAL/OpenCascade are found and linked, resolving from `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` environment variables or probing installed packages.

### Non-vcpkg code

The directory `third_parts_without_vcpkg/` exists as a placeholder for code intentionally not managed by vcpkg; coverage collection explicitly excludes it. No files currently reside there.

### Web UI dependencies

`webui/package.json` defines a private npm package (`hsba-rest-console`) targeting Node ≥ 20 with scripts for `start`, `test`, and `test:e2e`. No `package-lock.json` is committed, so dependency resolution is not reproducible at the npm level.

## Conventions and Constraints

- All C/C++ third-party libraries are declared in `vcpkg.json`; no `find_package(...)` calls search system-installed libraries directly for these components.
- Every dependency is scoped to one or more platforms using the `platform` field; Android and iOS receive reduced dependency sets as documented in `docs/en/vcpkg-dependencies.md`.
- Copyleft kernels (`cgal`, `opencascade`) are never direct dependencies — they enter exclusively through the `copyleft` feature, keeping MIT builds free of them.
- The vcpkg revision is pinned to a specific commit in `vcpkg-configuration.json`, making dependency versions deterministic across machines.
- Custom vcpkg ports and triplets belong under `overlay-ports/` and `triplets/` respectively, following the README placeholders.
- Coverage analysis excludes both `third_parts_without_vcpkg` and `vcpkg_installed` from instrumentation.
- On Windows, vcpkg's bundled `applocal.ps1` is relied upon for deploying dependent DLLs alongside the built executables (see `cmake/deploy_dlls.cmake`).
- The REST API feature requires `cpp-httplib` with the `openssl` feature enabled, which in turn requires the top-level `openssl` dependency already present.