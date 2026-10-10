---
kind: external_dependency
name: GitHub Actions 持续集成
slug: github-actions
category: external_dependency
category_hints:
    - client_constraint
scope:
    - '**'
source_files:
    - .github/workflows/build-windows.yml
    - .github/workflows/build-macos.yml
    - .github/workflows/build-ios.yml
    - .github/workflows/build-android.yml
    - .github/workflows/test-coverage.yml
    - .github/workflows/publish-wiki.yml
---

工作流覆盖 Windows/macOS/Linux/Android/iOS 构建以及覆盖率测试和 Wiki 发布。覆盖率作业曾出现 hosted runner lost communication 问题，已改为 pull request 自动触发、push 手动触发；覆盖率门限为核心 ≥80%、其他 ≥60%。Wiki 发布通过 `publish-wiki.yml` 调用 `scripts/repowiki_to_github_wiki.py` 将 `.qoder/repowiki/{en,zh}/content` 扁平化后推送到 GitHub Wiki。