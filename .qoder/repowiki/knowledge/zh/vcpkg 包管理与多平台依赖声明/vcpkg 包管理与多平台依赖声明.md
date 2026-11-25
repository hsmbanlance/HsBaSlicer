---
kind: dependency_management
name: vcpkg 包管理与多平台依赖声明
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - overlay-ports/README.md
    - overlay-ports/opencascade/portfile.cmake
    - overlay-ports/snappy/portfile.cmake
    - CMakeLists.txt
    - cadmodel/CMakeLists.txt
    - webui/package.json
    - docs/en/vcpkg-dependencies.md
    - docs/zh/vcpkg-dependencies.md
---

## 1. 使用的系统

仓库采用 **Microsoft vcpkg** 作为 C/C++ 第三方依赖管理器，同时为 Web UI 使用 **Node.js/npm**（`webui/package.json`）。CMake 通过 `VCPKG_TARGET_TRIPLET` 感知目标三元组，并在顶层 `CMakeLists.txt` 中根据 triplet 名称识别 Xbox/Switch/PlayStation/Stadia 等游戏主机平台。

## 2. 关键文件

- `vcpkg.json`：主清单，声明所有 C/C++ 依赖、默认特性与可选特性。
- `vcpkg-configuration.json`：锁定 vcpkg 工具版本（git baseline `93ad765c...`），注册 Microsoft artifact registry，并挂载本地 overlay-ports/triplets。
- `overlay-ports/README.md` + `overlay-ports/opencascade/` + `overlay-ports/snappy/`：本地覆盖 port，对 OpenCASCADE 和 snappy 打补丁并自定义构建逻辑。
- `triplets/README.md`：自定义 triplet 说明。
- `third_parts_without_vcpkg/`：明确标记不通过 vcpkg 管理的第三方代码目录。
- `webui/package.json`：Web UI 的 npm 依赖（仅声明 name/version/license/scripts，无运行时依赖）。
- 文档 `docs/en/vcpkg-dependencies.md` / `docs/zh/vcpkg-dependencies.md`：依赖说明文档。

## 3. 架构与约定

### 3.1 依赖分类与平台约束

`vcpkg.json` 将依赖分为三类：

- **通用依赖**：bit7z、boost-*（locale/log/dll/multiprecision/date-time/nowide/property-tree/test/units/uuid/pfr/graph）、clipper2、libjpeg-turbo(turbojpeg)、libpng、eigen3、freetype、miniz、protobuf、rapidjson、tinyxml2、yaml-cpp、lua、openssl、magic-enum、opencv（含 android 平台）。
- **桌面/Android/iOS 专用**：openvdb（排除 android/ios）、sqlpp11（android/ios 仅 sqlite3 后端；windows/linux/osx 启用 sqlite3/mysql/postgresql 三个后端）、vcpkg-pkgconfig-get-modules（用于 pkg-config 发现依赖）。
- **可选 copyleft 内核**：通过 `features.copyleft` 引入 CGAL (GPL-3.0-or-later)、libigl(cgal)、OpenCASCADE(tcl)，并将项目许可证从 MIT 切换为 GPL-3.0-or-later。该特性在 `default-features` 中声明，适用于 windows/linux/osx/android。
- **可选 REST API**：通过 `features.rest-api` 引入 cpp-httplib(openssl)，仅在 desktop 平台可用，需显式设置 `HSBA_BUILD_REST_API=ON` 且安装 `rest-api` feature。

### 3.2 覆盖 Port 机制

`vcpkg-configuration.json` 通过 `overlay-ports: ["./overlay-ports"]` 启用本地覆盖。当前覆盖两个 port：

- `overlay-ports/opencascade/`：提供 5 个 patch（`0001-cmake-keep-build-use-vcpkg-explicit.patch` ~ `0005-drop-bin-letter.patch`）以及自定义 `portfile.cmake` 和 `vcpkg.json`，用于解决 OpenCASCADE 在 vcpkg 下的 CMake 导出、FreeImage autolink、二进制后缀等问题，并支持 tcl 特性。
- `overlay-ports/snappy/`：提供 AVX include guard、no-werror、pkgconfig.diff、rtti.diff 等补丁，并生成 `.pc.in` 以暴露 pkg-config 接口。

### 3.3 CMake 集成

- 顶层 `CMakeLists.txt` 读取 `VCPKG_TARGET_TRIPLET`，若匹配 xbox/switch/playstation/stadia 则定义 `HSBA_IS_GAME_CONSOLE` 与 `HSBA_GAME_CONSOLE_${TRIPLET}`，并强制关闭非必需功能。
- REST API 构建开关 `HSBA_BUILD_REST_API` 会检查环境变量 `VCPKG_MANIFEST_FEATURES` 是否包含 `rest-api`，确保 cpp-httplib 已安装才允许构建。
- `cadmodel/CMakeLists.txt` 在检测到 OpenCASCADE 未带 TCL 时给出 FATAL_ERROR，提示用户确认 overlay-ports/opencascade 已提交且 vcpkg_installed 已重建。

### 3.4 版本锁定策略

- vcpkg 工具本身通过 `vcpkg-configuration.json` 中的 `default-registry.baseline` 锁定到指定 git commit，保证工具链一致性。
- 第三方库版本由 vcpkg registry 的 snapshot 决定，仓库不直接写版本号。
- Node.js 引擎要求 `>=20`（`webui/package.json` engines 字段）。

## 4. 约定与约束

- **所有 C/C++ 第三方依赖必须声明在 `vcpkg.json` 中**，不得手动链接 `third_parts_without_vcpkg/` 之外的源码。
- **需要修改上游 port 行为时，应添加覆盖 port 到 `overlay-ports/<name>/`**，而非直接修改 vcpkg 缓存。
- **copyleft 内核（CGAL/OpenCASCADE/libigl）是可选特性**，启用后项目许可证自动升级为 GPL-3.0-or-later；禁用时保持 MIT。
- **REST API 是可选构建项**，需在 CMake 选项 `HSBA_BUILD_REST_API=ON` 之外额外启用 vcpkg feature `rest-api`（`-DVCPKG_MANIFEST_FEATURES=rest-api`），否则 cpp-httplib 不会被安装。
- **Android/iOS 平台限制**：openvdb、libigl(除 uwp) 不支持；sqlpp11 在移动端仅启用 sqlite3 后端。
- **Overlay ports 必须随仓库一起提交**，否则 OpenCASCADE 构建会因缺少 TKDraw 而失败（见 `cadmodel/CMakeLists.txt` 的 FATAL_ERROR 信息）。
- **游戏主机 triplet**：当 `VCPKG_TARGET_TRIPLET` 匹配 xbox/switch/playstation/stadia 时，自动定义 `HSBA_IS_GAME_CONSOLE` 宏并关闭非必要功能，供条件编译使用。
- **Web UI 依赖独立管理**：`webui/package.json` 仅声明 Node.js 脚本入口与测试命令，不包含任何运行时依赖，表明前端静态资源由服务器端分发。