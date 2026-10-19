---
kind: business_term
name: Business Glossary
category: business_term
scope:
    - '**'
---

### HsBaSlicer
- Definition：The project's product name: a high-performance, modular, cross-platform C++ framework providing slicing-core capabilities for 3D-printing pipelines (FDM/SLA), distributed across static/shared library targets (`LibHsBaSlicer`, `DllHsBaSlicer`, `HsBaSlicerBase`, etc.) and installable via `find_package(HsBaSlicer)`.

### 切片核心能力
- Definition：Internal shorthand for the slicing core capability set exposed by the framework — mesh slicing, model preprocessing, FDM/SLA support generation, polygon filling, path generation, and floor generation — packaged as the `LibHsBaSlicer` / `DllHsBaSlicer` targets.
- Aliases：切片核心

### 条件双许可（Conditional Dual License）
- Definition：The project's licensing model: builds that link copyleft kernels (CGAL GPL-3.0-or-later, OpenCascade LGPL-2.1-only) ship under GPL-3.0-or-later; builds without those kernels (iOS, game consoles, or any non-copyleft configuration) ship under MIT. The effective license is determined at configure time from `vcpkg.json` and queryable at runtime via `GetVersionInfo()` / `HsBaGetVersionJson()`.
- Aliases：条件双许可、dual license

### copyleft（特性）
- Definition：The vcpkg feature that pulls in CGAL, libigl[cgal], and opencascade, thereby switching the project's effective license from MIT to GPL-3.0-or-later. It is enabled by default on Windows/Linux/macOS/Android via `default-features`.
- Aliases：copyleft feature

### linux-coverage 预设
- Definition：The CMake preset used for WSL/CI coverage gating: Debug build with `-fprofile-arcs -ftest-coverage`, samples/docs/static-tests disabled, and coverage thresholds `HSBA_COVERAGE_THRESHOLD_CORE` / `HSBA_COVERAGE_THRESHOLD_OTHER` pre-set. Paired with `linux-coverage-report` (runs the `coverage` target) and `linux-coverage` test preset.
- Aliases：coverage 预设

### 覆盖率门禁
- Definition：The CI quality gate enforced by `cmake/coverage.cmake`: after ctest produces `.gcda` files, gcovr aggregates coverage and fails the build if core modules fall below the configured threshold (≥80%) or other modules fall below theirs (≥60%).
- Aliases：coverage gate
