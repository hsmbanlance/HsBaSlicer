---
kind: external_dependency
name: GitHub Actions 持续集成
slug: github-actions
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - .github/workflows/test-coverage.yml
---

### 身份与用途
- 项目的 CI/CD 编排平台，工作流位于 `.github/workflows/`，覆盖 Linux/macOS/Windows/iOS/Android 构建、覆盖率门禁、Wiki 发布。

### 稳定行为
- 门控阈值：Core ≥ 45%、Other ≥ 25%，通过 `cmake --build --preset linux-coverage-report` 执行，结果写入 Job Summary 并产出 HTML/XML artifact。