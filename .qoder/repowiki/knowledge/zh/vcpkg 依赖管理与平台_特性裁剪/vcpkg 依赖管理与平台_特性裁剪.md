---
kind: dependency_management
name: vcpkg 依赖管理与平台/特性裁剪
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - CMakeLists.txt
    - docs/en/vcpkg-dependencies.md
    - docs/zh/vcpkg-dependencies.md
    - version/Generator_Version.ps1
    - version/version.cpp.in
    - overlay-ports/README.md
    - triplets/README.md
---

## 1. 使用的系统

仓库统一使用 **Microsoft vcpkg**（manifest 模式）作为第三方 C/C++ 依赖管理器，并通过 CMake 的 `find_package(... CONFIG REQUIRED)` 消费 vcpkg 生成的配置包。根目录提供：

- `vcpkg.json` — 声明所有依赖、平台过滤与可选 feature。
- `vcpkg-configuration.json` — 锁定 vcpkg 上游 baseline、注册 Microsoft artifact registry、挂载本地 overlay-ports 与 triplets。
- `overlay-ports/`、`triplets/` — 分别用于覆盖/新增 vcpkg port 与自定义 triplet（当前仅含 README 占位）。
- `docs/en/vcpkg-dependencies.md` / `docs/zh/vcpkg-dependencies.md` — 官方文档说明平台矩阵、copyleft 许可证策略与构建建议。

Android 子工程独立使用 Gradle + Android Studio，通过 JNI 加载已编译好的 HsBaSlicer 原生库，不直接参与 vcpkg 流程。

## 2. 关键文件

- `vcpkg.json`：集中声明依赖与 `default-features` / `features.copyleft`。
- `vcpkg-configuration.json`：锁定 vcpkg 上游 commit `04a9d8e5...`，启用 Microsoft artifact registry，并挂载 `./overlay-ports` 与 `./triplets`。
- `CMakeLists.txt`（根）：通过 `find_package(PkgConfig REQUIRED)` + `pkg_check_modules(Clipper2 ...)` 引入 pkg-config 后端；对 CGAL/OpenCASCADE 使用 `CONFIG QUIET` 探测以支持 copyleft 开关。
- `docs/en/vcpkg-dependencies.md`：维护依赖平台矩阵与许可证策略。
- `version/Generator_Version.ps1` + `version/version.cpp.in`：在 CMake 配置阶段解析 `vcpkg.json` 生成版本信息，包含第三方库清单。

## 3. 架构与约定

### 3.1 依赖分类与平台过滤

`vcpkg.json` 中每个依赖都显式标注适用平台：

- 桌面通用：`bit7z`、`boost-locale/log/dll/nowide`、`fontconfig`、`openvdb`、`sqlpp11`(sqlite3+mysql+postgresql)、`vcpkg-pkgconfig-get-modules` 等限定为 `windows | linux | osx`。
- 全平台基础：`boost-multiprecision/date-time/property-tree/test/units/uuid/pfr/graph`、`clipper2`、`libjpeg-turbo`、`libpng`、`eigen3`、`freetype`、`miniz`、`protobuf`、`rapidjson`、`tinyxml2`、`yaml-cpp`、`lua`、`openssl`、`magic-enum`。
- Android 专用：`opencv`（android 可用但 iOS 排除）、`libigl`（仅在非 windows/linux/osx/android 时安装，即 iOS）。
- iOS 排除：`bit7z`、`opencv`、`opencascade`、`sqlpp11`(MySQL/PG)。

### 3.2 Copyleft 许可证门控

顶层 `license: "MIT"`，同时定义 `features.copyleft`（默认在 `windows | linux | osx | android` 启用），该 feature 额外引入 `cgal`、`libigl[cgal]`、`opencascade`，并将自身 license 标记为 `GPL-3.0-or-later`。文档明确：只要启用了任一 copyleft kernel，整个项目输出即为 GPL-3.0-or-later；否则为 MIT。CMake 侧通过 `HSBA_COPL` 选项（默认 AUTO）控制是否查找并链接 CGAL/OpenCASCADE，并定义 `USE_CGAL`/`USE_OCCT` 宏。

### 3.3 双轨依赖发现

根 `CMakeLists.txt` 同时存在两类依赖发现方式：

1. **vcpkg 路径**：绝大多数依赖通过 `find_package(<Name> CONFIG REQUIRED)` 消费 vcpkg 产物（如 `Protobuf`、`RapidJSON`、`Eigen3`、`miniz`、`bit7z`、`libigl`、`OpenSSL`、`CGAL`、`OpenCASCADE`）。
2. **pkg-config 路径**：`Clipper2` 通过 `pkg_check_modules(Clipper2 REQUIRED IMPORTED_TARGET Clipper2)` 从系统/pkg-config 获取，对应 vcpkg 中的 `vcpkg-pkgconfig-get-modules` 依赖。
3. **Boost 分支**：当 `USE_CGAL` 开启时走 `find_package(Boost REQUIRED log)`，否则走 `find_package(Boost REQUIRED log_setup log)`，体现 CGAL 对 Boost 组件集合的差异。

注意：部分模块（如 `2D/CMakeLists.txt`）仍出现重复的 `find_package(libpng REQUIRED)` / `find_package(libjpeg-turbo REQUIRED)` 调用，与根层逻辑并存，属于历史遗留的双写。

### 3.4 版本与许可证元数据生成

`version/Generator_Version.ps1` 在 CMake configure 阶段解析 `vcpkg.json`，将依赖列表、构建类型、平台、许可证等信息注入 `version/version.cpp.in`，最终暴露给 `GetVersionInfo()` / `HsBaGetVersionJson()`，使运行时可报告实际链接的第三方库清单。

## 4. 约定与约束

- **依赖声明位置**：所有第三方库必须声明于根级 `vcpkg.json` 的 `dependencies` 数组，不得在单个模块的 `CMakeLists.txt` 中新增未声明的依赖。
- **平台过滤**：依赖的平台限制通过 vcpkg 的 `platform` 字段表达（如 `windows | linux | osx`、`!windows & !linux & !osx & !android`），而非在 CMake 中手工排除。
- **Feature 裁剪**：可选内核（CGAL、OpenCASCADE、libigl[cgal]）统一放入 `features.copyleft`，由 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 环境变量或 `-DHSBA_COPL=ON/OFF` 控制是否安装与链接。
- **Registry 锁定**：`vcpkg-configuration.json` 固定 vcpkg 上游 baseline (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`)，并使用 Microsoft artifact registry (`https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`) 加速下载。
- **Overlay 扩展点**：`overlay-ports/` 与 `triplets/` 目录按 vcpkg 规范挂载，用于添加自定义 port 或 triplet（当前为空模板）。
- **Android/iOS 差异**：Android 仅保留 SQLite 后端的 sqlpp11；iOS 排除 bit7z、opencv、opencascade、sqlpp11 MySQL/PG 特性，详见 `docs/en/vcpkg-dependencies.md` 的依赖矩阵表。
- **许可证产出**：构建产物的有效许可证由是否启用 copyleft feature 决定，并在 `version.cpp` 中静态写入，供 `GetVersionInfo()` 返回。
- **pkg-config 例外**：仅 `Clipper2` 通过 pkg-config 引入，其余依赖均走 vcpkg CONFIG 模式。

## 5. 已知不一致

根 `CMakeLists.txt` 中存在多处 `find_package(PkgConfig REQUIRED)` 以及 `pkg_check_modules(Clipper2 ...)` 调用，而 `vcpkg.json` 也声明了 `vcpkg-pkgconfig-get-modules`，表明该项目对 pkg-config 后端仍有依赖；同时 `2D/CMakeLists.txt` 内重复出现 `find_package(libpng/libjpeg-turbo)` 调用，与根层逻辑形成双重入口，是依赖管理上的历史遗留问题。