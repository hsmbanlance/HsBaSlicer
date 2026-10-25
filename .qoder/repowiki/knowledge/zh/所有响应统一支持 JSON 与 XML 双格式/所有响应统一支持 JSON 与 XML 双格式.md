---
kind: design
name: 所有响应统一支持 JSON 与 XML 双格式
source: session
category: adr
---

# 所有响应统一支持 JSON 与 XML 双格式

_来源：ca9d258 → ae47e03 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
不同客户端偏好不同序列化格式（curl/浏览器偏 JSON，部分遗留系统用 XML），且配置转换端点需在 proto-bytes 与两者之间互转。

## 决策驱动
- 兼容既有 XML 消费者
- 通过 struct_json/struct_xml 反射复用序列化基础设施
- 统一的错误体格式便于解析

## 备选方案
- **仅 JSON** _（已否决）_ — 优点：实现简单，现代生态首选；缺点：破坏现有 XML 使用者；proto↔XML 转换仍需实现
- **按 Accept 头自动协商** _（已否决）_ — 优点：标准做法；缺点：Accept 头常被代理/脚本忽略；?format 参数更可靠
- **?format 优先 > Accept 头 > 默认 JSON，to_response(Json|Xml) 统一输出** — 优点：显式覆盖、向后兼容、集中格式化逻辑；缺点：每个路由需走同一 to_response 路径

## 决策
content_negotiate 模块实现 ?format=query-param > Accept 头 > 默认 JSON 的优先级；所有响应经 write_pretty_json/write_xml 输出，错误体统一 {code,message} JSON 或 <Error><Code/><Message/></Error> XML。

## 影响
新增 kind 时需确保 from_json_fn/to_json_fn/to_xml_fn 三件套一致；禁止手拼字符串，全部走项目序列化规范。