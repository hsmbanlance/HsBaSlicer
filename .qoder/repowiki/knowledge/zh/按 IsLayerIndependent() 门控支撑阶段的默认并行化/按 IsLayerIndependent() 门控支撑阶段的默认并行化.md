---
kind: design
name: 按 IsLayerIndependent() 门控支撑阶段的默认并行化
source: session
category: adr
---

# 按 IsLayerIndependent() 门控支撑阶段的默认并行化

_来源：d9ffcdd → ca9d258 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
FdmPlaneSupport/FdmHoneycombSupport/SlaSacrificialSupport 各层独立，可安全并行；FdmTreeSupport 未来可能跨层生长，不能直接并行。需要在基类提供开关而不侵入具体算法。

## 决策驱动
- 允许不同支撑策略声明自身独立性
- 默认并行以最大化吞吐
- Lua 支撑保持串行
- 不影响已有 FDM/SLA 流水线调用方

## 备选方案
- **ISupport::IsLayerIndependent() 虚函数，默认 true，子类覆写** — 优点：最小侵入；FdmTreeSupport 可标记 false 走串行回退；LuaSupport 已单独分支不受影响
- **在 ISupport::GenerateAll 中硬编码分支判断具体子类** _（已否决）_ — 优点：一次性集中逻辑；缺点：新增支撑类型都要改基类；破坏开闭原则

## 决策
在 ISupport.hpp 增加 virtual bool IsLayerIndependent() const { return true; }，FdmPlaneSupport/FdmHoneycombSupport/SlaSacrificialSupport 继承默认 true，FdmTreeSupport 未来接入跨层生长时覆写为 false。ISupport.cpp 的 GenerateAll 默认走 ParallelMapLayers<R>(R=PolygonsD)，false 时回退串行。

## 影响
新增支撑类型默认即并行，除非显式标记跨层依赖；LuaSupport::GenerateAll 不被覆盖，fdm_pipeline/sla_pipeline 的 GenerateAllLuaSupport 分支维持现状。测试需在 tests/Support/parallel_equivalence_test.cpp 中验证 true/false 两条路径输出等价。