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
    - .github/workflows/build-windows.yml
    - .github/workflows/build-linux.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/build-android.yml
    - .github/workflows/test-coverage.yml
    - cmake/coverage.cmake
    - version/CMakeLists.txt
    - version/Generator_Version.ps1
    - cmake/HsBaWindowsUtf8.cmake
    - cmake/HsBaSlicerConfig.cmake.in
    - android/app/build.gradle
---

## 1. 构建系统总览

HsBaSlicer 以 **CMake ≥ 3.28** 为唯一顶层构建入口，统一聚合核心库、C ABI 动态库、C++20 module、示例与测试；依赖通过 **vcpkg**（`vcpkg.json` + `vcpkg-configuration.json`）管理，跨平台交叉编译由 CMake Presets (`CMakePresets.json`) 驱动，CI 基于 GitHub Actions 按平台分工作流。

语言标准：C++20 强制开启，C 标准根据编译器版本在 C11/C23 之间自动选择（GCC ≥ 13 / Clang ≥ 16 / MSVC ≥ 19.36 时启用 C23），并通过 `static_check/feature_check.cmake` 在配置阶段检测 concepts、ranges、source_location、NTTP class/FP、template-template matching、coroutines、explicit this 等特性，缺失则 `message(FATAL_ERROR)` 终止。

## 2. 关键文件与职责

| 文件 | 职责 |
|---|---|
| `CMakeLists.txt` | 顶层项目配置：特性探测、依赖 find_package、子目录装配、install/export、ctest 注册 |
| `CMakePresets.json` | 预置 Windows/Linux/macOS/iOS/Android 的 configure/build/test preset，含 Android arm64 与 iOS Xcode preset |
| `.github/workflows/*.yml` | 各平台构建、iOS/Android 构建、覆盖率门禁、Wiki 发布 |
| `cmake/coverage.cmake` | gcov/gcovr 覆盖率门控（core ≥ 45%，other ≥ 25%） |
| `version/CMakeLists.txt` + `Generator_Version.ps1` | 配置阶段调用 PowerShell 解析 `vcpkg.json` 生成 `version.cpp`（含 build type、triplet、许可证、第三方清单） |
| `cmake/HsBaWindowsUtf8.cmake` | Windows 可执行嵌入 UTF-8 activeCodePage manifest |
| `cmake/deploy_dlls.cmake` | DLL 部署辅助脚本 |
| `cmake/HsBaSlicerConfig.cmake.in` | 安装后供下游 `find_package(HsBaSlicer)` 使用的 Config 模板 |
| `vcpkg.json` | 依赖声明与 copyleft feature（CGAL/OpenCASCADE） |
| `android/app/build.gradle` | Android Gradle 应用层构建（JNI 加载原生库） |

## 3. 架构与约定

### 3.1 依赖与特性裁剪
- 所有第三方库通过 `find_package(... CONFIG REQUIRED)` 或 `pkg_check_modules(Clipper2 ...)` 引入，全部经 vcpkg 提供。
- 可选内核通过三态缓存变量 `HSBA_COPL`（AUTO/ON/OFF）控制：AUTO 下优先检查环境变量 `VCPKG_MANIFEST_FEATURES` 是否包含 `copyleft`，否则探测 CGAL/OpenCASCADE 是否已安装；启用后定义 `HSBA_COPL`、`USE_CGAL`、`USE_OCCT`、`USE_CAD_KERNEL`，并将项目许可证切换为 GPL-3.0-or-later。
- 平台相关开关：
  - `HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE`（由 `VCPKG_TARGET_TRIPLET` 推断）
  - `HSBA_USE_DLL_LOADER`（仅桌面默认 ON，移动端关闭并定义 `HSBA_NO_DLL_LOADER`）
  - `HSBA_HSBA_USE_BIT7Z`（非桌面默认 OFF）
  - `HSBA_SLICER_MODULE`（C++20 modules，需工具链支持）
  - `HSBA_POLYGON_DUMP`（桌面默认 ON，导出 SVG 调试能力）
  - `HSBA_ENABLE_THREAD_POOL_COROUTINE`（线程池协程支持）
- Debug 构建自动定义 `DISABLE_BOOLEAN_OPERATIONS`，避免 CGAL/IGL 在 Debug 下的内存爆炸。

### 3.2 静态/共享库策略
- 全局 `CMAKE_POSITION_INDEPENDENT_CODE = ON`，确保所有静态库能以 PIC 链接进共享库。
- `BUILD_SHARED_LIBS` 由 `VCPKG_TARGET_TRIPLET` 决定：匹配 `windows`/`xbox`/`dynamic` 时 ON，匹配 `linux`/`android`/`static` 时 OFF；未设置时默认 OFF。
- 输出目录统一收敛到 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`（运行时与库）。Debug 产物追加 `d` 后缀（`CMAKE_DEBUG_POSTFIX`）。

### 3.3 目标组织与安装
- 顶层 `add_subdirectory` 顺序即依赖顺序：proto → base → cipher → utils → logger → 2D → paths → fileoperator → meshmodel → cadmodel(条件) → pointcloud(条件) → preprocess → support → pipelinetypes → convert → LibHsBaSlicer → ModuleHsBaSlicer(条件) → DllHsBaSlicer → HsBaSlicer → version。
- 通过 `install(EXPORT HsBaSlicerTargets NAMESPACE HsBaSlicer::)` 导出 CMake targets，配合 `configure_package_config_file` 生成 `HsBaSlicerConfig.cmake` 与 `SameMajorVersion` 兼容的版本文件，供下游 `find_package(HsBaSlicer)` 消费。
- 公共头文件按模块目录结构安装至 `include/HsBaSlicer/<module>/`。

### 3.4 测试体系
- 运行期测试：Boost.Test，通过顶层 `enable_testing()` + `add_subdirectory(tests)` 注册，`ctest` 执行。`HSBA_SLICER_RUN_TESTS_AFTER_BUILD=OFF` 在交叉编译时禁用自动运行。
- 编译期静态测试：`static_tests/`，使用 constexpr/static_assert 验证模板/反射/Lua 绑定，独立 target。
- 覆盖率门禁：`HSBA_COVERAGE=ON` 时对所有目标注入 `--coverage`，`coverage` 自定义目标用 gcovr 分别计算 core 与 other 两组行覆盖率，低于阈值（core 45%、other 25%）直接失败。

### 3.5 版本信息生成
- `version/CMakeLists.txt` 在配置阶段调用 `Generator_Version.ps1`，读取 `vcpkg.json` 中依赖列表、`VCPKG_TARGET_TRIPLET`、`HSBA_COPL`、构建时间，生成 `version.cpp`，编译为静态库 `HsBaVersion` 暴露给上层查询。

### 3.6 文档与静态检查
- Doxygen：`docs/CMakeLists.txt` + `Doxyfile`，通过 `HSBA_SLICER_BUILD_DOCS` 选项控制，找不到 Doxygen 时自动关闭。
- 静态检查：`static_check/clang-format-batch.py`、`cpp_analyzer.py`、`feature_check.cmake`。

## 4. CI 流水线（GitHub Actions）

| 工作流 | 触发 | 行为 |
|---|---|---|
| `build-windows.yml` | `workflow_dispatch` | VS x64 + Ninja + vcpkg，Release 构建 |
| `build-linux.yml` | `workflow_dispatch` | Ubuntu + Ninja + vcpkg，Release 构建 + ctest |
| `build-macos.yml` | `workflow_dispatch` | macOS 构建 |
| `build-ios.yml` | `workflow_dispatch` | iOS (Xcode) 构建 |
| `build-android.yml` | `workflow_dispatch` | Android arm64 构建 |
| `test-coverage.yml` | PR to master/dev + `workflow_dispatch` | 使用 `linux-coverage` preset 构建 + 运行测试 + gcovr 门控（core≥45%/other≥25%），上传 HTML/XML 报告 |
| `publish-wiki.yml` | `workflow_dispatch` | 将 docs 同步到 GitHub Wiki |

所有工作流均复用同一模式：clone vcpkg → bootstrap → `vcpkg integrate install` → 设置 `VCPKG_BINARY_SOURCES=x-gha,readwrite` → cache `vcpkg/downloads`（key 含 `vcpkg.json` hash）→ cmake 配置 → 构建。

## 5. 约束与规则

- **C++ 标准**：强制 C++20（`set(CMAKE_CXX_STANDARD 20)` + `REQUIRED`），缺少 concepts/ranges/source_location/template-template matching 的编译器直接 FATAL_ERROR。
- **Copyleft 许可证切换**：启用 `copyleft` vcpkg feature 或 `HSBA_COPL=ON` 后，项目对外许可证从 MIT 切换为 GPL-3.0-or-later（见 `vcpkg.json` features 描述与顶层 `message`）。
- **游戏主机目标**：当 `VCPKG_TARGET_TRIPLET` 匹配 xbox/switch/playstation/stadia 时定义 `HSBA_IS_GAME_CONSOLE` 及具体 triplet 宏，同时关闭 logger、DLL loader、bit7z、OpenCV、CAD/PointCloud/SQL 等子模块。
- **Android/iOS 裁剪**：这两个平台默认关闭 tests、DLL loader、bit7z、OpenCV、CAD、PointCloud、MySQL/PostgreSQL（仅保留 SQLite3），并在 `CMAKE_SYSTEM_NAME` 上显式区分。
- **覆盖率门槛**：`cmake/coverage.cmake` 中硬编码 core ≥ 45%、other ≥ 25%，任何低于阈值的提交都会使 `coverage` 目标失败。
- **PowerShell 依赖**：`version/CMakeLists.txt` 在找不到 `pwsh`/`powershell` 时报 `FATAL_ERROR`，因此 Linux CI 必须安装 PowerShell。
- **输出布局**：所有可执行与库统一输出到 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`，资源 `logcfg.ini` 在配置阶段复制到该目录。
- **Install 命名空间**：对外暴露的 CMake target 统一使用 `HsBaSlicer::` 前缀（如 `HsBaSlicer::LibHsBaSlicer`），由 `NAMESPACE HsBaSlicer::` 保证。