---
kind: external_dependency
name: bit7z 压缩/解压库
slug: bit7z
category: external_dependency
category_hints:
    - vendor_identity
scope:
    - '**'
source_files:
    - fileoperator/bit7z_zipper.cpp
    - fileoperator/bit7z_unzipper.cpp
    - fileoperator/bit7z_def.hpp
---

### 身份与用途
- 第三方压缩库，通过 vcpkg 引入（`vcpkg.json` 声明 `bit7z`，平台限定 `windows | linux | osx`），项目内以 `HsBa::Slicer::FileOperator::Bit7zZipper` / `Bit7zUnzipper` 封装。
- 用于 ZIP/TAR.GZ/TAR.XZ 等归档的创建与解包，是 `Zipper` / `IUnzipper` 接口的一个实现；同时支持直接对 `.tar.gz` / `.tar.xz` 进行不解包式操作（会话中明确新增了对 tar.gz/tar.xz 的直接化解压/压缩）。

### 集成点
- 头文件：`fileoperator/bit7z_def.hpp`、`fileoperator/bit7z_zipper.hpp`、`fileoperator/bit7z_unzipper.hpp`。
- CMake：`fileoperator/CMakeLists.txt` 将 `bit7z` 链接到 `HsBaSlicerFileOperator`。

### 稳定用法要点
- 作为 `Zipper` / `IUnzipper` 的具体实现被上层 `file_transfer_pipeline`、`paths` 模块使用；新增的 tar.gz/tar.xz 直接处理走同一封装层，无需在调用方区分。