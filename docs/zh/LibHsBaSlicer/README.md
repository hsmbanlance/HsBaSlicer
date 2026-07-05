# LibHsBaSlicer 模块

LibHsBaSlicer 是 HsBaSlicer 的核心 C++ 静态库，提供五大切片接口：预处理、切片、支撑、填充和路径生成。

## 子模块列表

- [Preprocess（模型预处理）](./model_preprocess.md) - 模型加载、变换和信息查询
- [Slice（网格切片）](./mesh_slice.md) - Z 轴平面切片，生成层轮廓
- [Support（FDM 支撑生成）](./fdm_support.md) - FDM 支撑截面生成
- [Fill（多边形填充）](./polygon_fill.md) - 多种模式的多边形填充
- [Path（路径生成）](./path_generator.md) - 从层数据生成 G-code 路径

## 架构

```
LibHsBaSlicer
├── Preprocess/    模型加载与变换
├── Slice/         指定高度的网格切片
├── Support/       FDM 支撑生成
├── Fill/          多边形填充模式
└── Path/          G-code 路径生成
```

## 使用方法

使用 LibHsBaSlicer 需要包含相应的头文件：

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
```

链接 `LibHsBaSlicer` 及其依赖项。

## 典型工作流

1. **预处理**：通过 `LoadModel()` 加载模型，施加变换
2. **切片**：在每个层高位置通过 `Slice()` 生成层轮廓
3. **支撑**：通过 `GenerateAllFdmSupport()` 生成支撑结构
4. **填充**：通过 `FillPolygon()` 或 `FillWithBorder()` 填充层多边形
5. **路径**：通过 `GenerateGCodePath()` 生成 G-code 路径

## 命名空间

所有 API 均位于 `HsBa::Slicer` 命名空间中。
