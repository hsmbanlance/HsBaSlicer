# Getting Started

<cite>
**Referenced Files in This Document**   
- [CMakeLists.txt](file://CMakeLists.txt)
- [CMakePresets.json](file://CMakePresets.json)
- [vcpkg.json](file://vcpkg.json)
- [vcpkg-configuration.json](file://vcpkg-configuration.json)
- [README.md](file://README.md)
- [android/README.md](file://android/README.md)
- [docs/en/README.md](file://docs/en/README.md)
- [docs/zh/README.md](file://docs/zh/README.md)
- [utils/logcfg.ini](file://utils/logcfg.ini)
</cite>

## Update Summary
**Changes Made**
- Updated prerequisites section to reflect CMake 3.21+ requirement and enhanced compiler support
- Added comprehensive platform-specific setup instructions for Windows, Linux, macOS, Android, and iOS
- Enhanced build process documentation with detailed vcpkg configuration and dependency management
- Expanded troubleshooting section with platform-specific solutions
- Added verification steps for all supported platforms including mobile development
- Updated CMake presets documentation to include iOS support and improved cross-compilation guidance

## Table of Contents
1. [Introduction](#introduction)
2. [Prerequisites](#prerequisites)
3. [Windows Setup (Visual Studio 2022)](#windows-setup-visual-studio-2022)
4. [Linux Setup (Ubuntu 20.04+)](#linux-setup-ubuntu-2004)
5. [macOS Setup](#macos-setup)
6. [Android Build Setup](#android-build-setup)
7. [iOS Build Setup](#ios-build-setup)
8. [CMake Presets Configuration](#cmake-presets-configuration)
9. [Build Process](#build-process)
10. [Shared vs Static Libraries](#shared-vs-static-libraries)
11. [Troubleshooting Common Issues](#troubleshooting-common-issues)
12. [Verification and Testing](#verification-and-testing)

## Introduction
This guide provides comprehensive instructions for setting up the HsBaSlicer development environment across Windows, Linux, macOS, Android, and iOS platforms. The project uses CMake as the build system with vcpkg for dependency management, supporting cross-platform compilation through standardized presets. This document details the complete setup process including prerequisites, platform-specific configurations, build procedures, and verification steps.

HsBaSlicer is a high-performance C++ software framework for 3D printing slicing, providing modular and cross-platform slicing core capabilities with comprehensive documentation available in both English and Chinese.

## Prerequisites
Before building HsBaSlicer, ensure the following tools are installed:

- **CMake 3.21+**: Required for project configuration (minimum 3.12 recommended)
- **vcpkg**: C++ library manager for dependency resolution with modern registry support
- **Git**: Version control system for cloning repositories
- **C++20 Compliant Compiler**: Required for modern C++ features including concepts, ranges, and coroutines
- **Build Tools**:
  - Windows: Visual Studio 2022 with "Desktop development with C++" workload
  - Linux: GCC 11+, Ninja, make, and development libraries
  - macOS: Xcode Command Line Tools with Clang
  - Android: Android NDK r27d+ and Gradle
  - iOS: Xcode 14+ with iOS deployment target 16.3+

The project requires advanced C++20 features including concepts, ranges, std::source_location, non-type template parameters, and template-template parameter matching.

**Section sources**
- [CMakeLists.txt:4-38](file://CMakeLists.txt#L4-L38)
- [README.md:57-57](file://README.md#L57-L57)
- [vcpkg-configuration.json:1-21](file://vcpkg-configuration.json#L1-L21)

## Windows Setup (Visual Studio 2022)
To set up the development environment on Windows:

1. Install **Visual Studio 2022** with the following components:
   - Desktop development with C++
   - CMake tools for Visual Studio
   - Git for Windows
   - MSVC v143 - VS 2022 C++ x64/x86 build tools

2. Install **vcpkg**:
```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

3. Set environment variable:
```cmd
set VCPKG_ROOT=C:\path\to\vcpkg
```

4. Clone the repository:
```cmd
git clone https://github.com/hsmbanlance/HsBaSlicer.git
cd HsBaSlicer
```

5. Use CMake GUI or command line:
```cmd
cmake -S . -B out/build -G "Ninja" ^
-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

6. Build the project:
```cmd
cmake --build out/build
```

**Note**: If you need to copy DLL and PDB files, configure and build again after the first compilation. For debugging in Visual Studio Code, add the vcpkg installation path to environment variables.

**Section sources**
- [README.md:59-70](file://README.md#L59-L70)
- [CMakePresets.json:5-20](file://CMakePresets.json#L5-L20)

## Linux Setup (Ubuntu 20.04+)
For Ubuntu 20.04 or later systems:

1. Install system dependencies:
```bash
sudo apt update
sudo apt install git cmake ninja-build g++ gdb make rsync zip python3 python3-venv
```

2. Install X11 development packages (required for OpenCASCADE):
```bash
sudo apt install libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxmu-dev libxmu-headers libxrender-dev libxtst-dev autoconf-archive libfontconfig1-dev
```

3. Install database build dependencies:
```bash
sudo apt install libncurses-dev libtirpc-dev bison flex
```

4. Install pkg-config and build tools:
```bash
sudo apt install pkg-config automake libtool m4 autoconf python3-distutils
```

5. Install and bootstrap vcpkg:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=$PWD
```

6. Clone and build HsBaSlicer:
```bash
cd ..
git clone https://github.com/hsmbanlance/HsBaSlicer.git
cd HsBaSlicer
cmake -S . -B out/build -G "Ninja" \
-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build out/build
```

**Section sources**
- [README.md:72-168](file://README.md#L72-L168)
- [CMakePresets.json:41-82](file://CMakePresets.json#L41-L82)

## macOS Setup
For macOS development (note: not fully tested but builds successfully in CI):

1. Install Homebrew if not already installed:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

2. Install required dependencies:
```bash
brew install cmake ninja python autoconf automake libtool autoconf-archive
```

3. Install and bootstrap vcpkg:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=$PWD
```

4. Clone and build HsBaSlicer:
```bash
cd ..
git clone https://github.com/hsmbanlance/HsBaSlicer.git
cd HsBaSlicer
cmake -S . -B out/build -G "Ninja" \
-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build out/build
```

**Section sources**
- [README.md:170-178](file://README.md#L170-L178)
- [CMakePresets.json:111-150](file://CMakePresets.json#L111-L150)

## Android Build Setup
To build for Android:

1. Install **Android NDK** and set environment variable:
```bash
export ANDROID_NDK_HOME=/path/to/android-ndk
```

2. Ensure vcpkg is installed and configured.

3. Configure the build with Android-specific settings:
```bash
mkdir -p build-android && cd build-android
cmake .. \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
-DVCPKG_TARGET_TRIPLET=arm64-android \
-DANDROID_ABI=arm64-v8a \
-DANDROID_PLATFORM=android-28 \
-DANDROID_NDK=$ANDROID_NDK_HOME
```

4. Build native libraries:
```bash
cmake --build . --config Release
```

5. Copy generated .so files to Android project:
```bash
cp android_libs/arm64-v8a/*.so android/app/src/main/jniLibs/arm64-v8a/
```

6. Open the android folder in Android Studio and build the APK.

**Note**: Requires Android SDK version supporting API level 28+ and NDK r27d+ for C++20 support.

**Section sources**
- [android/README.md:7-21](file://android/README.md#L7-L21)
- [CMakePresets.json:84-109](file://CMakePresets.json#L84-L109)
- [README.md:180-195](file://README.md#L180-L195)

## iOS Build Setup
For iOS development (note: not fully tested but builds successfully in CI):

1. Install required dependencies:
```bash
brew install cmake ninja python autoconf automake libtool autoconf-archive
```

2. Install and bootstrap vcpkg:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=$PWD
```

3. Configure iOS build using CMake preset:
```bash
cmake . --preset ios-release
```

4. Build the project:
```bash
cd out/build/ios-release
cmake --build .
```

**Note**: Requires Xcode 14+ with iOS deployment target 16.3+ for C++20 support. Uses Xcode generator instead of Ninja for iOS compatibility.

**Section sources**
- [README.md:197-205](file://README.md#L197-L205)
- [CMakePresets.json:153-176](file://CMakePresets.json#L153-L176)

## CMake Presets Configuration
The `CMakePresets.json` file defines platform-specific build configurations:

- **windows-base**: Base preset for Windows using Ninja generator with MSVC compilers
- **x64-debug/x64-release**: Configuration presets inheriting from windows-base
- **linux-debug/linux-release**: Linux configurations with Debug/Release builds
- **android-release**: Android-specific configuration with NDK toolchain (arm64-v8a)
- **macos-debug/macos-release**: macOS configurations (not fully tested)
- **ios-release**: iOS configuration using Xcode generator with arm64 architecture

Key features:
- Uses vcpkg toolchain file via `VCPKG_ROOT` environment variable
- Sets appropriate binary and install directories
- Configures target triplets for cross-compilation
- Applies platform-specific cache variables
- Supports Visual Studio Remote Settings integration

The presets automatically detect the host system and apply appropriate settings, enabling consistent builds across platforms.

**Section sources**
- [CMakePresets.json:1-179](file://CMakePresets.json#L1-L179)
- [README.md:43-43](file://README.md#L43-L43)

## Build Process
The complete build process follows these steps:

1. **Clone repository**:
```bash
git clone https://github.com/hsmbanlance/HsBaSlicer.git
cd HsBaSlicer
```

2. **Bootstrap vcpkg**:
```bash
git clone https://github.com/Microsoft/vcpkg.git
./bootstrap-vcpkg.sh  # Linux/macOS
.\bootstrap-vcpkg.bat  # Windows
```

3. **Configure with CMake**:
```bash
cmake -S . -B out/build -G "Ninja" \
-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

4. **Build project**:
```bash
cmake --build out/build
```

5. **Install dependencies** specified in `vcpkg.json`, which includes:
   - Boost libraries (asio, graph, polygon, etc.)
   - CGAL and libigl for computational geometry
   - OpenCASCADE for CAD operations
   - Protocol Buffers for serialization
   - SQLite3, MySQL, PostgreSQL via sqlpp11
   - Image processing libraries (OpenCV, libpng, libjpeg-turbo)
   - Modern C++ utilities (magic_enum, rapidjson, yaml-cpp)

The project uses modern vcpkg configuration with registries and overlay ports for enhanced dependency management.

**Section sources**
- [vcpkg.json:1-93](file://vcpkg.json#L1-L93)
- [vcpkg-configuration.json:1-21](file://vcpkg-configuration.json#L1-L21)
- [CMakeLists.txt:32-157](file://CMakeLists.txt#L32-L157)

## Shared vs Static Libraries
The build system supports both shared and static library configurations:

### Static Libraries
- Default configuration when vcpkg triplet ends with `-static`
- LibHsBaSlicer built as STATIC library
- Defined in `LibHsBaSlicer/CMakeLists.txt`

### Shared Libraries
- Default for non-static triplets
- LibHsBaSlicer built as SHARED library
- Export symbols using `HSBA_SLICER_EXPORTS` macro
- Header files automatically included via `target_sources`

To build shared libraries:
```bash
cmake .. -DBUILD_SHARED_LIBS=ON
```

The DllHsBaSlicer component is always built as a shared library (DLL/.so) to provide system API exports. Platform-specific defaults are applied based on the target triplet:
- Windows/Xbox: Dynamic by default
- Linux/Android: Static by default
- Other platforms: Static by default

**Section sources**
- [CMakeLists.txt:288-311](file://CMakeLists.txt#L288-L311)

## Troubleshooting Common Issues
### Linux: Missing X11 Dependencies
**Symptom**: OpenCASCADE fails to compile  
**Solution**: Install X11 development packages:
```bash
sudo apt install libx11-dev mesa-common-dev libglu1-mesa-dev libxi-dev libxmu-dev
```

### Windows: Visual Studio Component Errors
**Symptom**: MSBuild or C++ tools not found  
**Solution**: 
1. Repair Visual Studio installation
2. Ensure "Desktop development with C++" workload is selected
3. Verify CMake tools are installed

### vcpkg: Dependency Installation Failures
**Symptom**: Package installation fails  
**Solution**:
```bash
./vcpkg remove --outdated
./vcpkg install <package> --clean-after-build
```

### CMake: Toolchain File Not Found
**Symptom**: CMAKE_TOOLCHAIN_FILE error  
**Solution**: Verify `VCPKG_ROOT` environment variable points to vcpkg installation directory.

### Android: NDK Configuration Issues
**Symptom**: Android toolchain not found  
**Solution**: Set `ANDROID_NDK_HOME` environment variable to NDK installation path.

### iOS: Xcode Configuration Problems
**Symptom**: iOS build fails with Xcode errors  
**Solution**: 
1. Ensure Xcode 14+ is installed
2. Verify iOS deployment target is 16.3+
3. Check that Xcode command line tools are selected

### Compiler Feature Support Issues
**Symptom**: C++20 feature detection failures  
**Solution**: Ensure your compiler supports required C++20 features including concepts, ranges, and coroutines.

**Section sources**
- [README.md:82-105](file://README.md#L82-L105)
- [CMakePresets.json:93-97](file://CMakePresets.json#L93-L97)

## Verification and Testing
After successful compilation:

1. **Verify executable location**:
   - Windows: `out/build/HsBaSlicer/HsBaSlicer.exe`
   - Linux: `out/build/HsBaSlicer/HsBaSlicer`
   - macOS: `out/build/HsBaSlicer/HsBaSlicer`
   - Android: Generated .so files in `android_libs/` directory
   - iOS: Built frameworks in Xcode build directory

2. **Check dependency copying**:
   - DllHsBaSlicer outputs are automatically copied to the executable directory
   - Configuration file `logcfg.ini` is copied during build

3. **Run basic functionality test**:
   - Execute the HsBaSlicer binary
   - Verify logging system initializes (checks `logcfg.ini`)
   - Confirm library loading and basic operations

4. **Test suite** (if enabled):
   - Located in `tests/` directory
   - Automatically included when `HSBA_SLICER_USE_TESTS=ON`
   - Disabled on Android and iOS platforms due to output limitations

5. **Platform-specific verification**:
   - **Android**: Confirm .so files are generated and can be loaded via `System.loadLibrary()`
   - **iOS**: Verify framework generation and linking in Xcode projects
   - **Cross-compilation**: Test executables are built but not executed when cross-compiling

The CI system automatically verifies builds across multiple platforms as indicated by the workflow badge in README.md.

**Section sources**
- [CMakeLists.txt:459-475](file://CMakeLists.txt#L459-L475)
- [utils/logcfg.ini](file://utils/logcfg.ini)
- [README.md:55-55](file://README.md#L55-L55)