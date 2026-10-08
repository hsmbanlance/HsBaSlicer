---
kind: external_dependency
name: Bit7z 压缩/解压封装（含 tar.gz/tar.xz 支持）
slug: bit7z
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - fileoperator/bit7z_zipper.cpp
    - fileoperator/bit7z_unzipper.cpp
    - fileoperator/bit7z_def.hpp
    - fileoperator/LuaAdapter.cpp
---

### 身份与角色
通过 vcpkg 引入的 bit7z 库，封装为 `Bit7zZipper` / `Bit7ZUnzipper`，对外暴露 ZIP/TarGz/TarXz 的统一 Zipper/Unzipper 接口，并注册到 Lua 适配器。

### 能力
- 同时支持 `Bit7zExtract` 一步解压到目录或内存 map。

### 实现约束
- 修复了既有 bug：原 `archiver_path_` 成员陈旧而非传入 path，首次打开为空失败；进度事件 `currentFileIndex` 从不递增。

verify format strings and temp-file cleanup behavior against current implementation.