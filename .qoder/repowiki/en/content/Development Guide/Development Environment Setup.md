# Development Environment Setup

<cite>
**Referenced Files in This Document**
- [README.md](file://README.md)
- [CMakeLists.txt](file://CMakeLists.txt)
- [vcpkg.json](file://vcpkg.json)
- [vcpkg-configuration.json](file://vcpkg-configuration.json)
- [CMakePresets.json](file://CMakePresets.json)
- [docs/en/vcpkg-dependencies.md](file://docs/en/vcpkg-dependencies.md)
- [docs/en/cpp_cmake_usage.md](file://docs/en/cpp_cmake_usage.md)
- [.clang-format](file://.clang-format)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [System Requirements](#system-requirements)
3. [Required Tools](#required-tools)
4. [Dependency Management with vcpkg](#dependency-management-with-vcpkg)
5. [Step-by-Step Installation](#step-by-step-installation)
6. [IDE Configuration](#ide-configuration)
7. [Code Formatting Rules](#code-formatting-rules)
8. [Optional Dependencies](#optional-dependencies)
9. [Troubleshooting Guide](#troubleshooting-guide)
10. [Conclusion](#conclusion)

## Introduction
This document explains how to set up a complete development environment for HsBaSlicer. It covers required tools, platform-specific requirements, dependency management through vcpkg, IDE configuration for Visual Studio, CLion, and VS Code, debugging setup, IntelliSense configuration, code formatting rules, optional dependencies such as OpenGL, SQLite, and OpenSSL, and common troubleshooting steps.

HsBaSlicer is a high-performance C++ framework for 3D-printing slicers. It uses modern C++20 features, CMake 3.28+, and vcpkg for dependency resolution. The project supports Windows, Linux, macOS, Android, and iOS, with different feature sets per platform.

## System Requirements
The project requires a modern toolchain and build system:

- **CMake**: version 3.28 or newer.
- **C++ compiler**: must support C++20.
- **vcpkg**: recommended dependency manager.
- **PowerShell**: on Windows, PowerShell 7 is recommended; Windows PowerShell 5.1 is acceptable.

Platform notes:
- Desktop platforms (Windows, Linux, macOS): full feature set including copyleft kernels when enabled.
- Android: reduced dependency set; some desktop-only libraries are excluded.
- iOS: limited dependency set due to platform constraints.

**Section sources**
- [README.md:41-55](file://README.md#L41-L55)
- [CMakeLists.txt:4-18](file://CMakeLists.txt#L4-L18)

## Required Tools
### Minimum Tool Versions
| Tool | Minimum Version | Notes |
|------|----------------|-------|
| CMake | 3.28 | Project enforces `cmake_minimum_required(VERSION 3.28)` |
| C++ Compiler | C++20 capable | GCC 14+, Clang 16+, MSVC 19.34+ recommended for module support |
| vcpkg | Latest stable | Used via toolchain file and manifest |
| PowerShell | 7 (recommended) | Windows builds and scripts benefit from PowerShell 7 |

Compiler capability checks:
- Concepts, ranges, source_location, non-type template parameters, template-template matching are required.
- Coroutines and explicit object parameter are optional but detected at configure time.
- C++20 modules support is optional; if available, the module wrapper target can be built.

**Section sources**
- [CMakeLists.txt:4-18](file://CMakeLists.txt#L4-L18)
- [CMakeLists.txt:48-104](file://CMakeLists.txt#L48-L104)
- [CMakeLists.txt:116-127](file://CMakeLists.txt#L116-L127)
- [docs/en/cpp_cmake_usage.md:155-158](file://docs/en/cpp_cmake_usage.md#L155-L158)

## Dependency Management with vcpkg
HsBaSlicer uses vcpkg for dependency resolution. The manifest defines core dependencies and an optional “copyleft” feature that enables CGAL and OpenCascade.

Key points:
- Default registry baseline is pinned for reproducible builds.
- Overlay ports and triplets are supported via local directories.
- Platform-specific dependencies are declared using `platform` filters.
- The “copyleft” feature adds CGAL, libigl[cgal], and OpenCascade, switching the effective license to GPL-3.0-or-later.

Dependency categories:
- Core: Boost components, Clipper2, Eigen3, Protobuf, RapidJSON, tinyxml2, yaml-cpp, Lua, OpenSSL, magic_enum.
- Geometry and mesh: libigl, CGAL (optional), OpenCASCADE (optional).
- Image and compression: OpenCV, miniz, bit7z (optional).
- Point cloud: OpenVDB (desktop/mobile excludes vary).
- Database: sqlpp11 with SQLite3, MySQL, PostgreSQL (desktop); SQLite only on mobile.

**Section sources**
- [vcpkg.json:1-113](file://vcpkg.json#L1-L113)
- [vcpkg-configuration.json:1-21](file://vcpkg-configuration.json#L1-L21)
- [docs/en/vcpkg-dependencies.md:1-21](file://docs/en/vcpkg-dependencies.md#L1-L21)

## Step-by-Step Installation
### Windows
Recommended setup:
1. Install Visual Studio 2022 or later with “Desktop development with C++”, CMake tools, and vcpkg.
2. Install Git.
3. Add vcpkg path to system environment variables so it is accessible from IDE/command line.
4. Use the `windows-release` preset for release builds; `windows-debug` may have issues with CGAL in submodules.

Build example:
- Configure with CMake presets:
  - `cmake --preset x64-release`
- Build:
  - `cmake --build out/build/x64-release --config Release`

Notes:
- Presets use Ninja generator and set the vcpkg toolchain automatically.
- On Windows, dynamic library loader is enabled by default for desktop builds.

**Section sources**
- [README.md:57-62](file://README.md#L57-L62)
- [CMakePresets.json:1-40](file://CMakePresets.json#L1-L40)
- [CMakeLists.txt:228-240](file://CMakeLists.txt#L228-L240)

### Linux / macOS
Example installation (Ubuntu/Debian):
```bash
sudo apt update
sudo apt install -y cmake git python3 python3-venv g++ gdb make ninja-build pkg-config automake libtool m4 autoconf python3-distutils libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxrender-dev libxtst-dev bison flex libncurses-dev libtirpc-dev libfontconfig1-dev rsync zip
```

Install vcpkg:
```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Build project:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

Or use CMake presets:
```bash
cmake . --preset linux-release
cd out/build/linux-release
cmake --build . --config Release
```

Notes:
- Presets define binary and install directories under `out/build/<presetName>` and `out/install/<presetName>`.
- Coverage preset is available for Linux/WSL/CI with gcov/gcovr.

**Section sources**
- [README.md:63-94](file://README.md#L63-L94)
- [CMakePresets.json:41-110](file://CMakePresets.json#L41-L110)

### Android / iOS
Android:
- Typically built on Linux with Android SDK/NDK installed.
- Set `ANDROID_NDK_HOME` and use the Android preset which configures NDK toolchain and ABI.

iOS:
- CI verified; local macOS builds not extensively tested.
- iOS preset uses Xcode generator and sets deployment target and architecture.

**Section sources**
- [README.md:96-98](file://README.md#L96-L98)
- [CMakePresets.json:112-205](file://CMakePresets.json#L112-L205)

## IDE Configuration
### Visual Studio
- Use Visual Studio 2022 or later with C++ workload and CMake integration.
- Select a CMake preset (e.g., `x64-release`, `linux-release`, `macos-release`) from the CMake targets view.
- Ensure vcpkg is bootstrapped and its path is in environment variables.
- For debugging:
  - Choose Debug configuration.
  - Set working directory to the binary output directory (`bin/Debug` or `bin/Release`).
  - Attach debugger to process or run via CMake launch profile.

IntelliSense:
- CMake preset provides compiler and include paths.
- If IntelliSense fails, refresh CMake project or reconfigure with the correct toolchain.

Formatting:
- Configure clang-format in Visual Studio settings to use `.clang-format`.
- Enable format-on-save if desired.

**Section sources**
- [CMakePresets.json:1-40](file://CMakePresets.json#L1-L40)
- [README.md:57-62](file://README.md#L57-L62)

### CLion
- Open the project root; CLion will detect CMakeLists.txt and presets.
- Configure CMake toolchain to use vcpkg:
  - In Settings > Build, Execution, Deployment > Toolchains, ensure CMake options include `-DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake`.
- Select a preset from the CMake profiles list.
- Debugging:
  - Create a CMake Run/Debug Configuration targeting the executable.
  - Set working directory to the build output bin folder.

IntelliSense:
- CLion uses CMake configuration for includes and macros.
- If symbols are unresolved, rebuild CMake project and verify vcpkg toolchain.

Formatting:
- Enable clang-format integration and point to `.clang-format`.

**Section sources**
- [CMakePresets.json:41-110](file://CMakePresets.json#L41-L110)
- [docs/en/cpp_cmake_usage.md:82-99](file://docs/en/cpp_cmake_usage.md#L82-L99)

### VS Code
- Install C/C++ extension and CMake Tools extension.
- Configure CMake preset selection in `.vscode/c_cpp_properties.json` or via CMake Tools UI.
- Ensure vcpkg toolchain is configured:
  - In CMake Tools settings, set `cmake.configureArgs` to include `-DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake`.
- Debugging:
  - Use CMake Tasks and Launch configurations generated by CMake Tools.
  - Set `cwd` to the binary output directory.

IntelliSense:
- C/C++ extension reads CMake configuration for include paths.
- If IntelliSense lags, reload window or regenerate CMake cache.

Formatting:
- Enable clang-format in VS Code settings and associate with `.clang-format`.

**Section sources**
- [CMakePresets.json:1-40](file://CMakePresets.json#L1-L40)
- [docs/en/cpp_cmake_usage.md:82-99](file://docs/en/cpp_cmake_usage.md#L82-L99)

## Code Formatting Rules
HsBaSlicer uses clang-format with Allman brace style and specific indentation rules:

- Language: C++
- Standard: c++17 (formatting standard; project compiles with C++20)
- Indent width: 4 spaces
- Tabs: disabled
- Continuation indent: 4 spaces
- Braces: Allman style (always on new line)
- Column limit: 120
- Pointer/reference alignment: left
- Short functions: inline only
- Include sorting: case-sensitive, preserve blocks
- Spaces around operators and punctuation follow defined rules

To apply formatting:
- Use `clang-format` CLI or integrate into your IDE/editor.
- Ensure `.clang-format` is present at repository root.

**Section sources**
- [.clang-format:1-99](file://.clang-format#L1-L99)

## Optional Dependencies
### OpenGL
OpenGL-related system packages are installed in the Linux example (mesa-common-dev, libglu1-mesa-dev, etc.). These provide OpenGL headers and runtime libraries used by graphics-capable components.

- Linux example installs Mesa and GLU development packages.
- On Windows/macOS, these are typically provided by the platform SDK or drivers.

**Section sources**
- [README.md:63-70](file://README.md#L63-L70)

### SQLite
SQLite support is provided via sqlpp11. On desktop platforms, MySQL and PostgreSQL features are included; on mobile platforms, only SQLite is enabled.

- Desktop: sqlpp11 with SQLite3, MySQL, PostgreSQL.
- Mobile: sqlpp11 with SQLite3 only.

**Section sources**
- [vcpkg.json:67-82](file://vcpkg.json#L67-L82)
- [CMakeLists.txt:329-336](file://CMakeLists.txt#L329-L336)

### OpenSSL
OpenSSL is a required dependency for encryption and hashing functionality.

- Found via CMake’s `find_package(OpenSSL REQUIRED)`.
- Transitive dependencies exposed to consumers include OpenSSL.

**Section sources**
- [CMakeLists.txt:273-274](file://CMakeLists.txt#L273-L274)
- [docs/en/cpp_cmake_usage.md:461-472](file://docs/en/cpp_cmake_usage.md#L461-L472)

### Copyleft Kernels (CGAL, OpenCASCADE)
These are optional and controlled by the “copyleft” feature:
- Enabling copyleft switches the project license to GPL-3.0-or-later.
- CGAL and OpenCASCADE are linked only when the feature is enabled.
- Desktop platforms support both; Android supports CGAL via feature; iOS excludes OpenCASCADE.

**Section sources**
- [vcpkg.json:89-111](file://vcpkg.json#L89-L111)
- [docs/en/vcpkg-dependencies.md:1-21](file://docs/en/vcpkg-dependencies.md#L1-L21)
- [CMakeLists.txt:170-204](file://CMakeLists.txt#L170-L204)

## Troubleshooting Guide
Common issues and resolutions:

- **CMake version too old**:
  - Upgrade to CMake 3.28+.
  - Verify with `cmake --version`.

- **Compiler does not support required C++20 features**:
  - Use GCC 14+, Clang 16+, or MSVC 19.34+.
  - Check configure logs for feature detection messages.

- **vcpkg toolchain not found**:
  - Ensure vcpkg is bootstrapped and `VCPKG_ROOT` or PATH is set.
  - Confirm toolchain file path in CMake presets or command line.

- **Dependency resolution failures**:
  - Update vcpkg registry baseline or clear cached packages.
  - Review platform exclusions in `vcpkg.json` and `docs/en/vcpkg-dependencies.md`.

- **Android/iOS build issues**:
  - Use reduced dependency sets as documented.
  - Ensure NDK paths are correctly set (`ANDROID_NDK_HOME`).

- **OpenGL missing on Linux**:
  - Install Mesa and GLU development packages as shown in the Linux example.

- **SQLite/MySQL/PostgreSQL linkage**:
  - On desktop, ensure sqlpp11 features are enabled.
  - On mobile, only SQLite is expected.

- **Copyleft kernel licensing**:
  - Disable default features to produce MIT-licensed builds without CGAL/OpenCASCADE.
  - Use environment variable `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` before configuring.

**Section sources**
- [README.md:41-55](file://README.md#L41-L55)
- [docs/en/vcpkg-dependencies.md:72-80](file://docs/en/vcpkg-dependencies.md#L72-L80)
- [CMakeLists.txt:48-104](file://CMakeLists.txt#L48-L104)
- [CMakeLists.txt:170-204](file://CMakeLists.txt#L170-L204)

## Conclusion
Setting up HsBaSlicer requires a modern C++20 toolchain, CMake 3.28+, and vcpkg for dependency management. Follow the platform-specific instructions to install prerequisites, configure IDEs for debugging and IntelliSense, and adhere to the project’s code formatting rules. Optional dependencies like OpenGL, SQLite, and OpenSSL are integrated via system packages and vcpkg, while copyleft kernels are controlled by a feature flag that affects licensing. Use the troubleshooting guide to resolve common setup and build issues.