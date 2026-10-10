---
kind: external_dependency
name: OpenCV 图像生成路径（桌面 Release 优化点）
slug: opencv
category: external_dependency
category_hints:
    - framework_behavior
scope:
    - '**'
source_files:
    - vcpkg.json
    - LibHsBaSlicer/Floor/sla_floor.cpp
    - 2D/ImageToPolygons.cpp
---

### 身份与角色
- vcpkg 依赖 `opencv`，用于多边形→位图渲染（SLA 层图/支撑图导出）。

### 行为约束
- 非 OpenCV 分支使用 libpng + libjpeg-turbo（`SavePNG` 自定义 `png_write` sink，`SaveJPG` 用 `tjCompress2` 输出内存缓冲）。
- 层图/支撑图循环已并行化为动态原子索引调度（`ForLayersParallel`），结果收集到独立槽位后串行插入 `ImagesPath` 以保序。