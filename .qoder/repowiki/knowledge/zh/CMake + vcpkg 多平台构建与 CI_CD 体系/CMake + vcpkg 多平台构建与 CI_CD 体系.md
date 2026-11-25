---
kind: build_system
name: CMake + vcpkg 多平台构建与 CI/CD 体系
category: build_system
scope:
    - '**'
source_files:
    - CMakeLists.txt
    - CMakePresets.json
    - vcpkg.json
    - vcpkg-configuration.json
    - cmake/coverage.cmake
    - cmake/HsBaWindowsUtf8.cmake
    - cmake/HsBaSlicerConfig.cmake.in
    - docker/Dockerfile
    - .github/workflows/build-linux.yml
    - .github/workflows/build-windows.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/docker-rest-api.yml
    - version/Generator_Version.ps1
    - version/version.cpp.in
    - overlay-ports/opencascade/portfile.cmake
    - overlay-ports/snappy/portfile.cmake
---

## 1. 构建系统总览

HsBaSlicer 采用 **CMake 3.28+** 作为唯一顶层构建系统，配合 **vcpkg (vcpkg-tool)** 管理第三方依赖，通过 `CMakePresets.json` 统一 Windows/Linux/macOS/iOS/Android 的 configure/build/test preset。CI 基于 GitHub Actions，每个平台一个 workflow；Linux 额外提供 Docker 镜像构建。

- C++ 标准：C++20（`CMAKE_CXX_STANDARD=20`），C 标准根据编译器自动选择 C11 或 C23（GCC≥13 / Clang≥16 / MSVC≥19.36）。
- 所有静态库强制 `-fPIC`（`CMAKE_POSITION_INDEPENDENT_CODE ON`），因为最终被共享库 `DllHsBaSlicer.so` 链接。
- 输出目录统一为 `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`，运行时、动态库、资源文件同目录。

## 2. 关键文件与职责

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | 顶层项目配置、特性探测、子目录聚合、install/export |
| `CMakePresets.json` | 预置的 configure/build/test preset（Windows x64 Debug/Release、Linux Debug/Release/Coverage、macOS、iOS Xcode、Android arm64） |
| `vcpkg.json` | 依赖清单 + feature 开关（`copyleft`、`rest-api`） |
| `vcpkg-configuration.json` | vcpkg registry baseline (`microsoft/vcpkg` git baseline) + overlay-ports/triplets |
| `cmake/coverage.cmake` | gcov/gcovr 覆盖率门控（core ≥45%，other ≥25%） |
| `cmake/HsBaWindowsUtf8.cmake` | Windows UTF-8 active code page manifest 注入 |
| `docker/Dockerfile` | HsBaSlicerRestServer 双阶段镜像（builder → runtime） |
| `.github/workflows/*.yml` | 各平台 CI 流水线 |

## 3. 架构与约定

### 3.1 依赖管理（vcpkg）

- 所有外部依赖通过 `find_package(... CONFIG REQUIRED)` 查找，由 vcpkg 工具链提供。
- `default-features: [copyleft]` 默认启用 CGAL/OpenCASCADE/libigl(cgal)，使工程许可证切换为 GPL-3.0-or-later；可通过 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 关闭。
- `rest-api` feature 引入 `cpp-httplib`（带 openssl），仅在桌面平台可用，且需同时设置 CMake 选项 `HSBA_BUILD_REST_API=ON`。
- Android/iOS 使用精简的 sqlpp11（仅 sqlite3）。
- 自定义 port 覆盖位于 `overlay-ports/`（opencascade、snappy），triplet 覆盖位于 `triplets/`。

### 3.2 平台分支策略

顶层 CMake 通过 `HSBA_DESKTOP` / `HSBA_MOBILE` / `HSBA_GAME_CONSOLE` 三态宏控制功能：

- `HSBA_DESKTOP`：启用 OpenCV、bit7z、Logger、CGAL/OCCT（若 copyleft）、OpenVDB、sqlpp11 MySQL/PostgreSQL。
- `HSBA_MOBILE`（Android/iOS）：禁用 OpenCV、bit7z、DLL loader、Logger、CAD kernel、PointCloud、MySQL/PostgreSQL，仅保留 SQLite。
- `HSBA_GAME_CONSOLE`：由 `VCPKG_TARGET_TRIPLET` 匹配 xbox/switch/playstation/stadia 触发，关闭 Logger 等。

### 3.3 可选模块门控

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `HSBA_SLICER_MODULE` | ON | 编译 C++20 module wrapper（ModuleHsBaSlicer），受编译器模块支持检测限制 |
| `HSBA_COVERAGE` | OFF | 启用 gcov 插桩 + coverage 目标 |
| `HSBA_BUILD_REST_API` | OFF | 构建 HsBaSlicerRestServer 可执行文件（桌面独占） |
| `HSBA_SLICER_USE_TESTS` | ON | 启用 tests/ 单元测试 |
| `HSBA_SLICER_RUN_TESTS_AFTER_BUILD` | ON | 构建后自动运行 ctest（交叉编译时自动关闭） |
| `HSBA_SLICER_BUILD_SAMPLES` | ON | 构建 samples/FDM..WAAM 示例 |
| `HSBA_SLICER_BUILD_DOCS` | ON | 需要 Doxygen |
| `HSBA_PROTOBUF_OUT` | ON | 生成并安装 protobuf 多语言导出 |

### 3.4 构建产物与安装

- 公共头文件按目录结构安装到 `${CMAKE_INSTALL_INCLUDEDIR}/HsBaSlicer/{base,2D,meshmodel,support,paths,convert,proto,logger,LibHsBaSlicer,fileoperator}`。
- 通过 `install(EXPORT HsBaSlicerTargets ... NAMESPACE HsBaSlicer::)` 导出 CMake target，并生成 `HsBaSlicerConfig.cmake` + `HsBaSlicerConfigVersion.cmake`（`SameMajorVersion` 兼容性）。
- 二进制输出：`CMAKE_RUNTIME_OUTPUT_DIRECTORY` / `CMAKE_LIBRARY_OUTPUT_DIRECTORY` 统一指向 `bin/$<CONFIG>`。
- Debug 后缀：`CMAKE_DEBUG_POSTFIX="d"`。

### 3.5 版本生成

- 版本号来自 `version/Generator_Version.ps1`（PowerShell 脚本），在 Linux CI 上通过 `apt install powershell` 安装。
- 生成的 `version/version.hpp` 随源码一起提交（commit hash 注释于顶层 CMakeLists.txt 第 9 行）。

### 3.6 覆盖率门控

`cmake/coverage.cmake` 定义两条基线：

- Core modules（`base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer`）≥ 45%
- Other modules ≥ 25%

通过 `linux-coverage` preset 和 `test-coverage.yml` CI 门禁强制执行；阈值不可随意降低（见 cmake/coverage.cmake 注释）。gcovr 并行度通过 `HSBA_COVERAGE_JOBS` 控制，CI 中设为 2 以避免 OOM。

## 4. CI 流水线（GitHub Actions）

| Workflow | 触发 | 行为 |
|---|---|---|
| `build-linux.yml` | `workflow_dispatch` | Ubuntu latest，Ninja + vcpkg，Release 构建 + ctest |
| `build-windows.yml` | `workflow_dispatch` | windows-latest，VS2022 + vcpkg，x64 Release |
| `build-macos.yml` | `workflow_dispatch` | macOS runner，Homebrew 安装 CMake/Ninja/PowerShell |
| `build-android.yml` | `workflow_dispatch` | Android NDK + vcpkg triplet `arm64-android` |
| `build-ios.yml` | `workflow_dispatch` | iOS Xcode build preset |
| `test-coverage.yml` | PR to master/dev + dispatch | Linux gcov-instrumented build，运行测试，执行覆盖率门控，上传 HTML/XML 报告 |
| `docker-rest-api.yml` | PR to master/dev | 构建 docker/Dockerfile，验证 HsBaSlicerRestServer 镜像 |
| `rest-webui.yml` | PR to master/dev | 构建 webui/ Node.js 前端 |
| `publish-wiki.yml` | push to master | 发布 Wiki |

通用模式：

1. 克隆仓库 → 克隆 vcpkg → bootstrap → 设置 `VCPKG_ROOT`。
2. 启用 GitHub Actions binary cache（`VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`）。
3. 缓存 `vcpkg/downloads`（key 含 `vcpkg.json` 哈希）。
4. CMake configure 最多重试 5 次（每次失败删除 build 目录并等待 30s），以应对 vcpkg 对网络错误的非瞬态判定。
5. 构建 → 测试（ctest）。

## 5. Docker 镜像

`docker/Dockerfile` 是双阶段构建：

- **Stage 1 (builder)**：Ubuntu + 完整系统依赖（与 `build-linux.yml` 对齐），vcpkg bootstrap，预热 cmake/ninja 二进制，configure 使用 `--preset linux-release` + `HSBA_BUILD_REST_API=ON` + `VCPKG_MANIFEST_FEATURES=${HSBA_FEATURES}`，构建并 `cmake --install` 到 `/opt/hsba-install`。
- **Stage 2 (runtime)**：仅安装运行期依赖（libssl3t64、libmysqlclient24、sqlite3 等），复制安装产物，修改 `restapi.ini` host 为 `0.0.0.0`，暴露 8080 端口，HEALTHCHECK 调用 `/api/health`。

环境变量：`HSBA_REST_HOST`、`HSBA_REST_PORT`、`HSBA_REST_AUTH_TOKEN`、`HSBA_REST_TLS_CERT`、`HSBA_REST_TLS_KEY`。

## 6. 约束与规则（来源自代码/文档）

- **C++ 标准**：`CMAKE_CXX_STANDARD=20` 且 `CMAKE_CXX_STANDARD_REQUIRED ON`（顶层 CMakeLists.txt 第 18-37 行）。
- **必需编译器特性**：C++ concepts、ranges、source_location、class-type NTTP、template-template matching（P0522R0）缺失即 `FATAL_ERROR`（static_check/feature_check.cmake 探测 + 顶层 FATAL_ERROR）。
- **REST API 必须显式开启**：`HSBA_BUILD_REST_API=ON` 且 vcpkg feature `rest-api` 已安装，否则 `FATAL_ERROR`（顶层 CMakeLists.txt 第 188-214 行）。
- **Copyleft 内核默认开启但可关闭**：`HSBA_COPL=AUTO` 三态，AUTO 时探测 CGAL/OpenCASCADE 是否安装；开启后工程许可证变为 GPL-3.0-or-later（vcpkg.json features.copyleft 声明 license）。
- **移动端禁用测试**：Android/iOS/game console 下 `HSBA_SLICER_USE_TESTS=OFF`（顶层 CMakeLists.txt 第 449-452 行）。
- **交叉编译不运行测试**：`CMAKE_CROSSCOMPILING` 时 `HSBA_SLICER_RUN_TESTS_AFTER_BUILD=OFF`（第 456-459 行）。
- **覆盖率门控不可低于基线**：core ≥45%、other ≥25%（cmake/coverage.cmake 注释明确“do not lower them without review”）。
- **Windows UTF-8 路径**：通过嵌入 `<activeCodePage>UTF-8</activeCodePage>` manifest 让 `GetACP()` 返回 65001，窄字符路径按 UTF-8 处理（顶层 CMakeLists.txt 第 462-476 行）。
- **vcpkg 基线锁定**：`vcpkg-configuration.json` 固定 `baseline: 93ad765c...`，Dockerfile 支持通过 `VCPKG_BASELINE` ARG 覆盖（第 114-117 行）。
- **构建产物 RPATH**：Dockerfile 注释指出 CMake install 已修正 RPATH，运行时无需额外 LD_LIBRARY_PATH 配置（除容器内 `/usr/local/lib`）。

## 7. 关键文件清单

- `CMakeLists.txt`
- `CMakePresets.json`
- `vcpkg.json`
- `vcpkg-configuration.json`
- `cmake/coverage.cmake`
- `cmake/HsBaWindowsUtf8.cmake`
- `cmake/deploy_dlls.cmake`
- `cmake/HsBaSlicerConfig.cmake.in`
- `docker/Dockerfile`
- `docker/Dockerfile.dockerignore`
- `.github/workflows/build-linux.yml`
- `.github/workflows/build-windows.yml`
- `.github/workflows/build-macos.yml`
- `.github/workflows/build-android.yml`
- `.github/workflows/build-ios.yml`
- `.github/workflows/test-coverage.yml`
- `.github/workflows/docker-rest-api.yml`
- `.github/workflows/rest-webui.yml`
- `.github/workflows/publish-wiki.yml`
- `version/Generator_Version.ps1`
- `version/version.cpp.in`
- `overlay-ports/opencascade/portfile.cmake`
- `overlay-ports/snappy/portfile.cmake`
- `android/app/build.gradle`
- `webui/package.json`
- `third_parts_without_vcpkg/`（非 vcpkg 依赖目录）