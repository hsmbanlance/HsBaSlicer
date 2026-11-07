---
kind: design
name: WebUI 几何面板采用 Layer | Stack | Side | Path 四视图 + 投影预算
source: session
category: adr
---

# WebUI 几何面板采用 Layer | Stack | Side | Path 四视图 + 投影预算

_来源：68c42f0 → c513cb3 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
用户需要同时查看逐层轮廓、堆叠概览、侧视投影以及真实的螺旋出料路径；Stack/Side 若全量渲染会超出浏览器性能预算。

## 决策驱动
- DOM-free 纯函数投影
- 固定节点预算（`GEOMETRY_MAX_PATHS`）
- 顺序配色区分出料次序
- 空驶段明确标注为推断

## 备选方案
- **四个 `<button>` 切换 `geometry.view`，Stack 抽样至 12 层并按 `stackBudget` 截断每层环数** — 优点：复用现有 `ringPath`/`currentViewBox`；预算恒定；Path 用真实折线；缺点：需实现投影矩阵与 HSL→hex 色表
- **用 radio/select 控件选择视图** _（已否决）_ — 优点：语义更标准；缺点：破坏现有 `.row` 布局；交互不一致

## 决策
在 `public/index.html` 的 `.row` 内追加四个 `id="geometry-view-*"` 按钮与 `#geometry-order` checkbox；`app.js` 扩展 `geometry` 状态，`loadGeometryStack` 串行请求采样层并用 `projectedRingPaths(..., stackBudget(K))` 缓存到 `geometry.stackLayers` Map；`renderGeometry` 按 view 分派，Path 视图直接画 `xyz` 折线，Order 开关用 presentation attribute `stroke` 配合 `svg:not(.geometry-ordered)` 作用域控制颜色。

## 影响
Stack/Side 最多 12 层、每层环数受 `stackBudget` 限制；Path 视图仅在 `summary.has_path === 1` 时启用；相邻环之间的连线被 UI 明确标注为隐含空驶而非数据。