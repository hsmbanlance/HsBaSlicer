---
kind: external_dependency
name: vcpkg 依赖管理（manifest 模式 + 二进制缓存）
slug: vcpkg
category: external_dependency
category_hints:
    - client_constraint
scope:
    - '**'
source_files:
    - vcpkg.json
    - CMakePresets.json
    - README.md
---

### 身份与角色
项目通过 `vcpkg.json` manifest 声明第三方依赖，并以 `CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` 接入 CMake。所有预设（Windows/Linux/macOS/Android/iOS）统一走 vcpkg。

### 关键约束
- **manifest 模式**：`$schema` 指向 microsoft/vcpkg-tool schema，依赖版本由仓库锁定；CI 使用 ubuntu-latest 的托管 runner。
- **平台条件化依赖**：bit7z/boost-* / fontconfig / opencv / openvdb/sqlpp11 在 windows|linux|osx 上启用，android|ios 仅保留 sqlite3 子集；libigl 在非桌面平台被排除。
- **二进制缓存**：WSL 本地 vcpkg 有 ~52GB 二进制缓存，CI 首次冷编译 OCCT/CGAL/VTK 等重依赖耗时较长但稳定（见 coverage CI OOM 修复后验证）。
- **Android NDK 集成**：`android-release` preset 通过 `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` 加载 Android toolchain，triplet `arm64-android`。

### 已知坑
- PrusaSlicer 遗留的依赖查找方式会污染 CMake 缓存，需删除旧 build 目录才能正常配置（q70/q71 中确认）。
- WSL 下 git registry fetch 可能被本机 MITM（SteamTools）拦截证书，需在 Windows 信任库导出 CA bundle 或通过本地完整克隆补齐注册表对象库。

verify exact dependency versions against the pinned manifest in `vcpkg.json`.