---
kind: external_dependency
name: vcpkg 包管理器与条件特性
slug: vcpkg
category: external_dependency
category_hints:
    - vendor_identity
scope:
    - '**'
source_files:
    - vcpkg.json
    - README.md
---

项目通过 `vcpkg.json` 声明依赖，使用 vcpkg manifest 模式（非 toolchain 文件）。关键约束：
- 平台裁剪：`opencv` 覆盖 android，`sqlpp11` 在 android/ios 仅 sqlite3，`libigl` 仅在非桌面平台（!windows & !linux & !osx & !android）启用。
- 构建说明中 Windows 推荐 Visual Studio 2022+ Desktop development with C++；Linux/macOS 示例安装命令包含 mesa/freetype/fontconfig 等系统库。
- 依赖查找曾被 PrusaSlicer 的遗留 CMake 污染，需删除旧缓存后重新配置。