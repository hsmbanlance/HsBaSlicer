---
kind: external_dependency
name: OpenCascade B-Rep 内核（copyleft 特性开关）
slug: opencascade
category: external_dependency
category_hints:
    - client_constraint
scope:
    - '**'
source_files:
    - vcpkg.json
    - README.md
    - cadmodel/OcctModel.cpp
---

### 身份与角色
- vcpkg feature `copyleft` 引入 OpenCascade（LGPL-2.1-only），通过 `cadmodel/OcctModel.cpp` 提供 B-Rep CAD 模型后端。
- Android/iOS/游戏主机平台默认不安装 `HsBaSlicerCADModel`（缺少 OCCT 支持）。

### 客户端约束
- 与 CGAL 一样，启用后触发项目级 GPL-3.0-or-later 许可切换。
- 桌面平台（Windows/Linux/macOS）全部库目标参与安装；移动/主机平台排除该模块。