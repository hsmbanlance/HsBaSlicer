# HsBaSlicer Wiki 维护指导（中英对照一致性 & 有效内容最大化）

[English version](../en/wiki_maintenance_guide.md)

本指南规定如何维护本项目 Wiki，使 **中文（`zh`）与英文（`en`）内容保持对照一致**，并在发布到
GitHub Wiki 时**最大化有效信息密度**。Wiki 内容源为 Qoder IDE 生成的 `.qoder/repowiki/{en,zh}`，
经 [`scripts/repowiki_to_github_wiki.py`](../../scripts/repowiki_to_github_wiki.py) 扁平化后由
[`.github/workflows/publish-wiki.yml`](../../.github/workflows/publish-wiki.yml) 发布。再生成时的强约束通过官方前置干预配置
[`.qoder/repowiki/wiki_plan.yaml`](../../.qoder/repowiki/wiki_plan.yaml) 注入（见
[Repo Wiki 官方文档](https://help.aliyun.com/zh/lingma/repo-wiki)「干预 Wiki 生成」）：`repowiki.notes`/`knowledgecard.notes`
携带对照、去重与有效内容标准，`scope.exclude` 裁剪噪声，`repowiki.template` 取 `architecture`。

## 1. 问题现状（为什么中英不相似）

| 维度 | EN（96 页） | ZH（116 页） |
|------|-------------|--------------|
| 顶层分类数 | 10 类 + 1 根散页 | 8 类 + 4 根散页 |
| 分类体系 | Advanced Features / API Reference / **Architecture Overview** / Core Concepts / **Core Library Documentation** / Development Guide / Examples and Samples / **Lua Scripting System** / Platform Integration / Troubleshooting and FAQ | 部署和维护 / 高级主题 / 核心概念 / 核心模块详解 / 集成指南 / 扩展开发指南 / 示例和教程 / API 参考文档 |
| 根级散页 | `Getting Started.md` | `附录.md`、`贡献指南.md`、`快速开始.md`、`项目概述.md` |

核心病因：

1. **两侧用了不同的分类骨架**——EN 有 `Architecture Overview`、`Core Library Documentation`、
   `Lua Scripting System` 三个独立类，ZH 没有对应类；ZH 有 `部署和维护`、`扩展开发指南`，EN 把它们打散。
2. **页面数不等（96 vs 116）**——约 20 页无法 1:1 配对。
3. **ZH 内部重复**——`网格模型实现` 同时出现在 `核心概念/模型抽象接口` 与 `核心模块详解/模型处理系统`；
   `填充算法` 同时出现在 `核心模块详解/2D 几何算法` 与 `核心模块详解/填充系统`。重复页拉低有效内容比。
4. **根级散页游离于分类外**——发布脚本的侧栏分组依赖"文件夹名==索引页标题"的规律，散页会落到
   uncategorized，破坏中英侧栏结构对称。

## 2. 目标：统一分类骨架（中立合并基准）

中英共用下面 **13 个顶层分类**（仅标题/正文语言不同），采用**中立合并**：某主题以更完整、更贴合代码
的一侧为基准，再改写对齐到另一侧。每个分类文件夹必须含一个索引页，其 H1 与文件夹同名。

| # | EN 分类 | ZH 分类 | 覆盖范围 |
|---|---------|---------|----------|
| 1 | Overview & Getting Started | 项目概述与快速开始 | 项目简介、快速开始、学习路径 |
| 2 | Architecture Overview | 架构总览 | 系统架构模式、组件与数据流、流水线编排、部署模式 |
| 3 | Core Concepts | 核心概念 | 3D 打印技术概述、关键数据结构、切片原理、流水线架构模式 |
| 4 | Core Modules | 核心模块详解 | 2D 几何、模型处理、切片引擎、填充系统、支撑系统、路径规划 |
| 5 | Advanced Features | 高级主题 | 高级网格处理、填充/路径优化策略、支撑算法、性能调优 |
| 6 | Lua Scripting System | Lua 脚本系统 | 环境与 API、自定义流水线编排、阶段钩子、Lua 调试 |
| 7 | API Reference | API 参考文档 | C ABI（Dll）、C++ API（Lib）、C++20 模块接口、集成协议 |
| 8 | Integration Guide | 集成指南 | 桌面（Qt/wx）、移动（Android/iOS）、游戏引擎（Unity/UE）、Web、第三方 |
| 9 | Development, Testing & Contribution | 开发·测试与贡献 | 环境搭建、构建系统、单元/静态/覆盖率测试、代码质量、贡献流程 |
| 10 | Extension Development | 扩展开发指南 | 插件架构、事件系统、自定义算法开发 |
| 11 | Examples & Samples | 示例与教程 | 基础/高级示例、按工艺分类的样例、生产集成模式 |
| 12 | Deployment, Troubleshooting & FAQ | 部署·故障排除与FAQ | 部署配置、监控与维护、版本管理、构建/运行问题、FAQ |
| 13 | Appendix | 附录 | 术语表、cipher 模块、横向参考 |

> 若 IDE 无法精确产出 13 类，保留 1–12 并将「附录」并入第 12 类；**任何根级散页都必须归入某分类**，
> 不允许游离。

## 3. 现有分类 → 统一分类映射（迁移指引）

**EN 侧：**

| 现有 EN 分类（页数） | 迁移到统一分类 |
|---|---|
| Architecture Overview (5) | #2 架构总览（保留） |
| Core Library Documentation (9) | #4 Core Modules（与 ZH 核心模块详解合并） |
| Lua Scripting System (5) | #6（保留） |
| Development Guide (10) | #9（测试与贡献并入） |
| Platform Integration (5) | #8 Integration Guide（改名对齐） |
| Troubleshooting and FAQ (5) | #12（与 ZH 部署和维护合并） |
| Getting Started.md（根散页） | #1 |

**ZH 侧：**

| 现有 ZH 分类（页数） | 迁移到统一分类 |
|---|---|
| 部署和维护 (5) | #12（与 EN Troubleshooting 合并） |
| 高级主题 (16) | #5（性能/测试/错误处理拆分：测试与代码质量→#9，错误处理→#12） |
| 核心概念 (16) | #3；其中 `模型抽象接口/网格模型实现` 去重后移入 #4 |
| 核心模块详解 (36) | #4；`2D 几何算法/填充算法` 与 `填充系统` 合并为单一填充页 |
| 集成指南 (12) | #8（保留） |
| 扩展开发指南 (6) | #10（保留；`调试工具和技巧` 若与 #12 重复则去重） |
| 示例和教程 (5) | #11（保留） |
| API 参考文档 (16) | #7（保留） |
| 附录.md / 贡献指南.md / 快速开始.md / 项目概述.md（根散页） | 分别→ #13 / #9 / #1 / #1 |

## 4. 有效内容标准（最大化信息密度）

每篇页面**必须**：

1. 以**单个 `# H1`** 标题开头——发布脚本取首个 H1 作为侧栏链接文本。
2. 内容**基于源码实证**：真实的函数签名、配置字段与默认值，取自
   `pipelinetypes/pipeline_types.h`、各 `*_pipeline.h`、`LuaSupport.cpp`、`PolygonFill.cpp` 等；
   **严禁臆造 API 或默认值**。
3. 至少含 **1 张 mermaid 图或 1 个表格**，且提供增量信息而非复述。
4. **非占位**：有效行 < 约 30 行的薄页应向上合并到父页，不单独立页。
5. 页脚**互链**到另一语言的对应页面（`Home` ↔ `Home-zh` 由脚本自动生成，页面级互链需手工补）。

发布时噪声处理：源码中可保留 repowiki 自带的 `<cite>`、`**Section sources**`、`**Diagram sources**`、
`file://` 链接，[`clean_content`](../../scripts/repowiki_to_github_wiki.py) 会在发布时自动剥离/降级；
**不要新增**清理器无法识别的其他噪声标记。

## 5. 页面 1:1 配对与命名

- `en/content/<类>/<页>.md` 必须在 `zh/content/<对应类>/<对应页>.md` 有兄弟页，两侧总数相等。
- 维护一张 EN↔ZH 文件名映射，保证扁平化后的 Wiki 页名可跨语言寻址。
- 分类索引页 H1 == 文件夹名（脚本靠 `X/X` 重复或公共前缀识别分组），否则侧栏分组会错乱。

## 6. 再生成与发布流程

1. **在 Qoder IDE 中**编辑 `.qoder/repowiki/wiki_plan.yaml`（官方前置干预配置），分别对 `en`、`zh` 从语言下拉框选择目标语言
   并触发「生成 / 重新生成」；修改 `wiki_plan.yaml` 后需**手动**重新生成才会生效（CLI 无法生成）。
   再生成可先用 `/knowledge` 命令对已有页面做局部修改/补充/重写，人工修订会被标记保护、不被下次自动更新覆盖。
2. 提交前用下面清单校验一致性。
3. `git add .qoder/repowiki` → commit → push 到工作分支。
4. 合并到 `master`，`publish-wiki.yml` 自动发布扁平化 Wiki 到 GitHub Wiki。

### 校验清单

- [ ] `en` 页面数 == `zh` 页面数
- [ ] 每个统一分类在两侧都存在，且各含索引页
- [ ] 无跨分类重复概念（尤其 `网格模型实现`、`填充算法`）
- [ ] 无游离根级散页
- [ ] 每页以单个 `# H1` 开头、含至少一图/表、内容为源码实证
- [ ] 本地演练：`python scripts/repowiki_to_github_wiki.py --lang both --src .qoder/repowiki --out wiki-output`
      后检查 `wiki-output/_Sidebar.md` 与 `_Sidebar-zh.md` 分组对称

可用下面 PowerShell 快速核对两侧顶层分类与页数：

```powershell
foreach ($l in 'en','zh') {
  Write-Output "==== $l ===="
  Get-ChildItem ".qoder/repowiki/$l/content" | ForEach-Object {
    if ($_.PSIsContainer) { "{0,-40}{1}" -f $_.Name, (Get-ChildItem -Recurse -Filter *.md $_.FullName).Count }
    else { "{0,-40}1" -f "[根]$($_.Name)" }
  }
}
```

## 7. `/knowledge` 干预手册（不重新生成，直接修正现有 Wiki）

`/knowledge` 支持 **生成 / 修改 / 补充 / 重写** 四种操作；人工修订会被系统标记保护，不会被下一次自动更新覆盖。
在输入框唤起 `/knowledge`，粘贴下列指令，可上传 `.qoder/repowiki/wiki_plan.yaml` 或本指南作为参考文件。

> **策略修正**：EN 分类名混入中文、退化为根散页的文件名污染**已修复**（EN 现为 128 个纯英文页面，内容完整）。
> 后续采用**中立合并**：逐分类保留两侧中更完整的一侧作为基准，**不删除任何一侧**，而是把较弱一侧补齐/翻译到统一结构。

### A. 以较强一侧为基准，补齐较弱一侧

- **以 ZH 为基准的更完整分类**（核心概念 22、Lua 32、API 36、扩展开发 8）：向 **en** 补齐——
  > 以 zh 对应分类的页面清单为基准，为 en 缺失的主题补充/重写实质性英文页面，使 en 与 zh 该分类叶子页一一对应。
- **以 EN 为基准的更完整分类**（Core Modules 40、Examples 18、Advanced 9、Integration 14）：向 **zh** 补齐——
  > 以 en 对应分类为基准，为 zh 缺失的主题补充/重写实质性中文页面，使两侧该分类叶子页数一一对应。
- **两侧均为单页的分类**（Overview/Architecture/Dev-Test/Deployment/Appendix）：保持精简，不强拆子页。

### B. 仅合并真正的薄页（不是“因为页多就删”）

> 只有当某页有效行 < ~30 行且与同目录页高度重叠时，才合并或删除；若 ZH 的 API/Lua 页面是实质性更完整内容，
> 则它们是**基准侧**，应反过来补齐 EN，而不是被精简掉。

### C. 双侧通用约束（每次 /knowledge 都应附带）

- 每页首行为单一 `# H1`；正文基于源码实证；每页至少一图或一表；页脚互链另一语言对应页面。
- 全部修正后重跑第 6 节的核对脚本，预期 `en 页数 == zh 页数`、两侧分类对称、无根散页。
