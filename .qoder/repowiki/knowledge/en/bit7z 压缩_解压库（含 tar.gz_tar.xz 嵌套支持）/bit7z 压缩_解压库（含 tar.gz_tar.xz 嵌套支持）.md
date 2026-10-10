---
kind: external_dependency
name: bit7z 压缩/解压库（含 tar.gz/tar.xz 嵌套支持）
slug: bit7z
category: external_dependency
category_hints:
    - sdk_real_api
scope:
    - '**'
source_files:
    - vcpkg.json
    - fileoperator/bit7z_def.hpp
    - fileoperator/bit7z_zipper.cpp
    - fileoperator/bit7z_unzipper.cpp
---

### 身份与角色
- vcpkg 依赖 `bit7z`，通过 `fileoperator/bit7z_def.hpp` 暴露 `Bit7zZipper` / `Bit7ZUnzipper`，作为项目统一的 ZIP/TAR/GZIP/XZ 封装层。
- 除标准 zip 外，额外支持 `TarGz` / `TarXz`：压缩时先在内存中打包 tar 再 gzip/xz 一次写出；解压时对 `.tar.gz/.tgz/.tar.xz/.txz` 识别后把内层 tar 透明解到系统临时文件，再以常规 reader 读取条目。

### 集成要点
- 格式枚举新增 `TarGz` / `TarXz`，Lua 侧通过字符串 `"TarGz"` / `"TarXz"` 传入 `Bit7zZipper.new`。
- 解压路径在析构/重开时清理临时 tar 文件。

### 已知约束
- 非 OpenCV 光栅化走 libpng + libjpeg-turbo（`libpng png_set_write_fn` / `tjCompress2`），OpenCV 分支走 `cv::imencode`。
- Windows 上需链接 `oleaut32.lib` / `user32.lib`（7z.dll 依赖）。