---
kind: dependency_management
name: 基于 vcpkg 的 C++ 依赖管理与多平台特性门控
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - CMakeLists.txt
    - docs/en/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - third_parts_without_vcpkg/
    - android/app/build.gradle
---

## 1. 使用的系统与工具

仓库使用 **vcpkg**（Microsoft 的 C/C++ 包管理器）作为唯一的第三方依赖声明与安装入口。CMake 通过 `find_package(... CONFIG REQUIRED)` 消费 vcpkg 安装的包，不直接调用系统包管理器。

- 清单文件：`vcpkg.json`
- 注册表/版本锁定：`vcpkg-configuration.json`
- 覆盖端口与自定义 triplets：`overlay-ports/`、`triplets/`（均为空目录 + README，预留扩展点）
- 文档化策略：`docs/en/vcpkg-dependencies.md`（及对应中文翻译）
- Android 子工程独立使用 Gradle（`android/app/build.gradle`），与主 C++ 工程的 vcpkg 解耦。

## 2. 关键文件

| 文件 | 作用 |
|---|---|
| `vcpkg.json` | 声明所有第三方依赖、平台过滤、可选 feature（`copyleft`、`rest-api`） |
| `vcpkg-configuration.json` | 固定 vcpkg 上游 git baseline、启用 microsoft artifact registry、挂载 overlay-ports/triplets |
| `CMakeLists.txt`（根） | 通过 `find_package` 消费依赖，按平台/feature 分支链接逻辑 |
| `docs/en/vcpkg-dependencies.md` | 许可证矩阵、Android/iOS 排除说明、构建建议与故障排查 |
| `.github/workflows/*.yml` | CI 中触发 vcpkg install（见各 workflow） |
| `third_parts_without_vcpkg/` | 保留给“不走 vcpkg”的三方代码（当前为空目录） |

## 3. 架构与约定

### 3.1 依赖分类与平台门控

`vcpkg.json` 中的依赖按平台能力分组，使用 `platform` 字段精确控制安装范围：

- **全平台通用**：`boost-multiprecision`、`boost-property-tree`、`boost-test`、`boost-units`、`boost-uuid`、`boost-pfr`、`boost-graph`、`clipper2`、`eigen3`、`miniz`、`protobuf`、`rapidjson`、`tinyxml2`、`yaml-cpp`、`lua`、`openssl`、`magic-enum`。
- **Windows/Linux/macOS 专用**：`bit7z`、`boost-locale`、`boost-log`、`boost-dll`、`boost-nowide`、`fontconfig`、`opencv`、`openvdb`、`sqlpp11`（含 SQLite3/MySQL/PostgreSQL）、`vcpkg-pkgconfig-get-modules`。
- **Android/iOS 精简集**：仅 `sqlpp11[sqlite3]`，其余被显式排除。
- **libigl** 默认在 `!windows & !linux & !osx & !android` 上禁用（即仅在 iOS 以外的非桌面平台？实际语义是只在未列出的平台上安装，iOS 由 CMake 层额外处理）。

### 3.2 可选 Feature 机制

- **`copyleft`**（默认启用于 `windows | linux | osx | android`）：引入 `cgal`、`libigl[cgal]`、`opencascade`，将项目许可证从 MIT 切换为 GPL-3.0-or-later。CMake 侧通过三态选项 `HSBA_COPL=AUTO|ON|OFF` 检测并定义 `USE_CGAL` / `USE_OCCT` / `HSBA_COPL` 宏。
- **`rest-api`**（默认关闭）：引入 `cpp-httplib`，构建可选的 `HsBaSlicerRestServer`。CMake 中 `HSBA_BUILD_REST_API=ON` 会检查 `VCPKG_MANIFEST_FEATURES` 环境变量或探测 `httplib` 是否已安装，否则 `FATAL_ERROR`。

### 3.3 CMake 消费模式

根 `CMakeLists.txt` 统一通过 `find_package(<Name> CONFIG REQUIRED)` 消费依赖，并按平台/feature 分支决定是否查找和链接：

- Boost：Windows 下 `-DBOOST_LOG_DYN_LINK` + `find_package(Boost REQUIRED log)`；Android/iOS/GameConsole 走 `find_package(Boost REQUIRED)`（无 log）。
- OpenCV：`NOT IOS AND NOT HSBA_GAME_CONSOLE` 时启用，定义 `HAS_OPENCV`。
- CGAL/OpenCASCADE：仅在 `HSBA_COPL` 且非 mobile/game-console 时启用，定义 `USE_CGAL` / `USE_OCCT` / `USE_CAD_KERNEL`。
- OpenVDB：非 mobile/game-console 时启用，定义 `USE_OPENVDB`。
- sqlpp11：桌面端要求 `SQLite3 MySQL PostgreSQL` 三个组件；移动端仅 `SQLite3`。
- bit7z：由 `HSBA_HSBA_USE_BIT7Z` 开关，非桌面平台强制关闭。
- libigl：始终启用，定义 `USE_IGL`。

### 3.4 注册表与版本锁定

`vcpkg-configuration.json` 固定了 vcpkg 上游 commit `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`，并通过 `registries` 引入 Microsoft 官方 artifact registry（`microsoft/vcpkg-ce-catalog`）。这使依赖来源可重现，不受本地缓存污染。

### 3.5 许可证门控

顶层 `license: "MIT"`，`copyleft` feature 的 `license: "GPL-3.0-or-later"`。CMake 在配置阶段根据是否发现 CGAL/OpenCascade 生成 `version.cpp` 中的有效许可证信息，并通过 `GetVersionInfo()` / `HsBaGetVersionJson()` 暴露。

### 3.6 Android 子工程

`android/` 目录是独立的 Gradle 工程，依赖管理完全由 `build.gradle` / `settings.gradle` 负责，与主工程的 vcpkg 解耦。其 `jniLibs/arm64-v8a/` 下预编译了原生库。

## 4. 约定与约束

- **所有 C++ 第三方依赖必须通过 `vcpkg.json` 声明**，并在 `CMakeLists.txt` 中以 `find_package(... CONFIG REQUIRED)` 消费；不存在手写 `Find*.cmake` 或硬编码路径的依赖查找。
- **平台兼容性以 `vcpkg.json` 的 `platform` 字段为权威来源**，CMake 层的条件分支应与该声明保持一致（见 `docs/en/vcpkg-dependencies.md` 中的依赖矩阵）。
- **Android/iOS 必须使用精简依赖集**：`boost-{locale,log,dll,nowide}`、`fontconfig`、`bit7z`、`opencv`、`opencascade`、`openvdb`、`sqlpp11` 的 MySQL/PostgreSQL 组件、`vcpkg-pkgconfig-get-modules` 在这些平台被显式排除。
- **启用 copyleft 内核会改变项目许可证**：任何包含 CGAL 或 OpenCASCADE 的构建自动采用 GPL-3.0-or-later；要获得 MIT 许可证构建，需在运行 vcpkg 前设置 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 以禁用 default-features。
- **REST API 构建必须同时开启 CMake 选项与 vcpkg feature**：仅设 `HSBA_BUILD_REST_API=ON` 不够，还必须通过 `VCPKG_MANIFEST_FEATURES=rest-api` 安装 `cpp-httplib`，否则 CMake 配置阶段报错。
- **`third_parts_without_vcpkg/` 目录保留给不走 vcpkg 的三方代码**；当前为空，但结构表明新增此类依赖应放在此目录而非 vcpkg。
- **overlay-ports 与 triplets 目录已挂载但未使用**（`overlay-ports/README.md`、`triplets/README.md` 存在但目录下无自定义 port/triplet），为未来自定义包或目标三元组预留扩展点。