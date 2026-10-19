---
kind: external_dependency
name: GitHub Actions — CI for coverage gating
slug: github-actions
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - .github/workflows/test-coverage.yml
    - CMakePresets.json
---

### Identity
GitHub-hosted Ubuntu runners execute the coverage pipeline defined in `.github/workflows/test-coverage.yml`.

### Role in this repo

### Stable integration shape
- Do not change the preset names (`linux-coverage`, `linux-coverage-report`) — the workflow hard-codes them.
- Coverage thresholds are CACHE variables `HSBA_COVERAGE_THRESHOLD_CORE` / `HSBA_COVERAGE_THRESHOLD_OTHER` overridden by the preset; adjust them in `CMakePresets.json`, not in the workflow.