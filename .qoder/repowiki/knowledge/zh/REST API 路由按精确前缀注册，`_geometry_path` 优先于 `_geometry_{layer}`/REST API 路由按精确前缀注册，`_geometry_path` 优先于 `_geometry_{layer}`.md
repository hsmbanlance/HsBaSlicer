---
kind: design
name: REST API 路由按精确前缀注册，`/geometry/path` 优先于 `/geometry/{layer}`
source: session
category: adr
---

# REST API 路由按精确前缀注册，`/geometry/path` 优先于 `/geometry/{layer}`

_来源：68c42f0 → c513cb3 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
httplib 按注册顺序匹配正则，已有通配 `/api/jobs/([^/]+)/geometry/([^/]+)` 会把 `path` 误当成层号校验吞掉，导致新端点永远 400。

## 决策驱动
- URL 语义正确性
- 避免隐式依赖注册顺序注释
- 保持既有层号路由不变

## 备选方案
- **在 `RegisterRoutes` 中紧接 cancel 之后、layer 之前注册 `/geometry/path`** — 优点：最小改动；显式注释说明 `path` 是保留段；缺点：仍需维护注册顺序
- **改用 httplib 子路由器或显式白名单匹配** _（已否决）_ — 优点：从语法上杜绝吞掉；缺点：重构现有路由结构，改动面大

## 决策
在 `routes.cpp` 的 `RegisterRoutes` 中，紧跟 993 的 cancel 之后、994 的 layer 之前注册 `svr.Get(R"(/api/jobs/([^/]+)/geometry/path)", ...)`，并在 990-991 的顺序注释里写明 `geometry/path` 是保留段。

## 影响
HTTP 测试锁住该顺序；新增路由走独立 handler，错误阶梯与 `HandleJobGeometry` 一致（job not found → captured no geometry → snapshot unavailable → no continuous path）。