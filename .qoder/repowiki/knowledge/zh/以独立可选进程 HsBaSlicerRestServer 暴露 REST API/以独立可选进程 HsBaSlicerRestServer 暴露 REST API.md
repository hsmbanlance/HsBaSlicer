---
kind: design
name: 以独立可选进程 HsBaSlicerRestServer 暴露 REST API
source: session
category: adr
---

# 以独立可选进程 HsBaSlicerRestServer 暴露 REST API

_来源：ca9d258 → ae47e03 提交周期内记录的编码计划——内容为规划时意图，实现可能滞后或有出入。_

**状态：** accepted

## 背景
需要为切片引擎提供外部可访问接口，但核心库 DllHsBaSlicer 是同步 C ABI，且项目已有大量桌面/非桌面平台裁剪逻辑。

## 决策驱动
- 与主程序解耦（失败隔离、独立部署）
- 保持默认构建最小化（HSBA_BUILD_REST_API=OFF）
- 复用现有 LibHsBaSlicer C ABI 而非侵入核心

## 备选方案
- **在主进程中嵌入 HTTP 服务器** _（已否决）_ — 优点：共享内存、无需 IPC；缺点：放大攻击面；HTTP 崩溃影响主程序；破坏现有构建裁剪
- **通过 gRPC / ZeroMQ 等消息总线** _（已否决）_ — 优点：天然异步、多语言客户端；缺点：引入新协议栈和序列化层；本期无跨进程需求
- **独立可选进程 + cpp-httplib + JSON/XML REST** — 优点：构建门控清晰；JSON/XML 对 curl/浏览器友好；依赖单一轻量；缺点：IPC 仅通过本地 TCP；大结果暂存进程内存

## 决策
新增顶层子目录 restapi/，生成可执行目标 HsBaSlicerRestServer，由 CMake 选项 HSBA_BUILD_REST_API（默认 OFF）门控；服务基于 cpp-httplib，监听地址从 restapi.ini 读取，向 DllHsBaSlicer 的 C ABI 转发请求。

## 影响
restapi 作为可选组件不进入默认安装产物；vcpkg.json 通过 feature.rest_api 门控 cpp-httplib；Windows/Linux/macOS 支持，Android/iOS/主机强制关闭；鉴权本期不实现，仅本机/内网信任模型。