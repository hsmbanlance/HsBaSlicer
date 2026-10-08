---
kind: build_system
name: CMake + vcpkg 多平台构建体系与 CI/覆盖率门禁
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - cmake/coverage.cmake
    - cmake/HsBaSlicerConfig.cmake.in
    - cmake/deploy_dlls.cmake
    - version/Generator_Version.ps1
    - .github/workflows/cmake-multi-platform.yml
    - .github/workflows/build-linux.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/test-coverage.yml
    - android/app/build.gradle
    - static_check/feature_check.cmake
---

## 1. 构建系统概览

HsBaSlicer 采用 **CMake 3.28+** 作为唯一顶层构建系统，通过 `vcpkg` 管理第三方依赖，使用 **Ninja** 生成器（iOS 除外，使用 Xcode），并通过 `.github/workflows` 下的 GitHub Actions 在 Windows/Linux/macOS/Android/iOS 上执行 CI。项目同时提供 C++20 module、C ABI 动态库、静态库和可执行文件多种产物形态。

## 2. 关键文件与角色

- `CMakeLists.txt`：顶层工程入口，定义 C++20/C23 标准、特性探测、依赖查找、子目录装配、install/export、测试与文档开关。
- `CMakePresets.json`：预置的 configure/build/test preset，覆盖 windows/linux/macos/android/ios 及 coverage 构建。
- `vcpkg.json`：依赖清单，声明 Boost、OpenCV、Eigen、Protobuf、Lua、OpenSSL、libigl、CGAL、OpenCASCADE、OpenVDB、sqlpp11 等依赖及其平台条件与 copyleft feature。
- `cmake/HsBaSlicerConfig.cmake.in`：CMake package config 模板，配合 `write_basic_package_version_file` 安装 `HsBaSlicerTargets.cmake` 与版本文件。
- `cmake/coverage.cmake`：启用 `--coverage` 编译/链接选项，定义核心/非核心模块覆盖率阈值（默认 core 45% / other 25%），通过 gcovr 产出 JSON/XML/HTML 报告并 fail-under-line 门禁。
- `version/Generator_Version.ps1`：CMake 配置阶段调用的 PowerShell 脚本，解析 `vcpkg.json` 生成 `version.cpp`，注入产品名、许可证（MIT/GPL-3.0-or-later）、版本号、构建类型、目标 triplet、第三方库清单等信息。
- `.github/workflows/*.yml`：`build-linux.yml`、`build-windows.yml`、`build-macos.yml`、`build-android.yml`、`build-ios.yml`、`cmake-multi-platform.yml`、`test-coverage.yml`、`publish-wiki.yml`。
- `android/app/build.gradle` 与 `android/build.gradle`：Gradle 工程，负责将 CMake 产出的 `.so` 拷贝到 `jniLibs/arm64-v8a` 并打包 APK。
- `static_check/feature_check.cmake`：CMake 特性探测辅助脚本（concepts/ranges/source_location/nttp/template-template/_Generic/coroutines/explicit-this/C23 constexpr）。
- `cmake/deploy_dlls.cmake`：部署辅助脚本（用于复制运行时 DLL）。

## 3. 架构与约定

### 3.1 语言与编译器要求
- 强制 C++20（`CMAKE_CXX_STANDARD 20`），C 标准根据编译器版本选择 C11 或 C23（GCC ≥13 / Clang ≥16 / MSVC ≥19.36）。
- 所有静态库统一开启 `-fPIC`（`CMAKE_POSITION_INDEPENDENT_CODE ON`），以便最终链接为共享库。
- MSVC 强制 `/utf-8` 源编码。

### 3.2 特性门控
顶层 CMake 调用 `check_cxx_feature_*` 宏对 concepts、ranges、source_location、NTTP class、template-template matching 进行**致命检查**（缺失则 `message(FATAL_ERROR)`），coroutines/explicit-this/C23 constexpr 为可选特性，通过 `HSBA_HAS_*` 宏暴露给源码。

### 3.3 平台与后端开关
- `HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE`：由 `VCPKG_TARGET_TRIPLET` 推断，控制日志、DLL 加载器、bit7z、OpenCV、CGAL/OpenCASCADE、OpenVDB、SQL 后端等是否启用。
- `HSBA_COPL`（AUTO/ON/OFF）：控制 CGAL + OpenCASCADE 等 copyleft 内核；AUTO 模式下通过环境变量 `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 或 `find_package(CGAL|OpenCASCADE)` 探测，启用后项目许可证切换为 GPL-3.0-or-later。
- `HSBA_SLICER_MODULE`：可选构建 C++20 module 封装层 `ModuleHsBaSlicer`。
- `BUILD_SHARED_LIBS`：由 `VCPKG_TARGET_TRIPLET` 自动推导——Windows/Xbox 默认动态，Linux/Android/static 默认静态。

### 3.4 依赖管理（vcpkg）
- 所有外部依赖通过 `find_package(... CONFIG REQUIRED)` 查找，工具链通过 `CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` 注入。
- Android/iOS 通过 `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` 指向 NDK 的 `android.toolchain.cmake`，并使用 `arm64-android` / `arm64-ios` triplet。
- `default-features` 包含 `copyleft`，但可通过 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 关闭。
- Linux CI 使用 `actions/cache@v5` 缓存 `vcpkg/downloads`，key 基于 `vcpkg.json` 哈希；同时设置 `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite` 启用 GitHub Actions 二进制缓存。

### 3.5 输出目录与 install
- 统一输出到 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`（runtime/library 同目录）。
- 通过 `GNUInstallDirs` + `install(EXPORT HsBaSlicerTargets ... NAMESPACE HsBaSlicer::)` 导出 CMake targets，并生成 `HsBaSlicerConfig.cmake` / `HsBaSlicerConfigVersion.cmake`（`SameMajorVersion` 兼容性）。
- 头文件按 `HsBaSlicer/<subdir>` 结构安装：DllHsBaSlicer C API、LibHsBaSlicer 公开 API、base、logger、fileoperator、convert、proto 生成物等。

### 3.6 版本生成
`version/Generator_Version.ps1` 在 CMake 配置阶段被调用，读取 `vcpkg.json` 解析依赖列表与 copyleft feature，根据依赖许可证（含 GPL/LGPL 且非 OR 双许可）决定项目许可证是 MIT 还是 GPL-3.0-or-later，并生成 `version.cpp` 中的 `VersionInfo`。

### 3.7 测试与覆盖率
- 运行期测试：Boost.Test，通过 `enable_testing()` + `add_subdirectory("tests")` 注册，`ctest --parallel 2` 执行；交叉编译时禁用自动运行。
- 静态测试：`static_tests/` 下纯 `static_assert` 套件，独立开关 `HSBA_SLICER_USE_STATIC_TESTS`。
- 覆盖率：`HSBA_COVERAGE=ON` 或 `linux-coverage` preset 启用，对所有目标加 `--coverage`，通过自定义 `coverage` target 用 gcovr 分 core/other 两组阈值门禁（core 45%，other 25%），失败即 build 报错。

### 3.8 CI 流水线
- `cmake-multi-platform.yml`：push/PR 到 master 触发，矩阵 `windows-latest | ubuntu-latest | macos-latest`，Release 构建 + ctest。
- `build-android.yml` / `build-ios.yml`：分别安装 NDK r27d / iOS SDK，构建原生库并打包 APK（Gradle 8.7）。
- `test-coverage.yml`：Linux + gcov/gcovr，跑单元测试后执行覆盖率门禁。
- `publish-wiki.yml`：发布 Wiki。

## 4. 约定与约束

- **必须使用 CMake ≥ 3.28**：顶层 `cmake_minimum_required(VERSION 3.28)`，低于此版本直接失败。
- **必须支持 C++20 与 C11/C23**：缺失 concepts/ranges/source_location/NTTP class/template-template 匹配会 `FATAL_ERROR` 终止配置。
- **依赖必须通过 vcpkg 提供**：所有 `find_package(... CONFIG REQUIRED)` 失败都会导致配置失败；Android/iOS 需额外设置 `VCPKG_CHAINLOAD_TOOLCHAIN_FILE`。
- **Copyleft 内核受 HSBA_COPL 控制**：AUTO 模式优先读 `VCPKG_MANIFEST_FEATURES` 环境变量，其次回退到 `find_package(CGAL|OpenCASCADE)` 探测；启用后项目许可证变为 GPL-3.0-or-later。
- **Android/iOS 禁用部分功能**：`HSBA_USE_DLL_LOADER`、`HSBA_HSBA_USE_BIT7Z`、OpenCV、CGAL、OpenCASCADE、OpenVDB、MySQL/PostgreSQL 均被强制关闭并定义 `HSBA_NO_*` 宏。
- **测试在交叉编译时不执行**：`CMAKE_CROSSCOMPILING` 时 `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` 强制 OFF。
- **覆盖率门禁阈值不可随意降低**：`cmake/coverage.cmake` 注释明确要求“do not lower them without review”，threshold 作为 cache variable 暴露但默认值固定。
- **Android 产物路径约定**：CI 将 `build-android/android_libs/arm64-v8a/*.so` 复制到 `android/app/src/main/jniLibs/arm64-v8a/`，再由 Gradle 打包 APK。
- **iOS 产物为静态库**：CI 验证 `libHsBaSlicer.a`、`libDllHsBaSlicer.a`、`libLibHsBaSlicer.a` 存在并检查符号 `HsBaRunPipelineExamples`。
- **CMake Preset 是推荐入口**：`CMakePresets.json` 定义了 `x64-debug/release`、`linux-debug/release/coverage`、`android-release`、`macos-debug/release`、`ios-release` 等 preset，统一了 binaryDir/installDir/toolchainFile/cacheVariables。
