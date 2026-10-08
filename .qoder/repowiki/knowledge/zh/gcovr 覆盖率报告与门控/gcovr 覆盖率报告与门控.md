---
kind: external_dependency
name: gcovr 覆盖率报告与门控
slug: gcovr
category: external_dependency
category_hints:
    - sdk_real_api
    - framework_behavior
scope:
    - '**'
source_files:
    - cmake/coverage.cmake
---

### 身份与角色
用于解析 gcc 插桩产出的 `.gcno/.gcda`，按 core/other 两组 filter 计算覆盖率并执行门控（`--fail-under-line`）。

### 正确用法
- 通过 `HSBA_COVERAGE_JOBS` 控制并行度（显式传参时无条件应用；自动探测兼容 gcovr 7.x/8.x 的 `-j` 帮助文本）。
- 阈值以 CACHE 变量 `HSBA_COVERAGE_THRESHOLD_CORE` / `HSBA_COVERAGE_THRESHOLD_OTHER` 传入，配置期写死进 `build.ninja`——修改阈值必须重新 configure。
- 使用绝对路径 filter/exclude 匹配 tracefile 内路径；`--add-tracefile` 模式下 gcovr 8.6 按绝对路径匹配。
- 输出 `core_summary.txt` / `other_summary.txt` 供 workflow summary 步骤读取。

### 已知坑
- gcovr 8.6 的帮助文本是 `-j [GCOV_PARALLEL]`（无 `--jobs` 长参数名），旧版 `MATCHES "-j, --jobs"` 检测恒假 → 并行限流一直是惰性，必须显式传 `HSBA_COVERAGE_JOBS`。
- 合并正则字符类 `[.\[\]()...]` 在 CMake POSIX 引擎下无法编译（`]` 提前闭合括号），需用字面量 REPLACE 归一化反斜杠 + 单字符类转义元字符。
- 陈旧 `build.ninja`/`CMakeCache` 会残留旧的 filter 模式（如 `^tests//`、`base//`），导致 CORE=0% 假象，需干净重配。

verify exact command line and options against installed gcovr version.