---
kind: build_system
name: CMake + vcpkg 多平台构建与 CI/容器化体系
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - cmake/coverage.cmake
    - cmake/HsBaWindowsUtf8.cmake
    - version/Generator_Version.ps1
    - docker/Dockerfile
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/docker-rest-api.yml
---

## 1. 构建系统概览

HsBaSlicer 采用 **CMake 3.28+** 作为唯一顶层构建系统，依赖管理通过 **vcpkg**（`vcpkg.json` + `vcpkg-configuration.json`）完成，生成器统一使用 **Ninja**（Windows/macOS/Linux），iOS 使用 Xcode。项目以 C++20 为基线，C 标准根据编译器能力在 C11/C23 之间选择。

## 2. 关键文件与目录

- 顶层构建：`CMakeLists.txt`、`CMakePresets.json`
- 依赖清单：`vcpkg.json`、`vcpkg-configuration.json`
- 平台预设：`cmake/HsBaWindowsUtf8.cmake`、`cmake/coverage.cmake`、`cmake/deploy_dlls.cmake`
- 版本生成：`version/Generator_Version.ps1`、`version/version.cpp.in`
- CI 工作流：`.github/workflows/build-{linux,windows,macos,android,iOS}.yml`、`docker-rest-api.yml`、`test-coverage.yml`
- 容器镜像：`docker/Dockerfile`、`docker/Dockerfile.dockerignore`
- Android 工程：`android/`（Gradle + NDK，独立于 CMake）
- Web UI：`webui/package.json`（Node.js，REST API 的配套前端）

## 3. 架构与约定

### 3.1 顶层 CMake 配置（`CMakeLists.txt`）

- 强制全局 `-fPIC`（所有静态库最终链接到共享库 `DllHsBaSlicer`）。
- 通过 `static_check/feature_check.cxx` 探测 C++ concepts/ranges/source_location/NTTP/template-template 等特性，缺失则 `FATAL_ERROR`；coroutines/explicit-this/C23 constexpr 为可选并导出 `HSBA_HAS_*` 宏。
- 平台判定：`VCPKG_TARGET_TRIPLET` 匹配 `xbox|switch|playstation|stadia` → `HSBA_IS_GAME_CONSOLE`；`WIN32/LINUX/(APPLE AND NOT IOS)` → `HSBA_DESKTOP`；`ANDROID/IOS` → `HSBA_MOBILE`。
- 可选功能门控：
  - `HSBA_COVERAGE` → 启用 gcov/gcovr 覆盖率目标（见 `cmake/coverage.cmake`）。
  - `HSBA_SLICER_MODULE` → C++20 module 包装（`ModuleHsBaSlicer`），需工具链支持且 GMF 不暴露 TU-local 实体。
  - `HSBA_BUILD_REST_API` → REST 服务（`restapi/`），仅桌面端，需 vcpkg feature `rest-api` 安装 `cpp-httplib`。
  - `HSBA_COPL`（AUTO/ON/OFF）→ CGAL/OpenCascade 开源内核，开启后项目许可证切换为 GPL-3.0-or-later。
  - `HSBA_USE_DLL_LOADER`、`HSBA_HSBA_USE_BIT7Z`、`HSBA_ENABLE_THREAD_POOL_COROUTINE` 等按平台关闭。
- 依赖发现顺序：Boost（log/log_setup）、Clipper2（pkg-config）、miniz、bit7z、Protobuf、RapidJSON、Eigen3、OpenSSL、OpenCV、libigl(+CGAL)、OpenCASCADE、OpenVDB、sqlpp11、Lua、tinyxml2、yaml-cpp、magic_enum。
- 输出目录统一：`bin/$<CONFIG>`；Debug 后缀 `d`。
- `install()` 导出 `HsBaSlicerTargets`（命名空间 `HsBaSlicer::`），并生成 `HsBaSlicerConfig.cmake` / `HsBaSlicerConfigVersion.cmake`（SameMajorVersion 兼容性）。

### 3.2 vcpkg 依赖与特性

- `default-features` 包含 `copyleft`（CGAL + libigl[cgal] + OpenCascade），默认在所有平台启用，但可通过 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 关闭。
- 两个可选 feature：
  - `copyleft`：GPL/LGPL 内核，触发项目许可证切换。
  - `rest-api`：`cpp-httplib`（带 openssl TLS），仅 Windows/Linux/macOS。
- 平台条件：Android/iOS 上 sqlpp11 仅 SQLite3；OpenCV/OpenVDB/CGAL/OCCT 在非移动端禁用。

### 3.3 CMake Presets（`CMakePresets.json`）

提供跨平台预设：`windows-base` → `x64-debug/release`；`linux-debug/release/coverage`；`android-release (arm64)`；`macos-debug/release`；`ios-release`。均指向 `out/build/${presetName}` / `out/install/${presetName}`，并通过 `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` 注入工具链。Linux 预设内置 VS Remote Settings 的源目录映射。

### 3.4 版本生成策略

`version/Generator_Version.ps1` 在 CMake 配置阶段运行，解析 `vcpkg.json` 的 dependencies + enabled features（含 default-features 的平台表达式求值），生成 `version.cpp`，其中包含：
- 项目名、版本号（硬编码 `1.0.0`）、构建类型、构建平台、vcpkg triplet、配置时间戳。
- 第三方库列表（名称/许可证/主页），由脚本内 `libInfoMap` 维护。
- 若任一依赖许可证匹配 `GPL` 且非 `X OR GPL` 双许可，则项目许可证设为 `GPL-3.0-or-later`，否则为 `MIT`。

### 3.5 覆盖率门禁（`cmake/coverage.cmake`）

- 仅 GCC/Clang 有效。
- 对所有目标添加 `--coverage` 编译/链接选项。
- 定义核心模块过滤集合（base、2D、utils、cipher、meshmodel、paths、preprocess、support、LibHsBaSlicer）与非核心模块。
- 默认阈值：核心 ≥45%、其他 ≥25%，未达标时 `gcovr --fail-under-line` 使构建失败。
- 提供 `coverage` 自定义目标，分两步：先解析 gcov 数据为 JSON，再分别对 core/other 执行阈值检查。

### 3.6 CI 流水线（`.github/workflows/`）

- `build-linux.yml`：Ubuntu runner，安装 Dockerfile 中列出的全部系统依赖（X11/GL/mesa/fontconfig/autoconf/meson/nasm/nodejs/Powershell 等），clone vcpkg，设置 `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`，缓存 `vcpkg/downloads`，用 Ninja + Release 构建，ctest 并行 2 执行测试。
- `build-windows.yml` / `build-macos.yml` / `build-android.yml` / `build-ios.yml`：各平台对应构建。
- `test-coverage.yml`：基于 `linux-coverage` preset 运行单元测试并触发覆盖率门禁。
- `docker-rest-api.yml`：构建 `docker/Dockerfile` 镜像。
- `rest-webui.yml`：Web UI 相关流程。

### 3.7 容器镜像（`docker/Dockerfile`）

两阶段构建：
- **builder**：Ubuntu + 完整构建依赖（与 `build-linux.yml` 对齐），bootstrap vcpkg，预拉取 cmake/ninja 二进制到 `VCPKG_DOWNLOADS`，以 `cmake --preset linux-release` 配置并构建 `HsBaSlicerRestServer`，安装到 `/opt/hsba-install`。`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 强制显式 `HSBA_FEATURES`（默认 `rest-api`）。
- **runtime**：最小 Ubuntu，仅安装运行时动态库（libssl3t64/zlib/libpng/fontconfig/freetype/mysql/postgresql/sqlite3），复制安装产物，修改 `restapi.ini` host 为 `0.0.0.0`，以非 root 用户 `hsba` 启动，EXPOSE 8080，HEALTHCHECK 调用 `/api/health`。

### 3.8 Android 与 iOS

- Android：独立的 Gradle 工程（`android/app/build.gradle`），NDK 交叉编译，CMake 通过 `CMakePresets.json` 中的 `android-release` 预设驱动。
- iOS：`ios/HsBaSlicerExample/` Swift 示例 + `CMakePresets.json` 中的 `ios-release` 预设（Xcode generator，`CMAKE_OSX_SYSROOT=iphoneos`，部署目标 26.5）。

## 4. 约定与约束

- **C++ 标准**：C++20 强制（`CMAKE_CXX_STANDARD 20`），C 标准按编译器能力在 C11/C23 间选择。
- **位置无关代码**：全局 `CMAKE_POSITION_INDEPENDENT_CODE ON`，所有静态库必须可被共享库链接。
- **调试后缀**：`CMAKE_DEBUG_POSTFIX "d"`，所有 Debug 产物带 `d` 后缀。
- **平台宏**：`HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE` / `HSBA_IS_GAME_CONSOLE` 由顶层 CMake 自动推导，子模块据此开关功能。
- **Copyleft 许可证切换**：启用 `copyleft` vcpkg feature 或检测到 CGAL/OCCT 安装时，项目许可证从 MIT 切换为 GPL-3.0-or-later（由 `Generator_Version.ps1` 和顶层 CMake 共同体现）。
- **REST API 双重门控**：`HSBA_BUILD_REST_API=ON` 且 vcpkg manifest feature `rest-api` 已启用（通过 `VCPKG_MANIFEST_FEATURES` 环境变量或 CMake 变量检测），否则 `FATAL_ERROR`。
- **测试开关**：`HSBA_SLICER_USE_TESTS` 默认 ON，Android/iOS/GameConsole 下强制 OFF；`HSBA_SLICER_RUN_TESTS_AFTER_BUILD` 默认 ON，交叉编译时强制 OFF。
- **输出布局**：所有可执行文件、共享库、资源统一输出到 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`。
- **安装命名空间**：对外导出目标统一使用 `HsBaSlicer::` 命名空间，供下游 CMake 项目通过 `find_package(HsBaSlicer)` 引用。
- **版本信息**：版本号 `1.0.0` 硬编码于 `Generator_Version.ps1`，非 Git tag 派生。
- **覆盖率门槛**：核心模块 45%、其他模块 25% 为硬性门禁（`cmake/coverage.cmake` 中 `--fail-under-line`），CI 通过 `test-coverage.yml` 强制执行。
- **Docker 镜像构建**：`VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 是镜像构建的不变量，确保 copyleft 依赖不会意外进入 MIT 分发镜像。