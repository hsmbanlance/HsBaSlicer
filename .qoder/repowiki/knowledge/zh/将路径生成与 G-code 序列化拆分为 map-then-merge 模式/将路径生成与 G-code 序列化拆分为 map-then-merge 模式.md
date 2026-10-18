---
kind: design
name: 将路径生成与 G-code 序列化拆分为 map-then-merge 模式
source: session
category: adr
---

# 将路径生成与 G-code 序列化拆分为 map-then-merge 模式

_来源：d9ffcdd → ca9d258 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
GenerateGCodePathV2、GenerateGCodePath、ToGCode 当前在单层循环内既做几何计算又做 path->push_back / ss << 拼接，后者对象非线程安全，无法直接并行。

## 决策驱动
- 让几何计算并行化
- 保持 GCodePath / ostringstream 写入顺序正确
- 避免大对象跨线程拷贝
- 控制内存峰值

## 备选方案
- **ParallelMapLayers 返回每层结果向量，主线程按序合并** — 优点：GCodePath 非线程安全被规避；ToGCode 用 std::string + reserve 预估大小减少 realloc；Combine 阶段在主线程完成保证顺序
- **给 GCodePath 加内部互斥锁使其线程安全** _（已否决）_ — 优点：可直接 push_back 并行；缺点：锁竞争成为瓶颈；改变 GCodePath 语义；调试困难

## 决策
GenerateGCodePathV2：ParallelMapLayers 返回 vector<pair<string, PolygonsD>>，主线程按序 path->push_back；GenerateGCodePath：返回 vector<vector<GPoint>> 三段后主线程 insert；ToGCode：ParallelMapLayers<int> 返回 vector<string> bodies，主线程 ss << header + join(bodies) + footer，并用 reserve 预分配。

## 影响
几何计算阶段获得并行加速，合并阶段回到主线程保证顺序；ToGCode 从 ostringstream 换为 string + reserve 以降低扩容开销。GenerateGCodePathSpiral 因单条螺旋本身串行而跳过并行。