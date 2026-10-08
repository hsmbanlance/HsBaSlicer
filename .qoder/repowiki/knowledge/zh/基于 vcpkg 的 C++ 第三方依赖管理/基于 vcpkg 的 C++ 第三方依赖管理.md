---
kind: dependency_management
name: 基于 vcpkg 的 C++ 第三方依赖管理
category: dependency_management
scope:
    - '**'
source_files:
    - vcpkg.json
    - vcpkg-configuration.json
    - CMakePresets.json
    - version/Generator_Version.ps1
    - version/version.cpp.in
    - docs/en/vcpkg-dependencies.md
    - docs/zh/vcpkg-dependencies.md
    - overlay-ports/README.md
    - triplets/README.md
    - third_parts_without_vcpkg/CMakeLists.txt
---

## 1. 使用的系统/方法

HsBaSlicer 仓库使用 **Microsoft vcpkg** 作为唯一的 C/C++ 第三方依赖管理器，通过 `vcpkg.json` 声明依赖、`vcpkg-configuration.json` 锁定版本与注册表，并通过 CMakePresets 在构建时注入 `VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake` toolchain。

Android 子项目（`android/`）使用 Gradle + NDK，但仅用于打包演示 APK；C++ 原生库仍由顶层 CMake + vcpkg 构建后以预编译 `.so` 形式放入 `jniLibs/arm64-v8a/`，Gradle 本身不声明任何 C++ 依赖。

## 2. 关键文件

- `vcpkg.json` — 依赖清单，定义所有第三方包、平台条件与 `copyleft` feature。
- `vcpkg-configuration.json` — 锁定 vcpkg 上游 git baseline (`04a9d8e...`)，注册 Microsoft artifact registry，并挂载本地 `overlay-ports` 与 `triplets`。
- `CMakePresets.json` — 为 Windows/Linux/macOS/Android/iOS 提供预设，统一设置 `toolchainFile: $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`。
- `version/Generator_Version.ps1` — CMake 配置阶段解析 `vcpkg.json`，生成 `version.cpp`，内嵌每个依赖的许可证与主页 URL，并据此决定项目最终许可证（MIT 或 GPL-3.0-or-later）。
- `docs/en/vcpkg-dependencies.md` / `docs/zh/vcpkg-dependencies.md` — 平台兼容性矩阵与 copyleft feature 说明文档。
- `overlay-ports/README.md`、`triplets/README.md` — 自定义 port/triplet 的占位目录说明。
- `third_parts_without_vcpkg/CMakeLists.txt` — 空模板，预留不使用 vcpkg 的手动集成点。

## 3. 架构与约定

### 依赖声明结构
`vcpkg.json` 中约 35 个依赖按用途分组：Boost 系列（locale/log/dll/nowide/date-time/property-tree/test/units/uuid/pfr/graph/multiprecision）、几何/网格（clipper2、libigl、cgal、opencascade、openvdb、eigen3、freetype）、序列化/协议（protobuf、rapidjson、yaml-cpp、tinyxml2）、加密（openssl、bit7z）、数据库（sqlpp11）、图像（opencv、libjpeg-turbo、libpng）、工具（magic-enum、miniz、fontconfig、vcpkg-pkgconfig-get-modules）。

### 平台条件
依赖通过 `platform` 字段进行细粒度控制：
- 桌面三平台通用：`windows | linux | osx`
- 含 Android：额外加 `| android`（如 opencv、cgal、libigl[cgal]、opencascade）
- 排除某平台：使用 `!windows & !linux & !osx & !android`（如 libigl 的非 copyleft 变体）
- sqlpp11 针对 desktop 启用 sqlite3+mysql+postgresql features，针对 android|ios 仅启用 sqlite3

### Copyleft feature
`vcpkg.json` 顶层 `license` 为 `MIT`，同时定义 `copyleft` feature（默认在 windows/linux/osx/android 启用），该 feature 引入 cgal (GPL-3.0-or-later)、libigl[cgal]、opencascade (LGPL-2.1-only)，并将 feature 自身的 `license` 设为 `GPL-3.0-or-later`。`Generator_Version.ps1` 在配置阶段根据是否启用 copyleft 以及依赖的许可证字符串（匹配 `GPL` 且不含 ` OR `）动态决定生成的 `VersionInfo::license` 是 MIT 还是 GPL-3.0-or-later。

### 注册表与锁定
`vcpkg-configuration.json` 将 default registry 指向微软官方 vcpkg 仓库的固定 git commit (`04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`)，并额外注册 `microsoft` artifact registry（`https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`）。这实现了“可复现的 vcpkg 版本 + 二进制制品缓存”的组合策略。

### Overlay ports 与 triplets
`overlay-ports/` 与 `triplets/` 分别通过 `overlay-ports` 和 `overlay-triplets` 字段挂载到 vcpkg 配置中，用于放置项目自维护的 port 与自定义 triplet（README 示例提到 Nintendo Switch2 的 `arm64-switch2` 与 RISC-V embedded 的 `riscv-embedded`）。

### CMake 集成
所有 preset 均通过 `toolchainFile: "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"` 接入 vcpkg。Android preset 额外设置 `VCPKG_TARGET_TRIPLET=arm64-android`、`VCPKG_CHAINLOAD_TOOLCHAIN_FILE=$env{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake`、`ANDROID_ABI=arm64-v8a`、`ANDROID_PLATFORM=android-28`。iOS preset 设置 `CMAKE_SYSTEM_NAME=iOS`、`CMAKE_OSX_ARCHITECTURES=arm64`、`CMAKE_OSX_SYSROOT=iphoneos`、`CMAKE_OSX_DEPLOYMENT_TARGET=26.5`。

### 版本与依赖清单生成
`Generator_Version.ps1` 在 CMake configure 阶段读取 `vcpkg.json`，遍历 dependencies（包括 copyleft feature 下的附加依赖），对照内置 `libInfoMap`（硬编码的许可证与 homepage 映射）生成 `version.cpp`，输出包含 `librariesName`、`license`、`version`、`buildType`、`buildPlatform`、`configureTime`、`vcpkgTargetTriplet` 及 `thirdLibraries` 列表的 `VersionInfo`。若解析失败则回退为注释提示手动添加。

## 4. 约定与约束

- **所有 C/C++ 第三方依赖必须声明于 `vcpkg.json`**，不得以手工方式链接未声明的库（`third_parts_without_vcpkg/` 仅为空模板，当前无实际内容）。
- **依赖版本锁定**：通过 `vcpkg-configuration.json` 中的 `default-registry.baseline` 固定 vcpkg 自身版本，而非在 `vcpkg.json` 中写死具体版本号——依赖版本由 vcpkg 上游在该 baseline 下提供。
- **平台条件集中维护**：各依赖的 `platform` 表达式与 `docs/en/vcpkg-dependencies.md` 中的平台矩阵保持一致；Android 上显式排除 boost-locale/log/dll/nowide、fontconfig、bit7z、opencascade、sqlpp11 MySQL/PostgreSQL features、vcpkg-pkgconfig-get-modules；iOS 上排除 bit7z、opencv、opencascade、sqlpp11 MySQL/PostgreSQL features。
- **Copyleft 许可证传播**：启用 CGAL/OpenCascade 等 copyleft 内核会使整个项目的有效许可证从 MIT 切换为 GPL-3.0-or-later，该决策由 `Generator_Version.ps1` 在配置阶段自动推导并在运行时通过 `GetVersionInfo()` 暴露。
- **Android 构建需关闭默认 feature 以得到 MIT 许可证**：文档要求通过环境变量 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=1` 禁用 default-features（含 copyleft），使 vcpkg 不安装 CGAL/OpenCascade，从而产出 MIT 许可的二进制。
- **自定义扩展点**：新增第三方库应优先尝试 vcpkg port；若需修改 port 行为则在 `overlay-ports/` 中添加同名 port，新增目标平台 triplet 则在 `triplets/` 中添加对应文件。
- **Gradle 侧不声明 C++ 依赖**：`android/app/build.gradle` 仅负责打包已构建好的 `.so`，C++ 依赖链完全由顶层 CMake + vcpkg 管理。

## 5. 不适用项

仓库中没有 Go (`go.mod`/`go.sum`)、JavaScript (`package.json`/`yarn.lock`)、Python (`requirements.txt`/`pyproject.toml`) 等语言层面的依赖清单；Android Gradle 仅打包产物而不声明 C++ 依赖。因此本卡片仅覆盖 C++ 侧的 vcpkg 体系。