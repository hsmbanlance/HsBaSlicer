---
kind: design
name: 用抢占式原子游标替代固定分块的 ParallelForLayers
source: session
category: adr
---

# 用抢占式原子游标替代固定分块的 ParallelForLayers

_来源：d9ffcdd → ca9d258 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
原有 ParallelForLayers 使用固定 block_size + 每块 barrier，短任务场景下尾部 worker 等待造成尾延迟；且每次调用构造 ThreadPool(n) 带来启动成本。

## 决策驱动
- 减少尾延迟
- 消除重复线程池创建
- 保持 on_progress 回调频率可控
- 异常语义与现有 try/catch(RuntimeError&) 兼容

## 备选方案
- **std::atomic<int> cursor + fetch_add 抢占取号** — 优点：worker 忙等直到越界，天然负载均衡；on_progress 通过 atomic<int> done 每 32 层节流回调；实现简单
- **继续用 fixed block_size + barrier** _（已否决）_ — 优点：实现更直观；缺点：短任务尾延迟明显；每块 barrier 同步成本高

## 决策
ParallelForLayers 内部改用 std::atomic<int> cursor，每个 worker 循环 cursor.fetch_add(1) 取层号直至越界；主线程通过 std::atomic<int> done 每 32 层触发一次 on_progress；异常仍经 future::get 重抛，上层 catch(RuntimeError&) 不变。HSBA_PIPELINE_THREADS=1 走串行 for 保证零回归。

## 影响
短任务负载更均衡，尾延迟下降；on_progress 不再是逐层回调而是批量节流，进度条更新频率降低但总次数不变。需确保 on_progress 内无锁竞争。