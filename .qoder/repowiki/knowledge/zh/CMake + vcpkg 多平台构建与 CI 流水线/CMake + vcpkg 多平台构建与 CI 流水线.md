---
kind: build_system
name: CMake + vcpkg 多平台构建与 CI 流水线
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - vcpkg-configuration.json
    - cmake/coverage.cmake
    - version/CMakeLists.txt
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
    - static_check/feature_check.cmake
---

## 1. 构建系统概览

HsBaSlicer 采用 **CMake 3.28+** 作为唯一构建入口，依赖管理通过 **vcpkg (manifest mode)** 完成，CI 基于 **GitHub Actions**。项目同时支持桌面（Windows/Linux/macOS）、Android、iOS 以及游戏主机（Xbox/Switch/PS/Stadia）等多目标交叉编译。

- 顶层 `CMakeLists.txt` 声明 `cmake_minimum_required(VERSION 3.28)`、全局 C++20 / C11-C23 标准、`CMAKE_POSITION_INDEPENDENT_CODE=ON`，并通过 `add_subdirectory()` 聚合 base/cipher/utils/logger/2D/paths/fileoperator/meshmodel/cadmodel/pointcloud/preprocess/support/pipelinetypes/convert/LibHsBaSlicer/ModuleHsBaSlicer/DllHsBaSlicer/HsBaSlicer/version/proto/samples/tests/static_tests/docs 等子模块。
- 所有输出统一到 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`（运行时与库目录合并），便于可执行文件直接加载同级 DLL/SO。
- 通过 `GNUInstallDirs` + `install(EXPORT HsBaSlicerTargets ... NAMESPACE HsBaSlicer::)` 导出 CMake 包配置（`HsBaSlicerConfig.cmake` + `HsBaSlicerConfigVersion.cmake`），供下游 `find_package(HsBaSlicer)` 消费。

## 2. 关键文件与角色

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | 顶层工程：编译器特性探测、依赖 find_package/pkg_check_modules、子目录聚合、install/export |
| `CMakePresets.json` | 预置的 configure/build/test preset（windows/linux/macos/android/ios/release/debug/coverage） |
| `vcpkg.json` | vcpkg manifest：依赖列表 + `copyleft` feature（CGAL/OpenCASCADE/libigl+cgal） |
| `vcpkg-configuration.json` | vcpkg 多注册表配置（git baseline + microsoft artifact registry + overlay-ports/triplets） |
| `cmake/coverage.cmake` | gcov/gcovr 覆盖率门禁（core ≥45%，other ≥25%） |
| `version/CMakeLists.txt` + `Generator_Version.ps1` | 在配置阶段调用 PowerShell 生成 `version.cpp`（含构建时间、triplet、copyleft 标志） |
| `.github/workflows/*.yml` | GitHub Actions CI 工作流 |
| `static_check/feature_check.cmake` | 编译期 C++17/20 特性探测宏（concepts/ranges/source_location/NTTP-class/NTTP-fp/template-template-matching/_Generic） |
| `android/app/build.gradle` | Android 壳应用 Gradle 脚本（NDK 集成） |

## 3. 架构与约定

### 3.1 依赖发现策略
- 优先使用 vcpkg 提供的 CONFIG 模式：`find_package(Protobuf|RapidJSON|Eigen3|miniz|tinyxml2|yaml-cpp|magic_enum|libigl|OpenSSL|bit7z) CONFIG REQUIRED`。
- 对 pkg-config 依赖使用 `pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)`。
- 对 Boost 特殊处理：Windows 下 `-DBOOST_LOG_DYN_LINK`，Android/iOS/GameConsole 仅 `find_package(Boost REQUIRED)`（无 log_setup/log）。
- 可选内核通过 `HSBA_COPL`（AUTO/ON/OFF）三态控制 CGAL/OpenCASCADE/libigl+cgal 是否启用；默认 AUTO 会探测环境变量或已安装包。

### 3.2 平台分支宏
- `HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE` 由 `VCPKG_TARGET_TRIPLET` 和 `CMAKE_SYSTEM_NAME` 推导。
- 平台相关开关：`HSBA_USE_DLL_LOADER`（非桌面禁用）、`HSBA_HSBA_USE_BIT7Z`（非桌面禁用）、`HAS_OPENCV`、`USE_CGAL`、`USE_OCCT`、`USE_OPENVDB`、`USE_IGL`、`USE_CAD_KERNEL`、`HSBA_IS_GAME_CONSOLE`、`HSBA_GAME_CONSOLE_<TRIPLET>`。
- Debug 构建自动关闭布尔运算：`DISABLE_BOOLEAN_OPERATIONS`（CGAL/IGL 慢且内存高）。

### 3.3 静态库 vs 动态库
- `BUILD_SHARED_LIBS` 由 `VCPKG_TARGET_TRIPLET` 决定：匹配 `windows|xbox|dynamic` → ON；匹配 `linux|android|static` → OFF；其余默认 OFF。
- 所有内部静态库链接进共享库时强制 `-fPIC`。

### 3.4 版本信息生成
- `version/CMakeLists.txt` 在 configure 阶段调用 `Generator_Version.ps1`，要求安装 PowerShell（pwsh 或 powershell），否则 `message(FATAL_ERROR ...)`。
- 生成的 `version.cpp` 被打包为静态库 `HsBaVersion`，对外暴露 `version.hpp`。

### 3.5 覆盖率门禁
- `cmake/coverage.cmake` 注入 `--coverage` 编译/链接选项，定义 `coverage` 自定义目标。
- 核心模块过滤：`base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer`；排除：`tests/samples/static_tests/static_check/third_parts_without_vcpkg/docs/out/build/vcpkg_installed/vcpkg/usr`。
- 阈值变量：`HSBA_COVERAGE_THRESHOLD_CORE`（默认 45）、`HSBA_COVERAGE_THRESHOLD_OTHER`（默认 25）、`HSBA_COVERAGE_JOBS`（gcovr 并行度）。
- 仅 GCC/Clang 支持；MSVC 下自动关闭并警告。

### 3.6 CMake Preset 矩阵
`CMakePresets.json` 提供以下预设（按 hostSystemName 条件选择）：
- Windows: `x64-debug`, `x64-release`（Ninja, cl.exe）
- Linux: `linux-debug`, `linux-release`, `linux-coverage`（Ninja, 带 HSBA_COVERAGE=ON 及阈值）
- macOS: `macos-debug`, `macos-release`
- iOS: `ios-release`（Xcode, arm64, deployment target 26.5）
- Android: `android-release`（arm64-v8a, android-28, 通过 `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` 链入 NDK toolchain）

所有 preset 统一设置 `binaryDir=out/build/${presetName}`、`installDir=out/install/${presetName}`、`toolchainFile=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`。

### 3.7 测试与文档
- `HSBA_SLICER_USE_TESTS` 控制 Boost.Test 测试（Android/iOS/GameConsole 下强制 OFF）。
- `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` 控制 `ctest --build-and-test`（交叉编译时强制 OFF）。
- `HSBA_SLICER_USE_STATIC_TESTS` 控制编译期静态测试（constexpr/模板断言）。
- `HSBA_SLICER_BUILD_DOCS` 控制 Doxygen 文档生成（未找到 Doxygen 则自动关闭）。
- `HSBA_SLICER_BUILD_SAMPLES` 控制 FDM/SLA/SLS/SLM/LOM/TDP/WAAP/Custom/LibHsBaSlicer 示例构建。

## 4. CI 流水线（GitHub Actions）

`.github/workflows/` 下按平台拆分：
- `build-linux.yml`: Ubuntu runner，apt 安装大量 X11/GL 依赖 + PowerShell，clone vcpkg，`cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=...`，Release 构建后 `cd build/tests && ctest --parallel 2`。
- `build-windows.yml` / `build-macos.yml` / `build-android.yml` / `build-ios.yml`: 各平台对应构建流程。
- `cmake-multi-platform.yml`: 通用多平台矩阵构建。
- `test-coverage.yml`: PR 触发（master/dev），使用 `linux-coverage` preset，限制 `CMAKE_BUILD_PARALLEL_LEVEL=2`、`VCPKG_MAX_CONCURRENCY=2` 防止 OOM；运行 `ctest --preset linux-coverage` 后执行 `cmake --build --preset linux-coverage-report` 触发 gcovr 阈值门禁，结果写入 Job Summary 并上传 `coverage-reports` artifact。
- `publish-wiki.yml`: 文档发布。

所有 Linux CI 均使用 `easimon/maximize-build-space@v10` 清理 Runner 空间，并使用 `actions/cache@v5` 缓存 `vcpkg/downloads`（key 基于 `vcpkg.json` hash）。

## 5. 规则与约束（来自源码/配置的显式约定）

1. **编译器最低要求**: C++ concepts、ranges、source_location、class-type NTTP、template-template matching 必须可用，否则 `message(FATAL_ERROR ...)`（`CMakeLists.txt:60-86`）。
2. **PowerShell 是硬性依赖**: `version/CMakeLists.txt` 找不到 pwsh/powershell 即 `FATAL_ERROR`。
3. **GCovr 是覆盖率门禁必需**: `cmake/coverage.cmake` 找不到 gcovr 时 `coverage` 目标会失败。
4. **覆盖率的“核心”目录集合固定**: `base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer`，不可随意更改而不更新阈值。
5. **Android/iOS/GameConsole 下测试、DLL Loader、Bit7z、OpenCV、CAD/PointCloud 等特性被强制关闭**（通过 `set(... FORCE CACHE BOOL ...)`）。
6. **Debug 构建禁用布尔运算**: `DISABLE_BOOLEAN_OPERATIONS` 宏在 Debug 下无条件添加（`CMakeLists.txt:294-298`）。
7. **vcpkg 依赖锁定**: `vcpkg-configuration.json` 通过 git baseline `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4` 锁定 vcpkg 仓库版本。
8. **Copyleft 许可证切换**: 启用 `copyleft` feature 会将项目许可证从 MIT 切换到 GPL-3.0-or-later（`vcpkg.json` features 中声明）。
9. **输出目录统一**: 所有目标产物进入 `bin/$<CONFIG>`，资源 `logcfg.ini` 在 configure 阶段复制到该目录（`CMakeLists.txt:358-365`）。