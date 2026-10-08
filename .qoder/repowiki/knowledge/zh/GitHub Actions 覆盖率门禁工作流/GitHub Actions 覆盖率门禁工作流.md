---
kind: external_dependency
name: GitHub Actions 覆盖率门禁工作流
slug: github-actions
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - .github/workflows/test-coverage.yml
    - cmake/coverage.cmake
    - CMakePresets.json
---

### 身份与角色
`.github/workflows/test-coverage.yml` 是聚合型覆盖率 CI，在 GitHub-hosted `ubuntu-latest` runner 上执行 `cmake --preset linux-coverage` → `ctest --preset linux-coverage` → `gcovr` 门控。

### 触发策略
- `pull_request`（branches: master, dev）自动触发。
- `workflow_dispatch` 手动触发。
- 已移除 `push` 自动触发，避免对 master/dev 直接推送重复跑插桩构建。

### 内存安全约定
- job 级 env：`CMAKE_BUILD_PARALLEL_LEVEL=2` + `VCPKG_MAX_CONCURRENCY=2`，防止 Ninja/vcpkg 满核并行撑爆 16GB runner。
- `swap-size-mb: 8192` 兜底 OOM。
- `-DHSBA_COVERAGE_JOBS=2` 限制 gcovr 报告阶段并发。
- 阈值门控：core ≥ 45%，other ≥ 25%（写入 Job Summary）。阈值在配置期写死进 `build.ninja`，变更必须重新 `cmake --preset`。

### 行为要点
- 覆盖率数据来自 ctest 产出的 `.gcda`，由 `cmake/coverage.cmake` 中的 `coverage` 目标驱动。
- 核心模块过滤器 = base/2D/utils/cipher/meshmodel/paths/preprocess/support/LibHsBaSlicer；static_tests 不参与覆盖率（`HSBA_SLICER_USE_STATIC_TESTS=OFF`）。
- HTML/XML 报告作为 artifact 上传。