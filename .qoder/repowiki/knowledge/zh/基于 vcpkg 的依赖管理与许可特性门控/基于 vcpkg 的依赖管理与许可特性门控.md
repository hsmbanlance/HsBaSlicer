---
kind: dependency_management
name: 基于 vcpkg 的依赖管理与许可特性门控
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
    - android/build.gradle
---

## 1. 使用的系统/方法

仓库统一使用 **vcpkg**（Microsoft 的 C/C++ 包管理器）作为第三方依赖来源，通过 `vcpkg.json` 清单声明依赖、`vcpkg-configuration.json` 锁定上游 registry 基线并启用 overlay ports/triplets。Android/iOS 示例应用使用 Gradle + NDK，但原生库本身不通过 Gradle 拉取依赖——依赖由顶层 CMake + vcpkg 在构建前解析。

## 2. 关键文件

- `vcpkg.json`：依赖清单，声明所有第三方库及平台/特性约束。
- `vcpkg-configuration.json`：registry 配置，固定 vcpkg 官方目录的 git baseline，注册 Microsoft artifact registry，并挂载 `overlay-ports` 与 `triplets`。
- `overlay-ports/README.md`、`triplets/README.md`：自定义 port / triplet 的占位说明。
- `docs/en/vcpkg-dependencies.md`：依赖矩阵、平台排除规则、copyleft 许可策略的权威文档。
- 根 `CMakeLists.txt`：将 vcpkg manifest feature (`VCPKG_MANIFEST_FEATURES`) 映射到 CMake 选项 `HSBA_COPL`，控制 CGAL/OpenCascade 的查找与链接。
- `android/build.gradle`：仅声明 Android Gradle Plugin 版本，不声明原生依赖。

## 3. 架构与约定

### 3.1 依赖声明与平台过滤

`vcpkg.json` 中每个依赖均可带 `platform` 字段进行条件安装，例如：
- `bit7z`、`boost-*`、`fontconfig`、`opencv`、`openvdb`、`sqlpp11`(MySQL/PG)、`vcpkg-pkgconfig-get-modules` 等明确排除了 `android`；
- `libigl` 默认以 `!windows & !linux & !osx & !android` 排除桌面与 Android，仅在 iOS 上启用；
- `sqlpp11` 在 android/ios 上只启用 `sqlite3` feature，在 desktop 上启用 `sqlite3, mysql, postgresql`。

### 3.2 Copyleft 许可门控（feature gate）

项目采用 vcpkg 的 `features` 机制实现许可门控：
- 顶层 `license: "MIT"`，同时定义 `copyleft` feature，其 `license: "GPL-3.0-or-later"`，并在 `default-features` 中对 `windows | linux | osx | android` 启用该 feature。
- `copyleft` feature 引入 `cgal`、`libigl[cgal]`、`opencascade` 三个 copyleft 内核。
- 根 `CMakeLists.txt` 暴露三态缓存变量 `HSBA_COPL`（AUTO/ON/OFF），逻辑为：
  1. AUTO 时优先检查环境变量 `VCPKG_MANIFEST_FEATURES` 是否包含 `copyleft`；
  2. 否则检查 `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` 是否为真；
  3. 最后探测 CGAL/OpenCascade 是否已安装（因为 vcpkg 的 default-features 不会出现在 `VCPKG_MANIFEST_FEATURES` 中）。
- 当 `HSBA_COPL` 为真时，定义编译宏 `HSBA_COPL`，并通过 `add_compile_definitions(HSBA_COPL)` 暴露给源码；同时在 CMake 层有条件地 find_package(CGAL/OpenCASCADE) 并链接。
- 文档 `docs/en/vcpkg-dependencies.md` 明确说明：启用任意 copyleft kernel 会使整个工程以 GPL-3.0-or-later 发布，禁用则维持 MIT。

### 3.3 Registry 与覆盖机制

`vcpkg-configuration.json` 固定了 vcpkg 官方 catalog 的 commit baseline `04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4`，并注册 Microsoft artifact registry（`https://github.com/microsoft/vcpkg-ce-catalog/archive/refs/heads/main.zip`）。同时启用 `overlay-ports` 和 `overlay-triplets` 指向仓库内的 `./overlay-ports` 与 `./triplets` 目录，用于本地覆盖或新增 port/triplet。

### 3.4 移动端依赖差异

文档 `docs/en/vcpkg-dependencies.md` 维护了平台依赖矩阵，明确 Android 与 iOS 下被排除的包及其原因（如 boost-locale/log/dll/nowide 在 Android 存在兼容性问题、OpenCV 在 iOS 构建复杂、OpenCASCADE 不适合移动平台、sqlpp11 MySQL/PostgreSQL 不支持交叉编译等）。

## 4. 约定与约束

- **依赖集中声明于 `vcpkg.json`**：所有第三方库均通过 vcpkg 清单声明，未观察到 `third_parts_without_vcpkg` 之外的 vendored 源码目录（该目录为空）。
- **平台兼容性通过 `platform` 表达式表达**：依赖的平台限制写在 `vcpkg.json` 中，而非分散在各模块的 `find_package` 调用里。
- **Copyleft 内核必须通过 `copyleft` feature 启用**：`vcpkg.json` 将 CGAL/libigl[opencascade] 置于 `features.copyleft.dependencies` 之下；根 `CMakeLists.txt` 通过 `HSBA_COPL` 三态变量决定是否查找并链接这些库，且定义了 `HSBA_COPL` 编译宏供源码分支。
- **Registry baseline 固定**：`vcpkg-configuration.json` 锁定了 vcpkg 官方 catalog 的 git commit，确保可重现构建。
- **Overlay ports/triplets 预留扩展点**：`overlay-ports/README.md` 与 `triplets/README.md` 分别说明在此处添加自定义 port 与 triplet，与 vcpkg 官方文档约定的 Nintendo Switch2/RISC-V 模式一致。
- **Android/iOS 依赖子集化**：通过 `platform` 字段与 `sqlpp11` 的 per-platform features 实现移动端最小依赖集；文档中的依赖矩阵是这一策略的权威说明。
- **Android 示例不直接管理原生依赖**：`android/build.gradle` 仅声明 AGP 版本，原生库由外部预构建后通过 JNI 加载，依赖解析不在 Gradle 侧完成。

## 5. 未观察到的内容

- 未发现私有 vcpkg registry 或 `VCPKG_BINARY_SOURCES` 配置，依赖全部来自 vcpkg 官方 catalog 与 Microsoft artifact registry。
- 未发现 `go.mod`/`package.json`/`Cargo.toml` 等其他语言包管理器文件。
- 未发现 `vendor/` 目录或手动拷贝的第三方源码（除空目录 `third_parts_without_vcpkg` 外）。