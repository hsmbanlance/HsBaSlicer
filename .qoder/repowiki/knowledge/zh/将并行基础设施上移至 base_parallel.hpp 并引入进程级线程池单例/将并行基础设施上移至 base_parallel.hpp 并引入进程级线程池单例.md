---
kind: design
name: 将并行基础设施上移至 base/parallel.hpp 并引入进程级线程池单例
source: session
category: adr
---

# 将并行基础设施上移至 base/parallel.hpp 并引入进程级线程池单例

_来源：d9ffcdd → ca9d258 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
support、paths 等库目录当前不依赖 DllHsBaSlicer，但新的支撑/路径/G-code 序列化并行需要共享的 ParallelForLayers / ParallelMapLayers；若保留在 DllHsBaSlicer/pipeline_parallel.hpp 会形成反向依赖。同时每次调用 ThreadPool(n) 启停与固定分块带来尾延迟。

## 决策驱动
- 消除 support/paths 对 DllHsBaSlicer 的反向依赖
- 复用线程池避免重复创建开销
- 保持 Lua 解释器串行约束
- 跨 Windows/Android/iOS 三平台一致行为

## 备选方案
- **将 pipeline_parallel.hpp 上移到 base/parallel.hpp（header-only + 命名空间别名转发）** — 优点：支持、paths、DllHsBaSlicer 统一引用同一实现；无额外编译单元；旧 include 可薄转发降低改动面
- **在 support/paths 各自复制一份并行实现** _（已否决）_ — 优点：彻底解耦；缺点：维护多份代码；无法共享线程池；违反单一事实原则

## 决策
把 pipeline_parallel.hpp 迁移到 base/parallel.hpp，暴露 HsBa::Slicer::Pipeline::ParallelForLayers / ParallelMapLayers；原路径保留为薄转发头。新增 inline PipelinePool() 返回 static 线程池单例，size 取自 HSBA_PIPELINE_THREADS，未设置时回退 hardware_concurrency()，生命周期至进程退出。

## 影响
support、paths、所有 *_pipeline.cpp 都通过 base/parallel.hpp 获取并行能力；Lua 支撑/填充仍走 ISupport::GenerateAllLuaSupport 分支保持串行；后续新增任意层间并行阶段只需调用 ParallelMapLayers，无需再改调度器。