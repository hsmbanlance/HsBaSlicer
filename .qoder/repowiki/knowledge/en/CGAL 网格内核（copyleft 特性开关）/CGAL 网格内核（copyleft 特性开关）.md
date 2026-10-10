---
kind: external_dependency
name: CGAL 网格内核（copyleft 特性开关）
slug: cgal
category: external_dependency
category_hints:
    - client_constraint
scope:
    - '**'
source_files:
    - vcpkg.json
    - README.md
    - meshmodel/CgalModel.cpp
---

### 身份与角色
- vcpkg feature `copyleft` 引入 CGAL（GPL-3.0-or-later）与 libigl（带 cgal 特性），启用后项目整体许可切换为 GPL-3.0-or-later。
- 通过 `meshmodel/CgalModel.cpp` 提供基于 CGAL 的网格模型后端，配合 `FullTopoModel` 做拓扑切片。

### 客户端约束
- 构建是否包含 CGAL 决定最终许可证；iOS/游戏主机等不启用 copyleft 的平台走 MIT 许可。
- 顶层 CMakeLists 未开启 IPO/LTO（`CMAKE_INTERPROCEDURAL_OPTIMIZATION`），对模板密集几何热路径是潜在优化空间。