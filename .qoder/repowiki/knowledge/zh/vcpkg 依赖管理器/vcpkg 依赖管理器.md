---
kind: external_dependency
name: vcpkg 依赖管理器
slug: vcpkg
category: external_dependency
category_hints:
    - migration_status
scope:
    - '**'
source_files:
    - vcpkg.json
    - CMakeLists.txt
---

### 身份与用途
- 项目的官方依赖来源（README 推荐方式），通过 `vcpkg.json` 声明全部第三方库，构建时通过 `-DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake` 注入。

### 迁移状态
- 历史曾出现 PrusaSlicer 子模块污染依赖查找的问题（会话 q98-q99 记录），最终通过清理本地 PrusaSlicer 编译产物后恢复；当前仓库不再包含 PrusaSlicer 子模块，依赖完全由根 `vcpkg.json` 管理。