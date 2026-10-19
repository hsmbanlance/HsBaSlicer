---
kind: external_dependency
name: gcovr 覆盖率报告工具
slug: gcovr
category: external_dependency
category_hints:
    - sdk_real_api
scope:
    - '**'
source_files:
    - cmake/coverage.cmake
---

### 身份与用途
- 覆盖率门禁的执行者，配合 GCC/Clang 的 `--coverage` 插桩，解析 `.gcda/.gcno` 输出 JSON/HTML/XML 并按目录分组判定 Core/Other 阈值。

### 稳定 API 形状
- 并行选项探测兼容 gcovr 7.x (`-j, --jobs`) 与 8.x (`-j [GCOV_PARALLEL]`)；显式传入 `HSBA_COVERAGE_JOBS` 时无条件应用 `-j`。
- 过滤器/排除器使用绝对路径匹配（`--filter` / `--exclude`），且需在配置期写死进 `build.ninja`，修改阈值必须重新 `cmake --preset`。
- 已知坑：`--add-tracefile` 模式下 filter/exclude 按 tracefile 内的绝对路径匹配，相对路径拼接会失效。