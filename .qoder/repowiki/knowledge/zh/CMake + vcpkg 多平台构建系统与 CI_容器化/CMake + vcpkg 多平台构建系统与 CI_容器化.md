---
kind: build_system
name: CMake + vcpkg 多平台构建系统与 CI/容器化
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
    - version/CMakeLists.txt
    - version/Generator_Version.ps1
    - docker/Dockerfile
    - .github/workflows/build-linux.yml
    - .github/workflows/docker-rest-api.yml
    - .github/workflows/build-windows.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-android.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/test-coverage.yml
---

## 1. 构建系统总览

HsBaSlicer 使用 **CMake ≥ 3.28** 作为唯一顶层构建系统，依赖管理通过 **vcpkg**（`vcpkg.json` + `vcpkg-configuration.json`）完成，跨平台配置集中在 `CMakePresets.json`。CI 由 GitHub Actions 驱动，每个目标平台一个 workflow；Linux REST API 镜像通过 `docker/Dockerfile` 多阶段构建。

语言标准：根 CMakeLists.txt 强制 `CMAKE_CXX_STANDARD=20`，在 GCC≥13 / Clang≥16 / MSVC≥19.36 时回退到 `CMAKE_C_STANDARD=23`，否则为 C11；所有静态库统一开启 `-fPIC`（`CMAKE_POSITION_INDEPENDENT_CODE ON`），因为最终链接进共享库 `DllHsBaSlicer.so/.dll`。

## 2. 关键文件与职责

| 文件 | 职责 |
|---|---|
| `CMakeLists.txt` | 全局特性探测、依赖发现、子目录装配、install/export |
| `CMakePresets.json` | Windows/Linux/macOS/iOS/Android 的 configure/build/test preset |
| `vcpkg.json` | 依赖清单 + `copyleft` / `rest-api` manifest features |
| `cmake/coverage.cmake` | gcov/gcovr 覆盖率门控（core 45% / other 25%） |
| `version/CMakeLists.txt` + `Generator_Version.ps1` | 生成 `version.cpp`（含 build type、triplet、configure time） |
| `cmake/HsBaWindowsUtf8.cmake` | Windows UTF-8 active code page manifest 注入 |
| `cmake/deploy_dlls.cmake` | DLL 部署辅助脚本 |
| `docker/Dockerfile` | HsBaSlicerRestServer 镜像（builder+runtime 两阶段） |
| `.github/workflows/*.yml` | 各平台构建、Docker 镜像、覆盖率、Wiki 发布工作流 |

## 3. 架构与约定

### 3.1 依赖与特性开关
- 所有第三方库通过 `find_package(... CONFIG REQUIRED)` 或 `pkg_check_modules(Clipper2 ...)` 引入，无手写 `include_directories` 散落在模块中。
- 可选能力以 `option(... CACHE BOOL ...)` 暴露：`HSBA_BUILD_REST_API`、`HSBA_COPL`（三态 AUTO/ON/OFF）、`HSBA_SLICER_MODULE`、`HSBA_USE_DLL_LOADER`、`HSBA_HSBA_USE_BIT7Z`、`HSBA_ENABLE_THREAD_POOL_COROUTINE`、`HSBA_SLICER_USE_TESTS`、`HSBA_SLICER_BUILD_SAMPLES`、`HSBA_SLICER_BUILD_DOCS`、`HSBA_COVERAGE`、`HSBA_PROTOBUF_OUT`。
- `HSBA_COPL` 控制 CGAL/OpenCascade 是否链接，并切换项目许可证从 MIT 到 GPL-3.0-or-later；默认通过 `vcpkg.json` 的 `default-features: ["copyleft"]` 启用，但 Docker 镜像通过 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 显式关闭，仅由 `HSBA_FEATURES` 参数决定。
- `HSBA_BUILD_REST_API` 必须与 vcpkg feature `rest-api`（安装 cpp-httplib）同时开启，顶层 CMake 会检查 `VCPKG_MANIFEST_FEATURES` 环境变量或 `find_package(httplib)` 结果，否则 `FATAL_ERROR`。

### 3.2 平台矩阵
- Desktop (`WIN32/LINUX/APPLE_NOT_IOS`)：启用 logger、OpenCV、OpenVDB、Sqlpp11(SQLite+MySQL+PostgreSQL)、bit7z、DLL loader。
- Mobile (`ANDROID/IOS`)：禁用 logger、OpenCV、OpenVDB、Sqlpp11(仅 SQLite)、bit7z、DLL loader。
- Game console：由 `VCPKG_TARGET_TRIPLET` 匹配 `xbox|switch|playstation|stadia` 触发 `HSBA_IS_GAME_CONSOLE` / `HSBA_GAME_CONSOLE_*` 宏。
- iOS preset 使用 Xcode generator + `CMAKE_OSX_SYSROOT=iphoneos` + `CMAKE_OSX_DEPLOYMENT_TARGET=26.5`。

### 3.3 输出布局
- 统一输出目录：`CMAKE_RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_BINARY_DIR}/bin/$<CONFIG>`，`CMAKE_LIBRARY_OUTPUT_DIRECTORY` 同目录。
- Debug 产物追加 `d` 后缀（`CMAKE_DEBUG_POSTFIX "d"`）。
- install 遵循 `GNUInstallDirs`：RUNTIME→bin，LIBRARY/ARCHIVE→lib，头文件→`include/HsBaSlicer/<subdir>`。
- 导出 CMake package：`HsBaSlicerTargets.cmake` + `HsBaSlicerConfig.cmake` + `HsBaSlicerConfigVersion.cmake`，namespace `HsBaSlicer::`，兼容性 `SameMajorVersion`。

### 3.4 Protobuf 代码生成
- `proto/` 目录下 `.proto` 文件经 `add_subdirectory(proto)` 生成 C++/Python/Java/C#/PHP 等绑定，输出路径由 `HSBA_PROTOBUF_OUT` 控制，生成的 `.pb.h` 随 install 一起分发。

### 3.5 版本信息
- `version/CMakeLists.txt` 在配置阶段调用 PowerShell `Generator_Version.ps1` 生成 `version.cpp`，注入 `CMAKE_BUILD_TYPE`、`VCPKG_TARGET_TRIPLET`、`CONFIGURE_TIME`、`HSBA_COPL` 等信息；PowerShell 缺失则 `FATAL_ERROR`。

### 3.6 覆盖率门控
- `cmake/coverage.cmake` 对 GNU/Clang 添加 `--coverage` 编译/链接选项，定义 `coverage` 自定义目标。
- 核心模块（base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer）要求行覆盖率 ≥ 45%，其他模块 ≥ 25%；阈值可通过 `HSBA_COVERAGE_THRESHOLD_CORE` / `HSBA_COVERAGE_OTHER` 覆盖。
- `CMakePresets.json` 提供 `linux-coverage` preset，自动关闭 samples/docs/static_tests 并设置 `HSBA_COVERAGE=ON`。

### 3.7 测试
- `HSBA_SLICER_USE_TESTS` 控制 `tests/` 子目录；`HSBA_SLICER_RUN_TESTS_AFTER_BUILD` 控制 `ctest` 自动执行；交叉编译时自动关闭。
- `static_tests/` 是独立的静态断言测试集，由 `HSBA_SLICER_USE_STATIC_TESTS` 控制。
- 根 CMake 在 `enable_testing()` 之前声明 `HSBA_SLICER_USE_TESTS`，以便 `base/CMakeLists.txt` 能基于该选项启用 mockit-like stubbing。

## 4. CI 与容器化

### 4.1 GitHub Actions
- `.github/workflows/build-linux.yml`：Ubuntu runner → 安装大量 X11/GL 依赖 → clone vcpkg → `cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=...` → `cmake --build` → `ctest --parallel 2`。
- `.github/workflows/docker-rest-api.yml`：push/PR 仅运行 hadolint + 上下文校验（检查 Dockerfile 同时包含 `-DHSBA_BUILD_REST_API=ON` 和 `VCPKG_MANIFEST_FEATURES`，且 `Dockerfile.dockerignore` 未排除 `samples/tests/static_tests/docs/overlay-ports/triplets`）；完整镜像构建仅 `workflow_dispatch` 触发，支持 `mit`/`copyleft` 两种 license 输入，冒烟测试 `/api/health` + `/api/version` + `/api/pipelines`。
- 另有 `build-windows.yml`、`build-macos.yml`、`build-android.yml`、`build-ios.yml`、`test-coverage.yml`、`rest-webui.yml`、`publish-wiki.yml`、`cmake-multi-platform.yml`。

### 4.2 Docker 镜像
- `docker/Dockerfile` 两阶段：
  - builder：Ubuntu 24.04 + cmake/ninja/pkg-config/python3 → bootstrap vcpkg → `cmake --preset linux-release -DHSBA_BUILD_REST_API=ON -DHSBA_SLICER_BUILD_SAMPLES=OFF -DHSBA_SLICER_USE_TESTS=OFF -DHSBA_SLICER_USE_STATIC_TESTS=OFF -DHSBA_SLICER_BUILD_DOCS=OFF` → `cmake --install out/build/linux-release`。
  - runtime：仅安装 `libssl3/zlib/libpng/fontconfig/freetype/libmysqlclient/libpq/sqlite3`，复制 `/usr/local/` 下的 bin/lib/etc，`sed` 将 `restapi.ini` 的 `host=127.0.0.1` 改为 `0.0.0.0`，以非 root 用户 `hsba` 运行，EXPOSE 8080，HEALTHCHECK 轮询 `/api/health`。
- BuildKit 缓存：`/root/.cache/vcpkg/downloads` 与 `/root/.cache/vcpkg/installed` 挂载为 cache mount；GHCR push 通过 `docker/build-push-action@v6` 的 `cache-from/to: type=gha` 复用。

## 5. 约束与规则（来源即注释/脚本）

- **编译器最低要求**：根 CMakeLists.txt 通过 `check_cxx_feature_concepts/ranges/source_location/nttp_class/tt_match` 强制探测，缺失任一特性直接 `message(FATAL_ERROR ...)`。
- **REST API 双门控**：顶层 CMakeLists.txt 第 209–214 行要求 `HSBA_BUILD_REST_API=ON` 时必须检测到 cpp-httplib（通过 `VCPKG_MANIFEST_FEATURES` 或 `find_package(httplib)`），否则 `FATAL_ERROR`；Dockerfile 也必须在构建命令中同时传入 `-DHSBA_BUILD_REST_API=ON` 与 `VCPKG_MANIFEST_FEATURES=rest-api`（由 `.github/workflows/docker-rest-api.yml` 的 lint job 用 `grep` 强制校验）。
- **Copyleft 许可证切换**：`vcpkg.json` 的 `copyleft` feature 声明 `license: "GPL-3.0-or-later"`，顶层 CMake 在 `HSBA_COPL=ON` 时打印 `project license will be GPL-3.0-or-later`。
- **游戏主机宏**：`VCPKG_TARGET_TRIPLET` 匹配 `xbox|switch|playstation|stadia` 时强制设置 `HSBA_IS_GAME_CONSOLE` 与 `HSBA_GAME_CONSOLE_<TRIPLET>` 编译宏。
- **Windows UTF-8 代码页**：根 CMakeLists.txt 通过 `cmake/HsBaWindowsUtf8.cmake` 在每个可执行目标上嵌入 `<activeCodePage>UTF-8</activeCodePage>` manifest，使 `GetACP()` 返回 65001，窄字符路径被当作 UTF-8 处理。
- **PowerShell 必需**：`version/CMakeLists.txt` 在找不到 `pwsh`/`powershell` 时报 `FATAL_ERROR`，CI Linux 流水线显式安装 `packages-microsoft-prod.deb` + `powershell`。
- **覆盖率阈值不可随意降低**：`cmake/coverage.cmake` 注释写明 “Raise them together with new tests; do not lower them without review.”，并通过 `--fail-under-line` 在 `coverage` 目标失败时退出非零码。
- **Docker 上下文完整性**：`docker-rest-api.yml` 的 lint step 遍历 `samples tests static_tests docs overlay-ports triplets`，若 `Dockerfile.dockerignore` 排除其中任一目录即报错，确保 `cmake add_subdirectory` 所需源码不会被忽略。
- **iOS 部署目标固定**：`CMakePresets.json` 中 iOS preset 硬编码 `CMAKE_OSX_DEPLOYMENT_TARGET=26.5`。
- **Android ABI 固定**：`CMakePresets.json` 中 Android preset 固定 `ANDROID_ABI=arm64-v8a`、`ANDROID_PLATFORM=android-28`。

## 6. 构建产物关系

```
proto/* → (protobuf) → *.pb.{h,cc,py,java,csharp,php}
base → cipher → utils → 2D → paths → fileoperator → meshmodel → preprocess → support → pipelinetypes → convert → LibHsBaSlicer → DllHsBaSlicer → HsBaSlicer(可执行)
                                    ↓
                              ModuleHsBaSlicer(C++20 module, 可选)
                                    ↓
                              restapi/HsBaSlicerRestServer(可选, desktop-only)
```

所有内部静态库通过 `install(EXPORT HsBaSlicerTargets NAMESPACE HsBaSlicer::)` 导出，供下游 CMake 项目通过 `find_package(HsBaSlicer)` 消费。