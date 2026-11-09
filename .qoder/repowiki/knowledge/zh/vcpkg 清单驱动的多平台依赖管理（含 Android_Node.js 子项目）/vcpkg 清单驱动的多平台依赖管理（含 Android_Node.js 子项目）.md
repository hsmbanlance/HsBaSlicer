---
kind: dependency_management
name: vcpkg 清单驱动的多平台依赖管理（含 Android/Node.js 子项目）
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - docs/en/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - CMakeLists.txt
    - webui/package.json
    - android/build.gradle
    - android/app/build.gradle
---

## 1. 使用的系统与方法

仓库以 **Microsoft vcpkg** 作为 C/C++ 第三方依赖的唯一声明入口，采用 vcpkg 的 **manifest 模式**（`vcpkg.json` + `vcpkg-configuration.json`），并通过 **registries + overlay-ports + triplets** 机制扩展包源与构建三元组。Android 子工程使用 Gradle（仅打包预编译 `.so`，不声明 Java 依赖），Web UI 子目录使用 Node.js 的 `package.json`。

## 2. 关键文件

- `vcpkg.json`：顶层依赖清单，定义 30+ 个包（Boost、Eigen、OpenCV、libigl、OpenVDB、protobuf、rapidjson、yaml-cpp、lua、openssl、sqlpp11、bit7z、clipper2、freetype、fontconfig、tinyxml2、miniz、magic-enum 等），并按平台条件启用。
- `vcpkg-configuration.json`：锁定 vcpkg 上游 git baseline (`04a9d8e5...`)，注册 Microsoft artifact registry (`github.com/microsoft/vcpkg-ce-catalog`)，并挂载本地 `overlay-ports` 与 `triplets`。
- `overlay-ports/README.md` / `triplets/README.md`：预留自定义 port 与 triplet 的占位说明。
- `docs/en/vcpkg-dependencies.md`：官方依赖文档，记录许可矩阵、平台排除规则、CMake 修复历史与故障排查。
- `CMakeLists.txt`（根）：解析 `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 环境变量，实现 `HSBA_COPL`（CGAL/OpenCascade）与 `rest-api` 特性的双向门控。
- `webui/package.json`：Node.js 子项目（standalone browser console + same-origin proxy），声明 `engines.node >= 20`，无运行时依赖。
- `android/build.gradle` / `android/app/build.gradle`：Gradle 仅配置 Android Application plugin (8.3.2)、compileSdk/targetSdk=34、ABI=`arm64-v8a`，`dependencies {}` 为空——NDK 库通过 `jniLibs.srcDirs=['src/main/jniLibs']` 引入预编译二进制。

## 3. 架构与约定

### 3.1 特性门控（Feature Gates）

`vcpkg.json` 通过 `features` 把可选依赖拆成两个门：

- `copyleft`：默认在 `windows | linux | osx | android` 上启用，引入 CGAL、libigl[cgal]、OpenCASCADE；启用后整个项目的许可证从 MIT 切换为 GPL-3.0-or-later。该特性由根 `CMakeLists.txt` 中的 `HSBA_COPL` 选项（AUTO/ON/OFF）控制，AUTO 模式下按以下顺序探测：
  1. 检查 `VCPKG_MANIFEST_FEATURES` 是否包含 `copyleft`；
  2. 检查 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 是否被设置（非 0/OFF/FALSE/N）；
  3. 回退到 find_package(CGAL/OpenCASCADE) 探测安装状态。
- `rest-api`：默认关闭，仅在 Windows/Linux/macOS 上可用，引入 `cpp-httplib[openssl]`；根 CMake 中 `-DHSBA_BUILD_REST_API=ON` 会强制要求 manifest feature 已安装，否则 abort configure。

### 3.2 平台条件依赖

依赖按平台用 `{name, platform}` 对象声明，而非在 CMake 中拆分：

| 包 | 平台限制 | 原因 |
|---|---|---|
| bit7z | `!android`, `!ios` | 移动端不需要 7z 压缩 |
| boost-{locale,log,dll,nowide} | `!android`, `!ios` | Android 兼容性问题 |
| fontconfig | `!android` | Android 自有字体管理 |
| opencv | `!ios` | iOS 构建复杂度高 |
| openvdb | `!android`, `!ios` | 体积大且移动场景不需要 |
| sqlpp11(sqlite3) | android/ios 仅 sqlite3 特性 | MySQL/PostgreSQL 不支持交叉编译 |
| libigl | `!uwp` | UWP 不支持 |

### 3.3 Registry 与可重现性

`vcpkg-configuration.json` 固定了 vcpkg 工具链的 git baseline，并使用 Microsoft 的 **artifact registry**（`vcpkg-ce-catalog` zip）来加速下载预构建二进制，减少每次构建都从源码编译的成本。`overlay-ports` 与 `triplets` 目录用于本地覆盖端口和自定义 ABI（如 Nintendo Switch2、RISC-V embedded Linux）。

### 3.4 其他语言依赖

- Web UI (`webui/package.json`)：纯 Node.js 脚本，无 npm 依赖，仅声明 `engines.node >= 20`。
- Android (`android/`)：Gradle 仅管理 Android SDK/Plugin 版本，NDK 产物以预编译 `.so` 形式通过 `jniLibs` 引入，不经过 Gradle 依赖解析。

## 4. 约定与约束

- **所有 C/C++ 第三方依赖必须声明在 `vcpkg.json`**，不得在 CMake 中直接 `find_package` 未注册的包（除 CGAL/OpenCASCADE 的回退探测外）。
- **新增依赖需同时维护 `docs/en/vcpkg-dependencies.md` 的平台矩阵**，明确 Android/iOS 的排除理由。
- **许可证门控**：任何 copyleft 依赖（CGAL GPL-3.0-or-later、OpenCASCADE LGPL-2.1-only）只能通过 `copyleft` feature 引入；启用后项目整体许可证变为 GPL-3.0-or-later，并在构建时生成 `version.cpp` 暴露给 `GetVersionInfo()` / `HsBaGetVersionJson()`。
- **REST API 构建门控**：`-DHSBA_BUILD_REST_API=ON` 必须在 `VCPKG_MANIFEST_FEATURES` 中包含 `rest-api`，否则根 `CMakeLists.txt` 直接 abort configure（见 `.github/copilot-instructions.md` 与 workflow 配置）。
- **vcpkg 基线锁定**：`vcpkg-configuration.json` 中的 `baseline` 字段锁定了 vcpkg 上游 commit，确保 CI 与本地构建一致。
- **Android ABI 限定**：`android/app/build.gradle` 中 `abiFilters "arm64-v8a"`，只产出 arm64 原生库。
- **Node.js 引擎约束**：`webui/package.json` 通过 `engines.node >= 20` 声明最低 Node 版本。

## 5. 观察到的缺失

- 仓库根目录没有 `vcpkg.lock` 或 `vcpkg-binary-cache` 配置；可重现性主要依赖 `vcpkg-configuration.json` 的 baseline 与 artifact registry。
- `third_parts_without_vcpkg/` 当前为空，但目录存在表明未来可能存放不走 vcpkg 的手动 vendored 代码。
- Android 子工程没有 `gradle.properties` 中的私有仓库配置（`local.properties` 存在但未展示内容），依赖来源完全依赖预编译 `.so`。