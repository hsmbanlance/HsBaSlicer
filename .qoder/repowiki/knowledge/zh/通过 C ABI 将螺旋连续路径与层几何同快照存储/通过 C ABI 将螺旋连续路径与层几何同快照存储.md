---
kind: design
name: 通过 C ABI 将螺旋连续路径与层几何同快照存储
source: session
category: adr
---

# 通过 C ABI 将螺旋连续路径与层几何同快照存储

_来源：68c42f0 → c513cb3 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
WebUI 需要展示 FDM/TDP/WAAM 的螺旋（vase）出料轨迹，但现有 `GeometrySnapshot` 只存逐层环；SLA 等工艺没有 `spiral_mode`，不能把路径逻辑耦合进通用采集。

## 决策驱动
- 向后兼容：不改动 LibHsBaSlicer 公共签名
- 零拷贝导出：C ABI 一次 malloc、调用方 free 一次
- 确定性：复用已有的纯函数 `SpiralizeOuterWall`
- 可测试性：快照级断言 z 单调不减

## 备选方案
- **在 `CaptureLayerGeometry` 新增第 6 参 `path_xyz`（带默认值）** — 优点：7 处现有调用点无需改；路径与层几何共享同一快照生命周期；ABI 最小化；缺点：非螺旋工艺传入空 vector，有微小开销
- **单独开一个 `ExportGeometryPath` API 并新建快照池** _（已否决）_ — 优点：与层几何完全解耦；缺点：引入第二份引用计数/回收语义，增加内存泄漏风险

## 决策
在 `geometry_store.hpp` 给 `CaptureLayerGeometry` 追加带默认值的第 6 参 `const std::vector<double>& path_xyz = {}`，并在 `extern "C"` 尾部暴露 `HsBaGeometryPathPointCount` / `HsBaGetGeometryPath` / `HsBaFreeGeometryPath`；三个 pipeline 在 spiral 之前调用 `SpiralizeOuterWall` 扁平化为 x,y,z 三元组后传入。

## 影响
路径块与层几何共享同一 `HSBA_GEOMETRY_POOL_MAX` 回收；调用方必须严格成对调用 Get/Free；空路径时端点返回 404 `job emitted no continuous path`，前端据此禁用 Path 按钮。