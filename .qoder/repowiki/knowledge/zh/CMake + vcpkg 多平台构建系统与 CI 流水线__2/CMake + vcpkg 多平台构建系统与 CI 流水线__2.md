---
kind: build_system
name: CMake + vcpkg 多平台构建系统与 CI 流水线
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - cmake/coverage.cmake
    - cmake/HsBaWindowsUtf8.cmake
    - cmake/deploy_dlls.cmake
    - version/Generator_Version.ps1
    - .github/workflows/build-linux.yml
    - .github/workflows/test-coverage.yml
---

## 1. 构建系统概览

HsBaSlicer 采用 **CMake 3.28+** 作为顶层构建系统，配合 **vcpkg** 管理第三方依赖，通过 `CMakePresets.json` 提供跨平台（Windows / Linux / macOS / Android / iOS）的标准化配置。所有可执行产物与共享库统一输出到 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`，并通过 `GNUInstallDirs` + `configure_package_config_file` 导出 `HsBaSlicerTargets.cmake` 供下游 CMake 项目使用。

- 语言标准：C++20（`CMAKE_CXX_STANDARD=20`），C 根据编译器版本在 C11/C23 之间选择；MSVC 强制 `/utf-8` 编码。
- 全局启用 `-fPIC`（`CMAKE_POSITION_INDEPENDENT_CODE ON`），因为静态库最终链接为共享库 `DllHsBaSlicer`。
- Debug 后缀：`CMAKE_DEBUG_POSTFIX="d"`。
- 可选特性通过 `option()` 门控：`HSBA_BUILD_REST_API`、`HSBA_SLICER_MODULE`、`HSBA_COVERAGE`、`HSBA_SLICER_USE_TESTS`、`HSBA_SLICER_BUILD_SAMPLES`、`HSBA_SLICER_BUILD_DOCS`、`HSBA_ENABLE_THREAD_POOL_COROUTINE`、`HSBA_HSBA_USE_BIT7Z`、`HSBA_USE_DLL_LOADER`、`HSBA_PROTOBUF_OUT`。

## 2. 关键文件与包

| 角色 | 文件 |
|---|---|
| 顶层工程 | `CMakeLists.txt` |
| 预设/工具链 | `CMakePresets.json` |
| 依赖清单 | `vcpkg.json`（含 `copyleft`、`rest-api` 两个 feature） |
| 覆盖率脚本 | `cmake/coverage.cmake` |
| Windows UTF-8 manifest 辅助 | `cmake/HsBaWindowsUtf8.cmake` |
| PDB 部署脚本 | `cmake/deploy_dlls.cmake` |
| 版本生成器 | `version/Generator_Version.ps1` |
| CI 工作流 | `.github/workflows/build-{linux,windows,macos,android,ios}.yml`、`.github/workflows/test-coverage.yml` |

## 3. 架构与约定

### 3.1 依赖管理与特性开关

- 依赖集中在 `vcpkg.json`，通过 `default-features` 默认启用 `copyleft`（CGAL/OpenCascade/libigl[cgal]），`rest-api`（cpp-httplib）需显式开启。
- `HSBA_COPL` 为三态缓存变量（AUTO/ON/OFF）：AUTO 时探测 `VCPKG_MANIFEST_FEATURES`、`VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 环境变量或 CGAL/OpenCASCADE 是否已安装；ON/OFF 强制覆盖。启用后定义 `HSBA_COPL` 并切换项目许可证为 GPL-3.0-or-later。
- `HSBA_BUILD_REST_API` 仅在桌面平台生效，且会检查 `VCPKG_MANIFEST_FEATURES` 中是否包含 `rest-api`，否则 `message(FATAL_ERROR ...)` 终止配置。
- 平台宏：`HSBA_DESKTOP`、`HSBA_MOBILE`、`HSBA_GAME_CONSOLE`（由 `VCPKG_TARGET_TRIPLET` 匹配 xbox/switch/playstation/stadia 触发），以及各内核宏 `USE_IGL`、`USE_CGAL`、`USE_OCCT`、`USE_OPENVDB`、`HAS_OPENCV`、`HSBA_HAS_CXX_MODULES`、`HSBA_HAS_NTTP_FP`、`HSBA_HAS_C_GENERIC`、`HSBA_HAS_CXX_COROUTINES`、`HSBA_HAS_C23_CONSTEXPR`、`HSBA_HAS_CXX_EXPLICIT_THIS`。

### 3.2 子目录组织与构建顺序

顶层 `add_subdirectory` 按依赖顺序引入：`third_parts_without_vcpkg → proto → base → cipher → utils → logger(条件) → 2D → paths → fileoperator → meshmodel → cadmodel(条件) → pointcloud(条件) → preprocess → support → pipelinetypes → convert → LibHsBaSlicer → ModuleHsBaSlicer(条件) → DllHsBaSlicer → HsBaSlicer → version`，最后可选地加入 `restapi`、`samples`、`tests`、`static_tests`、`docs`。

### 3.3 安装与导出

- 导出目标集 `_HSBA_INSTALL_TARGETS` 包含所有内部静态库，`LibHsBaSlicer`、`ModuleHsBaSlicer`、`HsBaSlicerRestServer` 单独 install。
- 公共头文件按模块目录结构安装到 `include/HsBaSlicer/<module>/`。
- 生成 `HsBaSlicerConfig.cmake` + `HsBaSlicerConfigVersion.cmake`（`SameMajorVersion` 兼容性），命名空间 `HsBaSlicer::`。

### 3.4 版本信息生成

`version/Generator_Version.ps1` 在 CMake 配置阶段被调用，解析 `vcpkg.json` 中的依赖列表与平台条件，结合 `BuildType`、`Platform`、`VcpkgTriplet`、`Copyleft` 参数生成 `version.cpp`，其中自动推导项目许可证（含 GPL/LGPL 依赖则取 `GPL-3.0-or-later`，否则 `MIT`）。

### 3.5 测试与覆盖率

- 单元测试基于 Boost.Test，通过 `enable_testing()` + `add_subdirectory("tests")` 注册，`ctest` 运行。
- 交叉编译时禁用自动运行测试（`HSBA_SLICER_RUN_TESTS_AFTER_BUILD=OFF`）。
- 覆盖率通过 `HSBA_COVERAGE=ON` 或 preset `linux-coverage` 启用，对所有目标注入 `--coverage`，使用 `gcovr` 分两档阈值门禁：**核心模块**（base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer）≥ 45%，**其他模块** ≥ 25%。
- 覆盖率报告写入 `${CMAKE_BINARY_DIR}/coverage/`，CI 将 HTML/XML 作为 artifact 上传并在 job summary 中打印摘要。

### 3.6 CI 流水线

- `build-linux.yml`：Ubuntu runner，安装大量 X11/GL 依赖，克隆 vcpkg 并 bootstrap，设置 `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite` 启用 GHA 二进制缓存，Ninja + Release 构建，`ctest --parallel 2`。
- `test-coverage.yml`：PR 触发（master/dev），限制 `CMAKE_BUILD_PARALLEL_LEVEL=2` 和 `VCPKG_MAX_CONCURRENCY=2` 防止 CAD 模块 OOM，依次执行 configure/build/test/coverage-gates。
- 各平台独立 workflow（windows/macos/android/ios）复用相同 vcpkg + CMake 模式。

### 3.7 预置构建环境

`CMakePresets.json` 定义了以下 preset：

- `windows-base` → `x64-debug` / `x64-release`
- `linux-debug` / `linux-release` / `linux-coverage`
- `android-release` (arm64-android, android-28)
- `macos-debug` / `macos-release`
- `ios-release` (Xcode generator, arm64, deployment target 26.5)

所有 preset 均指向 `out/build/${presetName}` / `out/install/${presetName}`，并使用 `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` 作为 toolchain 文件。Linux/macOS preset 还包含 VS Remote Settings vendor 扩展以支持远程开发。

## 4. 约定与约束

- **编译器能力检测**：`static_check/feature_check.cmake` 在配置早期检查 concepts/ranges/source_location/template-template NTTP 等特性，缺失则 `message(FATAL_ERROR ...)` 终止构建。
- **REST API 必须搭配 vcpkg feature**：仅设 `HSBA_BUILD_REST_API=ON` 不够，还必须通过 `VCPKG_MANIFEST_FEATURES=rest-api` 安装 cpp-httplib，否则配置失败。
- **Copyleft 内核三态**：`HSBA_COPL=AUTO` 是默认值，自动探测 vcpkg manifest 行为或已安装包；启用后项目许可证从 MIT 切换到 GPL-3.0-or-later。
- **Android/iOS/GameConsole 下禁用**：tests、DLL loader、bit7z、OpenCV、OpenVDB、Sqlpp11 MySQL/PostgreSQL、CAD kernel、PointCloud、logger 等。
- **Debug 构建禁用布尔运算**：定义 `DISABLE_BOOLEAN_OPERATIONS`，避免 CGAL/IGL 在 Debug 下的高内存占用。
- **vcpkg 二进制缓存**：CI 通过 `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite` 与 `actions/cache@v5` 缓存 `vcpkg/downloads`，key 基于 `vcpkg.json` 哈希。
- **覆盖率阈值不可随意下调**：`cmake/coverage.cmake` 注释明确说明阈值是当前测试套件的反回归基线（core ~53%, other ~29%），提高阈值需伴随新测试。
- **输出布局统一**：所有平台可执行文件和库都输出到 `bin/$<CONFIG>`，资源文件（`logcfg.ini`、`restapi.ini`）在配置阶段复制到该目录。
- **Windows UTF-8 代码页**：通过 `cmake/HsBaWindowsUtf8.cmake` 在每个可执行目标上嵌入 activeCodePage UTF-8 manifest，使窄字符路径被当作 UTF-8 处理。