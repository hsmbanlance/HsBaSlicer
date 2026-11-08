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
    - cmake/coverage.cmake
    - version/CMakeLists.txt
    - version/Generator_Version.ps1
    - .github/workflows/build-linux.yml
    - .github/workflows/build-windows.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/cmake-multi-platform.yml
    - cmake/HsBaWindowsUtf8.cmake
    - cmake/deploy_dlls.cmake
    - android/app/build.gradle
---

## 1. 构建系统与工具链

- **CMake 3.28+** 作为唯一顶层构建系统，`cmake_minimum_required(VERSION 3.28)`；所有子模块通过 `add_subdirectory()` 在根 `CMakeLists.txt` 中按依赖顺序注册（proto → base → utils → logger → 2D → paths → fileoperator → meshmodel → cadmodel(pointcloud) → preprocess → support → pipelinetypes → convert → LibHsBaSlicer → ModuleHsBaSlicer → DllHsBaSlicer → HsBaSlicer → version）。
- **vcpkg** 管理第三方依赖（`vcpkg.json`），并通过 CMake Preset 注入 toolchain：`toolchainFile: $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`。默认启用 `copyleft` feature（CGAL + OpenCASCADE + libigl[cgal]），使项目许可证从 MIT 切换为 GPL-3.0-or-later。
- **语言标准**：C++20 强制 (`CMAKE_CXX_STANDARD=20`)；C 标准根据编译器版本动态选择 C11/C23（GCC≥13、Clang≥16、MSVC≥19.36 用 C23，否则回退到 C11）。
- **静态检查**：`static_check/feature_check.cmake` 在配置阶段编译测试源码验证 concepts/ranges/source_location/NTTP-class/template-template matching/_Generic 等特性缺失即 `FATAL_ERROR`。

## 2. 关键文件

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | 顶层工程、全局选项、依赖查找、install/export |
| `CMakePresets.json` | Windows/Linux/macOS/iOS/Android 预置配置，含 coverage preset |
| `vcpkg.json` | 依赖清单与 `copyleft` / `rest-api` 两个可选 feature |
| `cmake/coverage.cmake` | gcov/gcovr 覆盖率门控（核心模块 ≥45%，其他 ≥25%） |
| `version/CMakeLists.txt` + `Generator_Version.ps1` | 构建时生成 `version.cpp`（含 build type、triplet、时间戳） |
| `.github/workflows/*.yml` | GitHub Actions 多平台构建与覆盖率门禁 |
| `android/app/build.gradle` | Android 示例应用（仅打包已编译的 `jniLibs/arm64-v8a`） |
| `cmake/HsBaWindowsUtf8.cmake` | Windows UTF-8 active code page manifest 注入 |
| `cmake/deploy_dlls.cmake` | DLL 部署辅助脚本 |

## 3. 架构与约定

### 3.1 平台分支策略
根 CMake 通过 `HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE` 三个布尔开关统一分支：
- `WIN32/LINUX/(APPLE AND NOT IOS)` → `HSBA_DESKTOP=ON`
- `ANDROID/IOS` → `HSBA_MOBILE=ON`
- `VCPKG_TARGET_TRIPLET` 匹配 `xbox/switch/playstation/stadia` → `HSBA_GAME_CONSOLE=ON`，并定义 `HSBA_IS_GAME_CONSOLE` 与各平台宏。
各功能库据此条件编译：OpenCV/OpenVDB/Sqlpp11(MySQL+PostgreSQL)/CGAL/OCCT/bit7z/dynamic loader/logger 等在 mobile/game-console 上被禁用或降级。

### 3.2 共享库 vs 静态库
由 `BUILD_SHARED_LIBS` 控制，但值被 vcpkg triplet 覆盖：
- `windows` / `dynamic` triplet → 动态库
- `linux` / `android` / `static` triplet → 静态库
- 其余 → 静态库
同时设置 `CMAKE_POSITION_INDEPENDENT_CODE=ON`，保证所有静态库可链接进共享库。

### 3.3 输出目录统一
```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
```
所有产物（exe、dll、so、dylib）统一落入 `out/build/<preset>/bin/<Config>`。

### 3.4 可选功能开关
- `HSBA_COPL`（AUTO/ON/OFF）：是否启用 CGAL/OpenCascade 等 copyleft 内核，AUTO 模式探测 `VCPKG_MANIFEST_FEATURES` 环境变量及已安装包。
- `HSBA_BUILD_REST_API`：仅在桌面平台且 `VCPKG_MANIFEST_FEATURES` 包含 `rest-api`（安装 cpp-httplib）时启用，否则 `FATAL_ERROR`。
- `HSBA_SLICER_MODULE`：C++20 module wrapper（`ModuleHsBaSlicer`），需编译器支持 modules。
- `HSBA_POLYGON_DUMP`：SVG 导出调试工具，桌面默认 ON。
- `HSBA_USE_DLL_LOADER`：桌面默认 ON，非桌面强制 OFF 并定义 `HSBA_NO_DLL_LOADER`。
- `HSBA_HSBA_USE_BIT7Z`：桌面默认 ON，非桌面强制 OFF。
- `HSBA_ENABLE_THREAD_POOL_COROUTINE`：线程池协程支持。

### 3.5 依赖发现策略
混合使用三种方式：
- `find_package(... CONFIG REQUIRED)`：Boost、Protobuf、RapidJSON、Eigen3、miniz、bit7z、OpenSSL、libigl、OpenVDB、sqlpp11、tinyxml2、yaml-cpp、magic_enum、openssl、opencv。
- `pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)`：通过 pkg-config 发现 Clipper2。
- `find_package(PkgConfig REQUIRED)` 后走 pkg-config。

### 3.6 Protobuf 代码生成
`proto/` 目录下 `.proto` 文件经 protobuf 编译器生成 C++/Python/Java/C#/PHP 等多语言绑定，通过 `HSBA_PROTOBUF_OUT` 选项控制是否安装到 `${CMAKE_INSTALL_DATADIR}/HsBaSlicer/proto/<lang>/`。

### 3.7 安装与导出
- 使用 `GNUInstallDirs` + `CMakePackageConfigHelpers` 生成 `HsBaSlicerTargets.cmake`、`HsBaSlicerConfig.cmake`、`HsBaSlicerConfigVersion.cmake`，命名空间 `HsBaSlicer::`，兼容策略 `SameMajorVersion`。
- 公开头文件按语义分组安装至 `include/HsBaSlicer/{DllHsBaSlicer,LibHsBaSlicer,base,fileoperator,meshmodel,support,paths,convert,proto}`。
- 配置文件 `utils/logcfg.ini` 和 `restapi/restapi.ini` 在安装时复制到 `bin/<Config>` 以及 `/etc/HsBaSlicer/`。

### 3.8 测试体系
- 运行时单元测试：`tests/`，基于 Boost.Test，通过 `enable_testing()` + `add_subdirectory("tests")` 注册，`ctest --parallel 2` 执行。
- 静态断言测试：`static_tests/`，独立 target。
- 交叉编译时自动关闭 `HSBA_SLICER_RUN_TESTS_AFTER_BUILD`。
- Android/iOS/Game Console 下 `HSBA_SLICER_USE_TESTS=OFF`。

### 3.9 覆盖率门禁
`cmake/coverage.cmake` 对 GCC/Clang 添加 `--coverage` 编译/链接选项，定义 `coverage` target，使用 gcovr 分两路门控：
- Core 模块（base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer）≥ 45%
- 其他模块 ≥ 25%
阈值不可随意下调，需在 PR 中同步提升。

## 4. CI 流水线（GitHub Actions）

每个平台一个 workflow：`build-linux.yml`、`build-macos.yml`、`build-windows.yml`、`build-android.yml`、`build-ios.yml`，以及聚合入口 `cmake-multi-platform.yml` 和覆盖率门禁 `test-coverage.yml`。

通用流程：
1. `actions/checkout@v6`
2. 安装系统依赖（Linux 上 apt 安装大量 X11/GLFW 相关包）
3. 克隆并 bootstrap vcpkg，设置 `VCPKG_ROOT` 环境变量
4. 启用 GitHub Actions 二进制缓存：`VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`，并对 `vcpkg/downloads` 按 `vcpkg.json` 哈希做 cache key
5. `cmake -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release`
6. `cmake --build build --config Release`
7. `cd build/tests && ctest --build-config Release --output-on-failure --parallel 2`

`test-coverage.yml` 使用 `CMakePresets.json` 中的 `linux-coverage` preset（开启 `HSBA_COVERAGE=ON`，关闭 samples/docs/static_tests/test run），随后运行 `coverage` target 触发 gcovr 阈值门控。

## 5. 约束与规则

- **必须**：编译器支持 C++ concepts、ranges、source_location、class-type NTTP、template-template parameter matching（P0522R0），任一缺失即 `FATAL_ERROR`（见根 `CMakeLists.txt` 第 60–86 行）。
- **必须**：PowerShell（pwsh/powershell）可用，否则 `version/CMakeLists.txt` 报 `FATAL_ERROR`（第 23–26 行）。
- **必须**：启用 `HSBA_BUILD_REST_API=ON` 时必须同时通过 vcpkg manifest feature `rest-api` 安装 cpp-httplib，否则配置阶段 `FATAL_ERROR`（第 198–202 行）。
- **必须**：Copyleft 内核（CGAL/OpenCASCADE）仅在 `HSBA_COPL=AUTO/ON` 且对应 vcpkg feature 启用时链接，否则 `USE_OCCT`/`USE_CGAL` 宏不定义，相关 `cadmodel`/`pointcloud` 子目录不被 add。
- **必须**：Desktop-only 功能（REST API、OpenCV、OpenVDB、Sqlpp11 MySQL/PostgreSQL、bit7z、dynamic loader、logger）在非桌面平台被强制关闭。
- **必须**：所有内部静态库加入 `HsBaSlicerTargets` export set，否则 CMake install 报错（第 531–540 行注释明确说明）。
- **必须**：`CMAKE_POSITION_INDEPENDENT_CODE=ON` 全局开启，因为静态库最终要链接进共享库 `DllHsBaSlicer.so`。
- **必须**：`CMAKE_DEBUG_POSTFIX="d"`，Debug 产物带后缀区分。
- **必须**：覆盖率阈值（core 45% / other 25%）是反回归基线，不得在无 review 情况下降低（见 `cmake/coverage.cmake` 第 27–31 行注释）。
- **必须**：Android ABI 固定为 `arm64-v8a`（`android/app/build.gradle` 第 32 行）。
- **必须**：iOS preset 使用 Xcode generator，部署目标 `iphoneos`，最低版本 `26.5`（`CMakePresets.json` 第 257–292 行）。
- **必须**：Windows 可执行文件嵌入 UTF-8 activeCodePage manifest，使窄字符路径以 UTF-8 解析（`cmake/HsBaWindowsUtf8.cmake` + 根 CMake 注释第 449–464 行）。