---
kind: dependency_management
name: vcpkg 依赖管理与特性门控
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - docs/en/vcpkg-dependencies.md
    - docs/zh/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - third_parts_without_vcpkg/.gitkeep
---

## 1. 使用的系统/方法

项目使用 **Microsoft vcpkg** 作为唯一的第三方 C/C++ 依赖管理工具，通过 `vcpkg.json`（manifest）声明依赖、`vcpkg-configuration.json` 锁定上游 vcpkg 仓库的 git baseline，并通过 `overlay-ports` / `triplets` 目录提供本地覆盖端口与自定义 triplets。Android 子工程使用 Gradle + Gradle Wrapper（`gradlew`），但仅用于 Android 应用侧构建，核心 C++ 库仍由顶层 CMake + vcpkg 管理。

## 2. 关键文件

- `vcpkg.json`：声明所有第三方依赖及两个可选特性（`copyleft`、`rest-api`）。
- `vcpkg-configuration.json`：固定 vcpkg 源码基线 `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`，注册 Microsoft artifact registry，并挂载 `./overlay-ports` 与 `./triplets`。
- `docs/en/vcpkg-dependencies.md`（同步存在 `docs/zh/vcpkg-dependencies.md`）：平台兼容性矩阵、许可证策略、CMake 集成说明与故障排查。
- `overlay-ports/README.md`：自定义 port 的放置位置说明。
- `triplets/README.md`：自定义 triplet 的放置位置说明。
- `third_parts_without_vcpkg/`：空目录，预留不通过 vcpkg 管理的第三方代码位置。
- 根 `CMakeLists.txt`：解析 `VCPKG_MANIFEST_FEATURES` / `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 环境变量，推导 `HSBA_COPL` 与 `HSBA_BUILD_REST_API` 行为。

## 3. 架构与约定

### 3.1 依赖清单与平台门控

`vcpkg.json` 中每个依赖均可带 `platform` 字段进行平台过滤。例如：
- `bit7z`、`boost-locale`、`boost-log`、`boost-dll`、`boost-nowide`、`fontconfig`、`opencv`、`openvdb`、`sqlpp11`(MySQL/PG)、`vcpkg-pkgconfig-get-modules`、`opencascade` 在 Android/iOS 上被排除。
- `libigl` 默认以 `!windows & !linux & !osx & !android` 排除桌面平台，仅在 iOS 等平台上启用。
- `sqlpp11` 出现两次：桌面端启用 sqlite3/mysql/postgresql 三个 feature；移动端仅启用 sqlite3。

### 3.2 可选特性（Features）

- **`copyleft`**：引入 CGAL (GPL-3.0-or-later)、OpenCascade (LGPL-2.1-only) 与 libigl[cgal]，并将项目整体许可证切换为 GPL-3.0-or-later。该特性在 `default-features` 中对 `windows | linux | osx | android` 默认启用。
- **`rest-api`**：引入 `cpp-httplib`，配合 CMake 选项 `HSBA_BUILD_REST_API=ON` 编译可选的 HsBaSlicerRestServer 进程，且仅限桌面平台。

### 3.3 许可证策略

根级 `license: "MIT"`，`copyleft` 特性的 `license: "GPL-3.0-or-later"`。是否启用 copyleft 内核决定最终构建产物许可证。有效许可证在配置阶段生成到 `version.cpp`，并通过 `GetVersionInfo()` / `HsBaGetVersionJson()` 暴露。

### 3.4 CMake 与 vcpkg 的集成

根 `CMakeLists.txt` 实现两套门控逻辑：

- **`HSBA_BUILD_REST_API`**：若设为 ON 且非桌面平台则强制关闭；若开启则检查 `VCPKG_MANIFEST_FEATURES` 环境变量是否包含 `rest-api`，否则 abort 配置并提示需设置 `VCPKG_MANIFEST_FEATURES=rest-api`。
- **`HSBA_COPL`**：三态（AUTO/ON/OFF）。AUTO 模式下依次检测 `VCPKG_MANIFEST_FEATURES` → `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` → 探测 CGAL/OpenCascade 是否已安装（因为 vcpkg default-features 不会出现在 `VCPKG_MANIFEST_FEATURES` 中）。

### 3.5 私有/镜像源

`vcpkg-configuration.json` 注册了 Microsoft artifact registry（`https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`），baseline 指向官方 vcpkg 仓库的固定 commit，确保可重现构建。

## 4. 约定与约束

- **规则 1**：所有 C/C++ 第三方依赖必须声明在 `vcpkg.json` 的 `dependencies` 或 `features.*.dependencies` 中，不得直接 `find_package` 未声明的包。（来源：`vcpkg.json` 是唯一依赖清单，文档 `docs/en/vcpkg-dependencies.md` 描述其作用。）
- **规则 2**：新增依赖时需在 `vcpkg.json` 中显式指定 `platform` 以排除 Android/iOS 上不兼容的包；具体排除列表见 `docs/en/vcpkg-dependencies.md` 的 Platform Exclusions 章节。
- **规则 3**：启用 `HSBA_BUILD_REST_API=ON` 时必须同时设置 `VCPKG_MANIFEST_FEATURES=rest-api`，否则 CMake 配置阶段 abort。（来源：根 `CMakeLists.txt` 第 181–200 行对 `VCPKG_MANIFEST_FEATURES` 的检查与错误消息。）
- **规则 4**：要获得 MIT 许可证构建，需在配置前设置 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 以禁用 `copyleft` 默认特性，从而不安装 CGAL/OpenCascade。（来源：`docs/en/vcpkg-dependencies.md` Build Recommendations 与根 `CMakeLists.txt` 的 `HSBA_COPL` 检测逻辑。）
- **规则 5**：自定义 vcpkg port 应放在 `overlay-ports/`，自定义 triplet 应放在 `triplets/`，并在 `vcpkg-configuration.json` 中通过 `overlay-ports` / `overlay-triplets` 注册。（来源：`vcpkg-configuration.json` 与 `overlay-ports/README.md`、`triplets/README.md`。）
- **规则 6**：`third_parts_without_vcpkg/` 目录为空，当前无不在 vcpkg 管理下的第三方代码；如需引入此类代码，应置于该目录并另行处理。
- **规则 7**：vcpkg 上游版本通过 `vcpkg-configuration.json` 中的 `default-registry.baseline` 固定，禁止随意升级 vcpkg 仓库而不更新此值。（来源：`vcpkg-configuration.json` 第 3–6 行。）