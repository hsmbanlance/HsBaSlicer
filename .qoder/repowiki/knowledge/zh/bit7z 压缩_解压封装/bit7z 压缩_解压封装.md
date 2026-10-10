---
kind: external_dependency
name: bit7z 压缩/解压封装
slug: bit7z
category: external_dependency
category_hints:
    - sdk_real_api
scope:
    - '**'
source_files:
    - fileoperator/bit7z_zipper.cpp
    - fileoperator/bit7z_unzipper.cpp
    - fileoperator/bit7z_def.hpp
---

项目对 bit7z 做了两层扩展：
- 已知上游限制：`BitNestedArchiveReader` 的 map 提取存在顺序模式条目数溢出查询越界缺陷，因此统一采用"外层条目 → 临时 tar → 常规 reader"方案而非 subfile 流；7z.dll 不支持 GZip/XZ 的 subfile 构造。
- Lua 侧接受 `"TarGz"`/`"TarXz"` 格式串。