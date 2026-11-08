---
kind: dependency_management
name: vcpkg 依赖管理与多平台构建配置
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - CMakePresets.json
    - CMakeLists.txt
    - overlay-ports/README.md
    - triplets/README.md
    - webui/package.json
---

## 1. 使用的系统与方法

HsBaSlicer 的 C++/CMake 侧统一使用 **Microsoft vcpkg** 作为第三方库管理器；Node.js 前端控制台 `webui/` 使用 npm（无锁文件，仅声明引擎版本）。项目通过 vcpkg manifest (`vcpkg.json`) + vcpkg configuration (`vcpkg-configuration.json`) 声明依赖、特性与注册表来源，并通过 CMakePresets 将 `scripts/buildsystems/vcpkg.cmake` 注入为 toolchain file。

## 2. 关键文件

- `vcpkg.json` — 依赖清单与特性定义（copyleft、rest-api）
- `vcpkg-configuration.json` — 默认 registry、artifact catalog、overlay-ports/triplets 路径
- `CMakePresets.json` — Windows/Linux/macOS/iOS/Android 预设，统一设置 `toolchainFile=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`
- `CMakeLists.txt`（顶层）— 检测 `VCPKG_TARGET_TRIPLET`、`HSBA_BUILD_REST_API`、CGAL/OpenCASCADE 探测等
- `overlay-ports/README.md` / `triplets/README.md` — 自定义 port/triplet 占位说明
- `webui/package.json` — Node 控制台依赖（仅 `engines.node >= 20`，无运行时依赖）

## 3. 架构与约定

### 3.1 依赖声明与特性门控

`vcpkg.json` 中所有 C++ 依赖集中在 `dependencies` 数组，按平台用 `platform` 字段筛选：
- 跨桌面平台通用：`bit7z`、`boost-*`、`fontconfig`、`libjpeg-turbo`、`libpng`、`eigen3`、`freetype`、`miniz`、`openvdb`、`protobuf`、`rapidjson`、`tinyxml2`、`yaml-cpp`、`lua`、`openssl`、`magic-enum`、`clipper2`。
- Android/iOS 专用：`opencv`、`sqlpp11(sqlite3)`。
- 非 Windows/Linux/macOS/Android 平台（如 Switch/RISC-V）：`libigl`。
- 测试专用：`boost-test`。
- pkg-config 辅助：`vcpkg-pkgconfig-get-modules`（在 `windows | linux | osx` 上启用）。

两个可选特性：
- `copyleft`（default-features 之一）：引入 `cgal`、`libigl[cgal]`、`opencascade`，并将项目许可证切换为 GPL-3.0-or-later。
- `rest-api`：引入 `cpp-httplib[openssl]`，仅在 desktop 平台可用；顶层 `CMakeLists.txt` 要求同时开启 `HSBA_BUILD_REST_API=ON` 且 `VCPKG_MANIFEST_FEATURES` 包含 `rest-api`，否则报错。

### 3.2 注册表与锁定

`vcpkg-configuration.json` 固定了三个来源：
1. `default-registry`：git registry，baseline 锁定到 `microsoft/vcpkg` 的 commit `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`。
2. `registries`：artifact registry `microsoft`，指向 `vcpkg-ce-catalog` 的 zip 归档，用于加速下载预编译包。
3. `overlay-ports` → `./overlay-ports`，`overlay-triplets` → `./triplets`，提供本地覆盖能力（当前目录为空，仅有 README 说明如何添加自定义 port/triplet）。

### 3.3 CMake 集成

`CMakePresets.json` 中每个 configure preset 都设置：
```
toolchainFile: $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
binaryDir: ${sourceDir}/out/build/${presetName}
installDir: ${sourceDir}/out/install/${presetName}
```
并针对各平台指定编译器、架构、NDK 路径或 iOS SDK。Android 预设额外设置 `VCPKG_TARGET_TRIPLET=arm64-android`、`ANDROID_ABI=arm64-v8a`、`ANDROID_PLATFORM=android-28`。

顶层 `CMakeLists.txt` 通过 `find_package(Boost REQUIRED log)`、`find_package(PkgConfig REQUIRED)` 以及 `find_package(CGAL CONFIG QUIET)` / `find_package(OpenCASCADE CONFIG QUIET)` 消费 vcpkg 安装的包；REST API 分支通过 `find_package(httplib CONFIG QUIET)` 探测 `cpp-httplib`，缺失时给出明确错误提示。

### 3.4 平台与工具链

- Windows：MSVC (`cl.exe`)，Ninja generator。
- Linux：GCC/Clang，Ninja。
- macOS：Ninja。
- iOS：Xcode generator，`arm64`，`iphoneos` sysroot，部署目标 `26.5`。
- Android：通过 `ANDROID_NDK_HOME` 链入 `android.toolchain.cmake`，`arm64-v8a`，`android-28`。

## 4. 约定与约束

- **依赖集中声明于 `vcpkg.json`**：所有 C++ 第三方库均在此列出，未出现在该文件的库需走 overlay-port 或系统安装。
- **版本锁定**：通过 `vcpkg-configuration.json` 中的 git baseline 固定 vcpkg 仓库版本，避免上游 port 漂移影响可重复构建。
- **特性驱动构建**：`copyleft` 和 `rest-api` 是显式启用的可选特性；启用 `HSBA_BUILD_REST_API` 不足以构建 REST 服务，必须同时通过 `VCPKG_MANIFEST_FEATURES=rest-api` 安装 `cpp-httplib`（顶层 CMake 会检查环境变量并报错）。
- **平台裁剪**：依赖通过 `platform` 表达式限制可用平台；Android/iOS 仅安装最小集（如 `sqlpp11` 只带 `sqlite3`），桌面平台才启用完整功能（`mysql`、`postgresql`）。
- **overlay 扩展点**：`overlay-ports/` 与 `triplets/` 目录保留给自定义 port 与 triplet，但当前为空，仅由 README 说明用途。
- **Node.js 前端无依赖锁定**：`webui/package.json` 仅声明 `engines.node >= 20`，不提交 `package-lock.json`，依赖解析由运行环境决定。
- **构建产物隔离**：所有 vcpkg 安装产物位于 `vcpkg_installed/`，被 `.gitignore` 忽略；CMake 输出位于 `out/build/` 与 `out/install/`，同样不纳入版本控制。