# Multi-Platform Build System

<cite>
**Referenced Files in This Document**
- [CMakeLists.txt](file://CMakeLists.txt)
- [CMakePresets.json](file://CMakePresets.json)
- [vcpkg.json](file://vcpkg.json)
- [vcpkg-configuration.json](file://vcpkg-configuration.json)
- [README.md](file://README.md)
- [LibHsBaSlicer/CMakeLists.txt](file://LibHsBaSlicer/CMakeLists.txt)
- [DllHsBaSlicer/CMakeLists.txt](file://DllHsBaSlicer/CMakeLists.txt)
- [ModuleHsBaSlicer/CMakeLists.txt](file://ModuleHsBaSlicer/CMakeLists.txt)
- [ModuleHsBaSlicer/hsba_slicer.cppm](file://ModuleHsBaSlicer/hsba_slicer.cppm)
- [ModuleHsBaSlicer/module_anchor.cpp](file://ModuleHsBaSlicer/module_anchor.cpp)
- [base/CMakeLists.txt](file://base/CMakeLists.txt)
- [cmake/HsBaSlicerConfig.cmake.in](file://cmake/HsBaSlicerConfig.cmake.in)
- [cmake/deploy_dlls.cmake](file://cmake/deploy_dlls.cmake)
- [android/build.gradle](file://android/build.gradle)
- [android/app/build.gradle](file://android/app/build.gradle)
- [static_check/feature_check.cmake](file://static_check/feature_check.cmake)
- [base/coroutine.hpp](file://base/coroutine.hpp)
- [LibHsBaSlicer/Slice/mesh_slice.hpp](file://LibHsBaSlicer/Slice/mesh_slice.hpp)
- [DllHsBaSlicer/fdm_pipeline.h](file://DllHsBaSlicer/fdm_pipeline.h)
- [preprocess/ModelLoader.hpp](file://preprocess/ModelLoader.hpp)
- [support/FdmSupport.hpp](file://support/FdmSupport.hpp)
- [.github/workflows/build-android.yml](file://.github/workflows/build-android.yml)
- [.github/workflows/cmake-multi-platform.yml](file://.github/workflows/cmake-multi-platform.yml)
</cite>

## Update Summary
**Changes Made**
- Enhanced Android build system with clang-scan-deps support for improved incremental build performance
- Updated GitHub Actions workflows to include CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS configuration parameter
- Added comprehensive documentation for Android build optimization and dependency tracking improvements

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Conclusion](#conclusion)
10. [Appendices](#appendices)

## Introduction
This document explains the multi-platform build system for HsBaSlicer, focusing on how CMake 3.28+, vcpkg, and platform-specific presets orchestrate a consistent build across Windows, Linux, macOS, Android, iOS, and game consoles. The system has been significantly enhanced with C++20 module support, comprehensive installation capabilities, improved cross-platform compatibility, and optimized Android builds with clang-scan-deps for superior incremental build performance. It documents the modernized module layout, dependency management, shared vs static library configuration, and the integration points that enable the FDM pipeline (preprocess, slice, support, fill, path generation) to be built and linked uniformly using both traditional headers and C++20 modules.

## Project Structure
The repository is organized into feature-based modules with a top-level CMake orchestrating subprojects. Key aspects:
- Top-level CMake configures compiler standards, feature detection, platform flags, and third-party dependencies via vcpkg/pkg-config.
- Subdirectories define libraries and executables (e.g., base, 2D, paths, preprocess, support, meshmodel, convert, LibHsBaSlicer, DllHsBaSlicer, HsBaSlicer).
- **New**: ModuleHsBaSlicer provides C++20 module interface for modern consumers.
- CMake presets standardize cross-platform builds using Ninja or Xcode generators and integrate vcpkg toolchains.
- Unified output directories place all artifacts under `bin/<configuration>` for consistent deployment.
- Android project uses Gradle to consume prebuilt native artifacts; iOS/macOS use Xcode generator presets.
- **Enhanced**: Android builds now leverage clang-scan-deps for significantly improved incremental compilation performance.

```mermaid
graph TB
Root["Top-level CMake 3.28+<br/>Feature checks, options, deps"] --> Base["base (HsBaSlicerBase)"]
Root --> Utils["utils (HsBaSlicerUtils)"]
Root --> TwoD["2D (HsBaSlicer2D)"]
Root --> Paths["paths (HsBaPaths)"]
Root --> Preprocess["preprocess (HsBaPreprocess)"]
Root --> Support["support (HsBaSupport)"]
Root --> Mesh["meshmodel (HsBaSlicerMesh)"]
Root --> Convert["convert"]
Root --> CAD["cadmodel (HsBaSlicerCADModel)"]
Root --> Lib["LibHsBaSlicer (static/shared)"]
Root --> Module["ModuleHsBaSlicer (C++20 module)"]
Root --> Dll["DllHsBaSlicer (shared)"]
Root --> App["HsBaSlicer (app)"]
Root --> Tests["tests / static_tests"]
Root --> Docs["docs"]
Root --> Install["Installation & Export<br/>CMake Package Config + FILE_SET"]
AndroidCI[".github/workflows/build-android.yml<br/>clang-scan-deps enabled"] --> Root
MultiPlatform[".github/workflows/cmake-multi-platform.yml<br/>clang-scan-deps enabled"] --> Root
```

**Diagram sources**
- [CMakeLists.txt:304-328](file://CMakeLists.txt#L304-L328)
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)
- [ModuleHsBaSlicer/CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)
- [.github/workflows/cmake-multi-platform.yml:268-276](file://.github/workflows/cmake-multi-platform.yml#L268-L276)

**Section sources**
- [CMakeLists.txt:1-107](file://CMakeLists.txt#L1-L107)
- [CMakeLists.txt:304-328](file://CMakeLists.txt#L304-L328)
- [README.md:41-194](file://README.md#L41-L194)

## Core Components
- **Enhanced CMake Requirements**: Minimum CMake 3.28 for C++20 module support and FILE_SET functionality.
- Compiler and language standards: Enforces C++20 and conditional C23 where supported.
- **Advanced Feature Detection**: Concepts, ranges, source_location, NTTP, template-template matching, coroutines, explicit this, and C++20 modules.
- Platform detection: Desktop vs mobile vs game console toggles features like logging, dynamic loader, CGAL, OpenCASCADE, SQL backends.
- Dependency resolution: vcpkg-managed packages with platform-scoped features (e.g., sqlpp11 SQLite-only on mobile).
- Library type control: Shared vs static based on VCPKG_TARGET_TRIPLET or user option.
- Unified output directories: All binaries placed in `bin/<configuration>` for consistent deployment.
- **Comprehensive Installation Support**: Full CMake package configuration with export targets, header installation, and FILE_SET for C++20 modules.
- **Optimized Android Builds**: Enhanced with clang-scan-deps for superior incremental compilation performance.

Key behaviors:
- HSBA_DESKTOP, HSBA_MOBILE, HSBA_GAME_CONSOLE flags gate optional subsystems.
- Optional bit7z compression and dynamic loader disabled on non-desktop platforms.
- Boolean operations disabled in Debug due to performance/memory constraints.
- Game console detection via VCPKG_TARGET_TRIPLET patterns (xbox, switch, playstation, stadia).
- **C++20 Modules**: Optional module building with automatic compiler capability detection.
- **Android Optimization**: clang-scan-deps integration enables faster rebuilds by tracking precise file dependencies.

**Updated** Enhanced with CMake 3.28 minimum requirement, optional C++20 module support, and Android clang-scan-deps optimization.

**Section sources**
- [CMakeLists.txt:17-38](file://CMakeLists.txt#L17-L38)
- [CMakeLists.txt:48-104](file://CMakeLists.txt#L48-L104)
- [CMakeLists.txt:108-136](file://CMakeLists.txt#L108-L136)
- [CMakeLists.txt:160-194](file://CMakeLists.txt#L160-L194)
- [CMakeLists.txt:226-237](file://CMakeLists.txt#L226-L237)
- [CMakeLists.txt:280-302](file://CMakeLists.txt#L280-L302)
- [CMakeLists.txt:108-119](file://CMakeLists.txt#L108-L119)
- [static_check/feature_check.cmake:96-111](file://static_check/feature_check.cmake#L96-L111)

## Architecture Overview
The build architecture integrates four layers:
- Configuration layer: CMake 3.28+ + CMakePresets + vcpkg configuration.
- Module layer: Feature-based libraries and executables with optional C++20 modules.
- Platform layer: OS-specific toolchains, generators, and ABI settings.
- Installation layer: CMake package configuration, target export system, and FILE_SET support.
- **Enhanced CI layer**: Optimized workflows with clang-scan-deps for Android builds.

```mermaid
graph TB
Presets["CMakePresets.json<br/>Windows/Linux/macOS/iOS/Android presets"] --> CMakeRoot["Top-level CMakeLists.txt (3.28+)"]
VcpkgCfg["vcpkg-configuration.json<br/>registries, overlays, triplets"] --> CMakeRoot
VcpkgJson["vcpkg.json<br/>dependencies per platform"] --> CMakeRoot
BuildAndroid[".github/workflows/build-android.yml<br/>clang-scan-deps enabled"] --> CMakeRoot
CMakeMulti[".github/workflows/cmake-multi-platform.yml<br/>clang-scan-deps enabled"] --> CMakeRoot
CMakeRoot --> Modules["Submodules (base, utils, 2D, paths, preprocess, support, meshmodel, convert, cadmodel)"]
Modules --> LibHsBaSlicer["LibHsBaSlicer (static/shared)"]
LibHsBaSlicer --> ModuleHsBaSlicer["ModuleHsBaSlicer (C++20 module)"]
ModuleHsBaSlicer --> DllHsBaSlicer["DllHsBaSlicer (shared)"]
DllHsBaSlicer --> App["HsBaSlicer (executable)"]
CMakeRoot --> Tests["tests/static_tests"]
CMakeRoot --> Docs["docs"]
CMakeRoot --> Install["Installation & Export<br/>Package Config + FILE_SET"]
Install --> ConfigFile["HsBaSlicerConfig.cmake.in"]
Install --> Targets["HsBaSlicerTargets.cmake"]
Install --> Version["HsBaSlicerConfigVersion.cmake"]
Install --> ModulesSet["FILE_SET hsba_slicer_modules"]
```

**Diagram sources**
- [CMakePresets.json:1-179](file://CMakePresets.json#L1-L179)
- [vcpkg-configuration.json:1-21](file://vcpkg-configuration.json#L1-L21)
- [vcpkg.json:1-93](file://vcpkg.json#L1-L93)
- [CMakeLists.txt:304-328](file://CMakeLists.txt#L304-L328)
- [cmake/HsBaSlicerConfig.cmake.in:1-16](file://cmake/HsBaSlicerConfig.cmake.in#L1-L16)
- [ModuleHsBaSlicer/CMakeLists.txt:17-20](file://ModuleHsBaSlicer/CMakeLists.txt#L17-20)
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)
- [.github/workflows/cmake-multi-platform.yml:268-276](file://.github/workflows/cmake-multi-platform.yml#L268-L276)

## Detailed Component Analysis

### Top-level CMake Configuration
Responsibilities:
- Set minimum CMake version to 3.28 for C++20 module support.
- Configure C++20 and conditional C23.
- Run static feature checks including C++20 modules capability detection.
- Detect platform family and set desktop/mobile/console flags.
- Resolve third-party libraries via pkg-config and find_package.
- Toggle optional features (bit7z, dynamic loader, boolean ops, CAD kernel, SQL backends).
- Control BUILD_SHARED_LIBS based on VCPKG_TARGET_TRIPLET.
- Include subdirectories for all modules and optional tests/docs.
- **Unified output directories**: All binaries placed in `bin/<configuration>`.
- **Comprehensive installation**: Full CMake package configuration with export targets and FILE_SET support.

```mermaid
flowchart TD
Start(["Configure"]) --> Standards["Set C++20 / C23"]
Standards --> Features["Run feature checks + C++20 modules"]
Features --> Platform["Detect platform family"]
Platform --> Deps["Find dependencies (Boost, Clipper2, miniz, protobuf, etc.)"]
Deps --> Options["Apply platform options (bit7z, DLL loader, CGAL, OCCT, SQL)"]
Options --> Linkage["Decide static vs shared via triplet"]
Linkage --> OutputDirs["Set unified output dirs (bin/<config>)"]
OutputDirs --> Install["Configure installation & export + FILE_SET"]
Install --> Subdirs["Add subdirectories (modules, tests, docs)"]
Subdirs --> End(["Configure complete"])
```

**Diagram sources**
- [CMakeLists.txt:1-107](file://CMakeLists.txt#L1-L107)
- [CMakeLists.txt:108-136](file://CMakeLists.txt#L108-L136)
- [CMakeLists.txt:138-237](file://CMakeLists.txt#L138-L237)
- [CMakeLists.txt:280-302](file://CMakeLists.txt#L280-L302)
- [CMakeLists.txt:304-328](file://CMakeLists.txt#L304-L328)
- [CMakeLists.txt:277-286](file://CMakeLists.txt#L277-L286)
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)

**Updated** Enhanced with CMake 3.28 minimum requirement and C++20 modules support.

**Section sources**
- [CMakeLists.txt:1-107](file://CMakeLists.txt#L1-L107)
- [CMakeLists.txt:108-136](file://CMakeLists.txt#L108-L136)
- [CMakeLists.txt:138-237](file://CMakeLists.txt#L138-L237)
- [CMakeLists.txt:280-302](file://CMakeLists.txt#L280-L302)
- [CMakeLists.txt:304-328](file://CMakeLists.txt#L304-L328)
- [CMakeLists.txt:277-286](file://CMakeLists.txt#L277-L286)
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)

### C++20 Module System (ModuleHsBaSlicer)
**New** Comprehensive C++20 module support with class-based API:
- **Module Interface**: Single-file module (`hsba_slicer.cppm`) providing modern C++ API.
- **Automatic Detection**: Compiler capability detection for MSVC 19.34+, GCC 14+, Clang 16+.
- **Conditional Building**: Optional module building controlled by `HSBA_SLICER_MODULE` option.
- **Modern API Design**: Class-based RAII interfaces replacing free functions.
- **FILE_SET Integration**: Proper installation of module interface files.

```mermaid
classDiagram
class ModuleHsBaSlicer {
+Static library with CXX_MODULES FILE_SET
+C++20 module interface (hsba_slicer.cppm)
+Class-based API wrapping LibHsBaSlicer
+Automatic compiler detection
}
class Model {
+RAII model handle
+translate(), rotate(), scale()
+slice(), sliceD()
+info(), raw(), name()
}
class FdmPipeline {
+Full FDM pipeline
+run(), sliceAll(), generateSupports()
+fill(), generatePath()
}
class SlaPipeline {
+SLA pipeline with floor/render
+run(), generateFloor(), renderLayer()
+savePackage()
}
ModuleHsBaSlicer --> Model : "exports"
ModuleHsBaSlicer --> FdmPipeline : "exports"
ModuleHsBaSlicer --> SlaPipeline : "exports"
ModuleHsBaSlicer --> LibHsBaSlicer : "links"
```

**Diagram sources**
- [ModuleHsBaSlicer/CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [ModuleHsBaSlicer/hsba_slicer.cppm:114-276](file://ModuleHsBaSlicer/hsba_slicer.cppm#L114-L276)

**Section sources**
- [ModuleHsBaSlicer/CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [ModuleHsBaSlicer/hsba_slicer.cppm:1-642](file://ModuleHsBaSlicer/hsba_slicer.cppm#L1-642)
- [ModuleHsBaSlicer/module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-13)
- [CMakeLists.txt:108-119](file://CMakeLists.txt#L108-L119)
- [static_check/feature_check.cmake:96-111](file://static_check/feature_check.cmake#L96-L111)

### CMake Presets and Toolchains
- Provides named presets for Windows, Linux, macOS, iOS, and Android.
- Uses Ninja as primary generator; Xcode for iOS.
- Integrates vcpkg toolchain file and sets binary/install directories under out/.
- Android preset configures NDK, ABI, API level, and CMAKE_SYSTEM_NAME.
- iOS preset targets arm64 with deployment target 16.3.
- **Consistent build directory structure**: All presets use `${sourceDir}/out/build/${presetName}`.

```mermaid
sequenceDiagram
participant Dev as "Developer"
participant CMake as "CMake CLI"
participant Preset as "CMakePresets.json"
participant Vcpkg as "vcpkg.cmake"
Dev->>CMake : cmake . --preset <name>
CMake->>Preset : Load preset (generator, vars, condition)
Preset-->>CMake : Toolchain file, build type, arch
CMake->>Vcpkg : Use vcpkg toolchain
Vcpkg-->>CMake : Resolved dependencies
CMake-->>Dev : Configure complete (output : out/build/<preset>/bin/<config>)
```

**Diagram sources**
- [CMakePresets.json:1-179](file://CMakePresets.json#L1-L179)

**Section sources**
- [CMakePresets.json:1-179](file://CMakePresets.json#L1-L179)

### vcpkg Integration and Dependencies
- Centralized dependency list with platform scoping and features.
- Registries include default git baseline and Microsoft artifact registry.
- Overlay ports and triplets allow local overrides.
- **Platform-specific dependencies**: Different dependency sets for desktop, mobile, and game console targets.

```mermaid
graph LR
VcpkgCfg["vcpkg-configuration.json"] --> Reg["Default registry (git baseline)"]
VcpkgCfg --> ArtReg["Microsoft artifact registry"]
VcpkgCfg --> Overlays["overlay-ports / overlay-triplets"]
VcpkgJson["vcpkg.json"] --> Deps["Dependencies by platform/features"]
Deps --> CMake["find_package() / pkg_check_modules()"]
Deps --> Mobile["Mobile-specific deps (SQLite only)"]
Deps --> Desktop["Desktop-specific deps (MySQL, PostgreSQL)"]
```

**Diagram sources**
- [vcpkg-configuration.json:1-21](file://vcpkg-configuration.json#L1-L21)
- [vcpkg.json:1-93](file://vcpkg.json#L1-L93)

**Updated** Enhanced platform-specific dependency management with mobile vs desktop configurations.

**Section sources**
- [vcpkg-configuration.json:1-21](file://vcpkg-configuration.json#L1-L21)
- [vcpkg.json:1-93](file://vcpkg.json#L1-L93)

### Installation and Package Configuration
**Enhanced** Comprehensive installation support with CMake package configuration:
- **Target export**: All libraries exported with `HsBaSlicer::` namespace.
- **Header installation**: Public headers installed to structured include directories.
- **FILE_SET support**: C++20 module interface files properly installed.
- **Package config files**: Generated `HsBaSlicerConfig.cmake` and version files.
- **Dependency management**: Automatic dependency resolution via `find_dependency()`.
- **Cross-platform compatibility**: Proper handling of runtime/library/archive destinations.

```mermaid
flowchart TD
Install["install(TARGETS ...)"] --> Export["Export HsBaSlicerTargets"]
Export --> FileSet["Install FILE_SET hsba_slicer_modules"]
FileSet --> Config["configure_package_config_file()"]
Config --> Version["write_basic_package_version_file()"]
Config --> FindDep["find_dependency() calls"]
FindDep --> Usage["External projects use find_package(HsBaSlicer)"]
```

**Diagram sources**
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)
- [cmake/HsBaSlicerConfig.cmake.in:1-16](file://cmake/HsBaSlicerConfig.cmake.in#L1-L16)
- [ModuleHsBaSlicer/CMakeLists.txt:17-20](file://ModuleHsBaSlicer/CMakeLists.txt#L17-20)

**Section sources**
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)
- [cmake/HsBaSlicerConfig.cmake.in:1-16](file://cmake/HsBaSlicerConfig.cmake.in#L1-L16)

### Deployment Utilities
**Enhanced** Enhanced deployment support with specialized tools:
- **DLL deployment script**: `deploy_dlls.cmake` for copying PDB debug symbols.
- **Configuration-aware copying**: Automatically copies logcfg.ini to correct build configuration directory.
- **Conditional execution**: Skips missing files gracefully (Release mode may not generate PDBs).

```mermaid
flowchart TD
Build["Build completes"] --> DeployScript["deploy_dlls.cmake"]
DeployScript --> CheckFiles["Check if PDB files exist"]
CheckFiles --> Copy["Copy existing PDBs to exe directory"]
Copy --> Done["Deployment complete"]
```

**Diagram sources**
- [cmake/deploy_dlls.cmake:1-27](file://cmake/deploy_dlls.cmake#L1-L27)

**Section sources**
- [cmake/deploy_dlls.cmake:1-27](file://cmake/deploy_dlls.cmake#L1-L27)
- [CMakeLists.txt:277-286](file://CMakeLists.txt#L277-L286)

### Enhanced Android Build System with clang-scan-deps
**New** Significantly improved Android build performance through clang-scan-deps integration:
- **Incremental Build Optimization**: clang-scan-deps enables precise dependency tracking for faster rebuilds.
- **GitHub Actions Integration**: Both Android workflow files now include CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS parameter.
- **NDK Integration**: Leverages Android NDK's built-in clang-scan-deps tool for optimal performance.
- **CI/CD Enhancement**: Automated builds benefit from reduced compilation times in continuous integration environments.

```mermaid
flowchart TD
AndroidWorkflow[".github/workflows/build-android.yml"] --> CMakeConfig["CMake Configuration"]
CMakeConfig --> ScanDeps["CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS"]
ScanDeps --> NDKTool["Android NDK clang-scan-deps"]
NDKTool --> Incremental["Improved Incremental Builds"]
Incremental --> FasterRebuild["Faster Compilation Times"]
CMakeMulti[".github/workflows/cmake-multi-platform.yml"] --> CMakeConfig2["CMake Configuration"]
CMakeConfig2 --> ScanDeps2["CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS"]
ScanDeps2 --> NDKTool2["Android NDK clang-scan-deps"]
NDKTool2 --> Incremental2["Improved Incremental Builds"]
Incremental2 --> FasterRebuild2["Faster Compilation Times"]
```

**Diagram sources**
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)
- [.github/workflows/cmake-multi-platform.yml:268-276](file://.github/workflows/cmake-multi-platform.yml#L268-L276)

**Section sources**
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)
- [.github/workflows/cmake-multi-platform.yml:268-276](file://.github/workflows/cmake-multi-platform.yml#L268-L276)

### Module: base (HsBaSlicerBase)
- Static library providing core utilities, coroutine primitives, object pools, thread pool, units, reflection helpers.
- Links Eigen3 and Boost; locale/nowide excluded on mobile/console.
- Exposes magic_enum.
- **Platform-specific linking**: Conditional linking based on target platform capabilities.

```mermaid
classDiagram
class HsBaSlicerBase {
+Static library
+Coroutines (Task, Generator)
+Object/Thread/Memory pools
+Units, reflection, delegates
}
HsBaSlicerBase --> Eigen3 : "links"
HsBaSlicerBase --> Boost : "links"
HsBaSlicerBase --> magic_enum : "links"
HsBaSlicerBase ..> iconv : "iOS only"
```

**Diagram sources**
- [base/CMakeLists.txt:1-49](file://base/CMakeLists.txt#L1-L49)

**Updated** Enhanced platform-specific linking with iOS iconv support.

**Section sources**
- [base/CMakeLists.txt:1-49](file://base/CMakeLists.txt#L1-L49)

### Module: LibHsBaSlicer (Public API surface)
- Builds as static or shared depending on global BUILD_SHARED_LIBS.
- Includes Slice, Preprocess, Support, Fill, Path modules.
- Links against base, utils, mesh, 2D, preprocess, support, paths, and optionally CAD model.
- Applies precompiled headers and export macros when shared.
- **Conditional CAD model linking**: Only links CAD model on desktop platforms.

```mermaid
graph TB
LHS["LibHsBaSlicer"] --> Base["HsBaSlicerBase"]
LHS --> Utils["HsBaSlicerUtils"]
LHS --> Mesh["HsBaSlicerMesh"]
LHS --> TwoD["HsBaSlicer2D"]
LHS --> Prep["HsBaPreprocess"]
LHS --> Supp["HsBaSupport"]
LHS --> Paths["HsBaPaths"]
LHS -. desktop only .-> CAD["HsBaSlicerCADModel"]
```

**Diagram sources**
- [LibHsBaSlicer/CMakeLists.txt:1-59](file://LibHsBaSlicer/CMakeLists.txt#L1-L59)

**Section sources**
- [LibHsBaSlicer/CMakeLists.txt:1-59](file://LibHsBaSlicer/CMakeLists.txt#L1-L59)

### Module: DllHsBaSlicer (System API)
- Shared library exposing C-compatible API for FDM pipeline.
- Links LibHsBaSlicer and underlying modules.
- Defines enums, structs, and functions for synchronous/asynchronous execution and progress callbacks.
- **Export macro configuration**: Properly configured for both static and shared builds.

```mermaid
classDiagram
class DllHsBaSlicer {
+Exports C API
+FDM pipeline config/result
+Sync/Async run functions
+Progress/result callbacks
}
DllHsBaSlicer --> LibHsBaSlicer : "links"
DllHsBaSlicer --> HsBaSlicerBase : "links"
DllHsBaSlicer --> HsBaSlicerUtils : "links"
DllHsBaSlicer --> HsBaSlicer2D : "links"
DllHsBaSlicer --> HsBaPreprocess : "links"
DllHsBaSlicer --> HsBaSupport : "links"
DllHsBaSlicer --> HsBaPaths : "links"
```

**Diagram sources**
- [DllHsBaSlicer/CMakeLists.txt:1-20](file://DllHsBaSlicer/CMakeLists.txt#L1-L20)
- [DllHsBaSlicer/fdm_pipeline.h:1-147](file://DllHsBaSlicer/fdm_pipeline.h#L1-L147)

**Section sources**
- [DllHsBaSlicer/CMakeLists.txt:1-20](file://DllHsBaSlicer/CMakeLists.txt#L1-L20)
- [DllHsBaSlicer/fdm_pipeline.h:1-147](file://DllHsBaSlicer/fdm_pipeline.h#L1-L147)

### Coroutines Foundation (Task/Generator)
- Provides Task<T>, Task<void>, CustomAllocatorTask, IExecutor, AsyncExecutor, NoopExecutor, DispatchAwaiter, and Generator<T>.
- Enables asynchronous pipelines with co_await semantics and completion callbacks.

```mermaid
classDiagram
class IExecutor {
+execute(func)
}
class NoopExecutor
class AsyncExecutor
class Task~T, Executor~ {
+then(cb)
+catching(cb)
+finally(cb)
+get_result()
}
class Task~void, Executor~ {
+then(cb)
+catching(cb)
+finally(cb)
+get_result()
}
class CustomAllocatorTask~T, Executor, Allocator~
class Generator~T~ {
+yield_value(value)
+initial_suspend()
+final_suspend()
}
IExecutor <|-- NoopExecutor
IExecutor <|-- AsyncExecutor
Task~T, Executor~ ..> IExecutor : "uses"
Task~void, Executor~ ..> IExecutor : "uses"
CustomAllocatorTask~T, Executor, Allocator~ ..> IExecutor : "uses"
```

**Diagram sources**
- [base/coroutine.hpp:1-800](file://base/coroutine.hpp#L1-L800)

**Section sources**
- [base/coroutine.hpp:1-800](file://base/coroutine.hpp#L1-L800)

### Slice Interface (Existing)
- Exposes safe and unsafe slicing APIs returning polygonal layers at a given height.
- Also provides Lua-scripted variants.

```mermaid
flowchart TD
Model["IModel"] --> Slice["Slice(height) -> Polygons"]
Model --> UnsafeSlice["UnSafeSlice(height) -> UnSafePolygons"]
Model --> SliceLua["SliceLua(script,height) -> Polygons"]
Model --> UnsafeSliceLua["UnSafeSliceLua(script,height) -> UnSafePolygons"]
```

**Diagram sources**
- [LibHsBaSlicer/Slice/mesh_slice.hpp:1-28](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L1-L28)

**Section sources**
- [LibHsBaSlicer/Slice/mesh_slice.hpp:1-28](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L1-L28)

### FDM Pipeline API (C-compatible)
- Defines configuration struct, result struct, progress callback, and sync/async entry points.
- Intended to orchestrate preprocess -> slice -> support -> fill -> path generation internally.

```mermaid
sequenceDiagram
participant Client as "Client"
participant API as "DllHsBaSlicer"
participant Pipe as "Pipeline (internal)"
Client->>API : HsBaRunFdmPipeline(config, callback, user_data)
API->>Pipe : Execute steps (preprocess/slice/support/fill/path)
Pipe-->>API : Progress updates via callback
Pipe-->>API : Result (G-code, stats)
API-->>Client : HsBaFdmPipelineResult_t
Client->>API : HsBaFreePipelineResult(result)
```

**Diagram sources**
- [DllHsBaSlicer/fdm_pipeline.h:1-147](file://DllHsBaSlicer/fdm_pipeline.h#L1-L147)

**Section sources**
- [DllHsBaSlicer/fdm_pipeline.h:1-147](file://DllHsBaSlicer/fdm_pipeline.h#L1-L147)

### Preprocessing and Support Primitives
- ModelLoader manages named models with automatic format selection and optional CGAL boolean/shell operations.
- FDM support implementations provide plane, tree, and honeycomb strategies.

```mermaid
classDiagram
class ModelLoader {
+LoadModel(name, filePath)
+GetModel(name)
+InsertModel(name, model)
+RemoveModel(name)
+ContainsModel(name)
+ModelCount()
+GetModelNames()
+Cleanup()
}
class FdmPlaneSupport {
+Generate(current_layer, prev_layer, layer_height, config)
}
class FdmTreeSupport {
+Generate(...)
-GenerateBranches(...)
}
class FdmHoneycombSupport {
+Generate(...)
-GenerateHoneycomb(...)
}
```

**Diagram sources**
- [preprocess/ModelLoader.hpp:1-131](file://preprocess/ModelLoader.hpp#L1-L131)
- [support/FdmSupport.hpp:1-63](file://support/FdmSupport.hpp#L1-L63)

**Section sources**
- [preprocess/ModelLoader.hpp:1-131](file://preprocess/ModelLoader.hpp#L1-L131)
- [support/FdmSupport.hpp:1-63](file://support/FdmSupport.hpp#L1-L63)

### Android Integration
- Gradle wrapper and app module configure namespace, SDK versions, ABI filters, and jniLibs directory.
- Native libraries are consumed from prebuilt artifacts produced by CMake presets.
- **Unified output consumption**: Gradle expects artifacts in standardized locations.
- **Enhanced CI Integration**: GitHub Actions workflows now leverage clang-scan-deps for faster Android builds.

```mermaid
graph TB
Gradle["android/build.gradle"] --> AppGradle["android/app/build.gradle"]
AppGradle --> JNILibs["jniLibs.srcDirs = src/main/jniLibs"]
AppGradle --> ABIs["abiFilters arm64-v8a"]
AppGradle --> CMakeOut["CMake output: out/build/android-release/bin/Release"]
AndroidCI[".github/workflows/build-android.yml<br/>clang-scan-deps enabled"] --> CMakeOut
```

**Diagram sources**
- [android/build.gradle:1-17](file://android/build.gradle#L1-L17)
- [android/app/build.gradle:1-45](file://android/app/build.gradle#L1-L45)
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)

**Section sources**
- [android/build.gradle:1-17](file://android/build.gradle#L1-L17)
- [android/app/build.gradle:1-45](file://android/app/build.gradle#L1-L45)

## Dependency Analysis
High-level linkage between major components:

```mermaid
graph TB
Base["HsBaSlicerBase"] --> Lib["LibHsBaSlicer"]
Utils["HsBaSlicerUtils"] --> Lib
TwoD["HsBaSlicer2D"] --> Lib
Prep["HsBaPreprocess"] --> Lib
Supp["HsBaSupport"] --> Lib
Mesh["HsBaSlicerMesh"] --> Lib
Paths["HsBaPaths"] --> Lib
CAD["HsBaSlicerCADModel"] -. desktop only .-> Lib
Lib --> Module["ModuleHsBaSlicer (C++20)"]
Module --> Dll["DllHsBaSlicer"]
Dll --> Install["Installation & Export"]
Install --> External["External Projects"]
AndroidCI[".github/workflows/build-android.yml<br/>clang-scan-deps"] --> Lib
MultiPlatform[".github/workflows/cmake-multi-platform.yml<br/>clang-scan-deps"] --> Lib
```

**Diagram sources**
- [LibHsBaSlicer/CMakeLists.txt:37-50](file://LibHsBaSlicer/CMakeLists.txt#L37-L50)
- [ModuleHsBaSlicer/CMakeLists.txt:23-29](file://ModuleHsBaSlicer/CMakeLists.txt#L23-L29)
- [DllHsBaSlicer/CMakeLists.txt:12-20](file://DllHsBaSlicer/CMakeLists.txt#L12-L20)
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)
- [.github/workflows/cmake-multi-platform.yml:268-276](file://.github/workflows/cmake-multi-platform.yml#L268-L276)

**Updated** Added C++20 module layer, enhanced installation/export layer for external project usage, and Android clang-scan-deps optimization.

**Section sources**
- [LibHsBaSlicer/CMakeLists.txt:37-50](file://LibHsBaSlicer/CMakeLists.txt#L37-L50)
- [ModuleHsBaSlicer/CMakeLists.txt:23-29](file://ModuleHsBaSlicer/CMakeLists.txt#L23-L29)
- [DllHsBaSlicer/CMakeLists.txt:12-20](file://DllHsBaSlicer/CMakeLists.txt#L12-L20)
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)

## Performance Considerations
- Boolean operations disabled in Debug builds to avoid high memory usage and slow CGAL/IGL performance.
- Model pool size reduced on constrained platforms (mobile/console) to limit memory footprint.
- Dynamic loader and bit7z disabled on non-desktop platforms to reduce runtime overhead and complexity.
- Prefer Release builds for production to optimize slicing and path generation throughput.
- **Game console optimizations**: Specific feature gating for console platforms to minimize resource usage.
- **C++20 modules benefits**: Faster compile times and improved build performance for module consumers.
- **Android clang-scan-deps optimization**: Significantly improved incremental build performance through precise dependency tracking, reducing unnecessary recompilation during development and CI/CD processes.

**Updated** Added C++20 modules performance benefits, game console optimization considerations, and Android clang-scan-deps incremental build improvements.

## Troubleshooting Guide
Common issues and resolutions:
- Missing C++20 support: Ensure compiler meets requirements; CMake enforces C++20 and will fail early if concepts/ranges/source_location are unavailable.
- **CMake 3.28 requirement**: Upgrade CMake to 3.28+ for C++20 module support and FILE_SET functionality.
- vcpkg not found: Provide VCPKG_ROOT environment variable or pass -DCMAKE_TOOLCHAIN_FILE explicitly; verify vcpkg-configuration.json registries and baselines.
- Android build fails: Confirm ANDROID_NDK_HOME is set; use android-release preset; ensure ABI matches app module abiFilters.
- iOS build fails: Verify Xcode generator preset and deployment target >= 16.3; confirm arm64 architecture.
- Debug slowdowns: Switch to Release; boolean operations are intentionally disabled in Debug.
- **Installation issues**: Ensure proper CMake package configuration files are installed; check namespace usage (`HsBaSlicer::`).
- **Game console builds**: Verify VCPKG_TARGET_TRIPLET matches expected patterns (xbox, switch, playstation, stadia).
- **DLL deployment**: Use deploy_dlls.cmake script for PDB copying; ensure TARGET_BIN and PDB_FILES variables are set correctly.
- **C++20 modules not building**: Check compiler version (MSVC 19.34+, GCC 14+, Clang 16+) and HSBA_SLICER_MODULE option.
- **Module import errors**: Ensure consumer project also uses CMake 3.28+ and supports C++20 modules.
- **Android clang-scan-deps issues**: Verify Android NDK r27d+ is installed; ensure clang-scan-deps path is accessible; check that CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS points to correct NDK toolchain location.

**Updated** Added troubleshooting for CMake 3.28 requirement, C++20 modules, installation, game console builds, DLL deployment, and Android clang-scan-deps configuration.

**Section sources**
- [CMakeLists.txt:17-38](file://CMakeLists.txt#L17-38)
- [CMakeLists.txt:226-237](file://CMakeLists.txt#L226-L237)
- [CMakeLists.txt:108-119](file://CMakeLists.txt#L108-L119)
- [CMakePresets.json:84-109](file://CMakePresets.json#L84-L109)
- [CMakePresets.json:153-176](file://CMakePresets.json#L153-L176)
- [android/app/build.gradle:39-41](file://android/app/build.gradle#L39-L41)
- [CMakeLists.txt:110-126](file://CMakeLists.txt#L110-L126)
- [cmake/deploy_dlls.cmake:1-27](file://cmake/deploy_dlls.cmake#L1-L27)
- [static_check/feature_check.cmake:96-111](file://static_check/feature_check.cmake#L96-L111)
- [.github/workflows/build-android.yml:41-50](file://.github/workflows/build-android.yml#L41-L50)
- [.github/workflows/cmake-multi-platform.yml:222-231](file://.github/workflows/cmake-multi-platform.yml#L222-L231)

## Conclusion
The HsBaSlicer build system leverages modern CMake 3.28+ practices, vcpkg for dependency management, and platform presets to deliver a consistent, extensible, and efficient build across desktop and mobile environments. The recent enhancements introduce C++20 module support through ModuleHsBaSlicer, comprehensive installation capabilities with FILE_SET support, improved cross-platform compatibility including game console targets, and significantly optimized Android builds with clang-scan-deps for superior incremental compilation performance. The modular design cleanly separates core utilities, geometry kernels, and pipeline stages, while the dual API approach (traditional C ABI in DllHsBaSlicer and modern C++20 modules in ModuleHsBaSlicer) exposes ergonomic interfaces for both legacy and modern applications. The enhanced CMake package configuration enables seamless integration into external projects with proper dependency resolution and module support. The new Android build optimizations through clang-scan-deps integration provide substantial performance improvements in both development and continuous integration environments.

## Appendices

### Quick Build Commands
- Windows (Release): Select windows-release preset and build.
- Linux (Release): cmake . --preset linux-release && cd out/build/linux-release && cmake --build .
- Android (arm64): cmake . --preset android-release && cd out/build/android-release && cmake ..
- iOS (arm64): cmake . --preset ios-release
- **Installation**: cmake --install out/build/windows-release --prefix ./install
- **External project usage**: find_package(HsBaSlicer REQUIRED) in consumer CMakeLists.txt
- **C++20 modules**: Enable HSBA_SLICER_MODULE=ON for module support (requires CMake 3.28+)
- **Android with clang-scan-deps**: Ensure ANDROID_NDK is set to r27d+ and CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS is configured automatically in CI workflows.

**Updated** Added C++20 modules build instructions, updated CMake version requirements, and Android clang-scan-deps configuration guidance.

**Section sources**
- [README.md:47-194](file://README.md#L47-L194)
- [CMakePresets.json:1-179](file://CMakePresets.json#L1-L179)
- [CMakeLists.txt:345-457](file://CMakeLists.txt#L345-L457)
- [CMakeLists.txt:108-119](file://CMakeLists.txt#L108-L119)

### CMake Package Configuration
**Enhanced** External projects can now easily integrate HsBaSlicer with full C++20 module support:

```cmake
# In your project's CMakeLists.txt (requires CMake 3.28+)
find_package(HsBaSlicer REQUIRED)

# Traditional C API usage
target_link_libraries(your_app PRIVATE HsBaSlicer::DllHsBaSlicer)

# Modern C++20 modules usage (if available)
# target_link_libraries(your_app PRIVATE HsBaSlicer::ModuleHsBaSlicer)
```

The package configuration automatically handles:
- Dependency resolution (Eigen3, magic_enum, Clipper2, Lua, Protobuf, OpenSSL)
- Target namespace (`HsBaSlicer::`)
- Cross-platform compatibility
- Version checking and compatibility
- **C++20 module FILE_SET installation**

**Section sources**
- [cmake/HsBaSlicerConfig.cmake.in:1-16](file://cmake/HsBaSlicerConfig.cmake.in#L1-L16)
- [CMakeLists.txt:441-457](file://CMakeLists.txt#L441-L457)
- [ModuleHsBaSlicer/CMakeLists.txt:17-20](file://ModuleHsBaSlicer/CMakeLists.txt#L17-20)

### C++20 Module Usage Example
**New** Modern C++20 module consumer example:

```cpp
// Consumer application using C++20 modules
import hsba.slicer;

int main() {
    try {
        // Create model with RAII
        HsBa::Slicer::Model model("test", "model.stl");
        
        // Use FDM pipeline
        HsBa::Slicer::FdmPipeline pipeline;
        auto result = pipeline.run(model);
        
        std::cout << "Generated " << result.total_layers << " layers\n";
    } catch (const HsBa::Slicer::SlicerError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

**Section sources**
- [ModuleHsBaSlicer/hsba_slicer.cppm:114-276](file://ModuleHsBaSlicer/hsba_slicer.cppm#L114-L276)
- [ModuleHsBaSlicer/CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)

### Android clang-scan-deps Configuration Details
**New** Technical details for Android build optimization:

The clang-scan-deps integration is configured through the following parameters in GitHub Actions workflows:

- **Parameter**: `-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS="$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang-scan-deps"`
- **NDK Requirement**: Android NDK r27d or later
- **Benefit**: Enables precise dependency tracking for incremental builds
- **Impact**: Significantly reduces compilation time during development and CI/CD processes

Both workflow files have been updated to include this configuration:
- `.github/workflows/build-android.yml`: Line 95
- `.github/workflows/cmake-multi-platform.yml`: Line 276

**Section sources**
- [.github/workflows/build-android.yml:87-95](file://.github/workflows/build-android.yml#L87-L95)
- [.github/workflows/cmake-multi-platform.yml:268-276](file://.github/workflows/cmake-multi-platform.yml#L268-L276)