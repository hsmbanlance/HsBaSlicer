# OcctModel OpenCascade B-Rep建模

<cite>
**本文引用的文件**   
- [OcctModel.hpp](file://cadmodel/OcctModel.hpp)
- [OcctModel.cpp](file://cadmodel/OcctModel.cpp)
- [OcctTypeAliases.hpp](file://cadmodel/OcctTypeAliases.hpp)
- [UserCustomCADModel.hpp](file://cadmodel/UserCustomCADModel.hpp)
- [UserCustomCADModel.cpp](file://cadmodel/UserCustomCADModel.cpp)
- [IModel.hpp](file://base/IModel.hpp)
- [ModelFormat.hpp](file://base/ModelFormat.hpp)
- [occt_model_test.cpp](file://tests/Models/occt_model_test.cpp)
- [CMakeLists.txt](file://CMakeLists.txt)
- [README.md](file://README.md)
- [vcpkg-dependencies.md](file://docs/zh/vcpkg-dependencies.md)
</cite>

## 目录
1. [项目概述与快速开始](#项目概述与快速开始)
2. [架构总览](#架构总览)
3. [核心概念](#核心概念)
4. [核心模块详解](#核心模块详解)
5. [高级主题](#高级主题)
6. [API 参考文档](#api-参考文档)
7. [集成指南](#集成指南)
8. [开发测试与贡献](#开发测试与贡献)
9. [扩展开发指南](#扩展开发指南)
10. [示例与教程](#示例与教程)
11. [部署故障排除与FAQ](#部署故障排除与faq)
12. [附录](#附录)

## 项目概述与快速开始
本技术文档聚焦于 HsBaSlicer 中的 OcctModel 实现，说明如何基于 OpenCascade 的边界表示进行实体建模、曲面操作、布尔运算、抽壳、拉伸以及 B-Rep 到三角网格的转换。同时给出 OpenCascade 后端在 CMake 中的启用方式、平台兼容性、精度控制与性能调优建议，并提供加载 CAD 模型、执行几何操作和导出网格数据的使用路径。

- 目标读者：需要理解或扩展 HsBaSlicer 中 CAD/B-Rep 建模能力的开发者。
- 前置依赖：OpenCascade（可选，受 HSBA_COPL 控制）、Eigen、Boost（用于动态库加载）。
- 关键入口：`HsBa::Slicer::OcctModel` 实现 `IModel` 接口，封装 TopoDS_Shape 并提供文件读写、变换、体积、包围盒、布尔运算、抽壳、原始体创建与拉伸等能力。

**章节来源**
- [OcctModel.hpp:22-29](file://cadmodel/OcctModel.hpp#L22-L29)
- [README.md:41-94](file://README.md#L41-L94)

## 架构总览
OcctModel 位于 cadmodel 模块，作为 IModel 的一种具体实现，内部持有 OCCT 的 `TopoDS_Shape`，并通过 OCCT 提供的 IO、布尔运算、几何属性计算、三角剖分等 API 完成功能。用户自定义 CAD 后端通过 UserCustomCADModel 以动态库形式接入，遵循统一的 IModel 契约。

```mermaid
graph TB
IModel["IModel 抽象接口"] --> OcctModel["OcctModel<br/>OpenCascade B-Rep 实现"]
IModel --> UserCustomCADModel["UserCustomCADModel<br/>外部 CAD DLL 适配器"]
OcctModel --> OCCT_IO["STEP/IGES/VRML/BRep 读写"]
OcctModel --> OCCT_BOOL["布尔运算<br/>并/交/差/XOR"]
OcctModel --> OCCT_GEO["几何属性<br/>体积/包围盒/三角化"]
OcctModel --> OCCT_PRIM["原始体工厂<br/>Box/Sphere/Cylinder/Cone/Torus"]
OcctModel --> OCCT_EXTRUDE["拉伸<br/>单多边形 Extrude"]
OcctModel --> OCCT_THICK["抽壳<br/>ThickSolid"]
UserCustomCADModel --> DllLoader["动态库加载器<br/>boost::dll"]
```

**图表来源**
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [OcctModel.cpp:76-190](file://cadmodel/OcctModel.cpp#L76-L190)
- [OcctModel.cpp:451-539](file://cadmodel/OcctModel.cpp#L451-L539)
- [UserCustomCADModel.hpp:27-121](file://cadmodel/UserCustomCADModel.hpp#L27-L121)
- [UserCustomCADModel.cpp:24-115](file://cadmodel/UserCustomCADModel.cpp#L24-L115)

**章节来源**
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)
- [UserCustomCADModel.hpp:27-121](file://cadmodel/UserCustomCADModel.hpp#L27-L121)

## 核心概念
- B-Rep 数据模型：OcctModel 内部使用 `TopoDS_Shape` 表达拓扑结构，支持复合形状、面、边、顶点等层次结构。
- 文件格式：支持 STEP、IGES、VRML、BRep 的读取与保存；对网格格式（如 STL/OBJ）则先转换为三角网格再交由 mesh 模型处理。
- 布尔运算：提供 Union、Intersection、Difference、Xor 四种布尔操作，底层调用 OCCT 的 BRepAlgoAPI。
- 抽壳与拉伸：支持整体抽壳与按指定面集合抽壳；支持从二维多边形（含孔洞）沿方向拉伸为实体。
- 三角网格转换：遍历所有 Face，获取 Poly_Triangulation，应用局部变换后输出 libigl 风格的顶点与面索引。
- 类型别名：OcctTypes 根据 OCCT 版本（8.0+ 或更早）映射标准 C++ 类型，保证跨版本兼容。

**章节来源**
- [OcctModel.cpp:76-190](file://cadmodel/OcctModel.cpp#L76-L190)
- [OcctModel.cpp:353-407](file://cadmodel/OcctModel.cpp#L353-L407)
- [OcctModel.cpp:451-539](file://cadmodel/OcctModel.cpp#L451-L539)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)
- [OcctTypeAliases.hpp:14-41](file://cadmodel/OcctTypeAliases.hpp#L14-L41)

## 核心模块详解

### OcctModel：OpenCascade B-Rep 建模核心
- 文件读写：Load/Save 根据扩展名选择对应 Reader/Writer；对网格格式走 TriangleMesh 后再由 IglModel 保存。
- 变换：Translate/Rotate/Scale/Transform 将 Eigen 变换矩阵映射到 OCCT 的 gp_Trsf 并重建 Shape。
- 几何查询：BoundingBox 遍历顶点计算 AABB；Volume 使用 GProp 计算有符号体积。
- 布尔运算：UnionAll 合并复合体中所有 Solid；全局函数 Union/Intersection/Difference/Xor 封装 BRepAlgoAPI。
- 抽壳：ThickSolid(model, thickness) 与 ThickSolid(model, faces, thickness) 分别支持整体抽壳与按面集抽壳。
- 原始体与拉伸：CreateBox/Sphere/Cylinder/Cone/Torus 创建基本体；CreatePrime 支持单多边形与多边形组（外轮廓+孔）拉伸。

```mermaid
classDiagram
class IModel {
+Load(fileName) bool
+Save(fileName, format) bool
+Translate(translation) void
+Rotate(rotation) void
+Scale(scale) void
+Scale(axisScale) void
+Transform(transform) void
+BoundingBox(min, max) void
+Volume() float
+TriangleMesh() pair
}
class OcctModel {
-shape_ : TopoDS_Shape
-fileName_ : string
+AddShape(shape) void
+Load(fileName) bool
+Save(fileName, format) bool
+Translate(translation) void
+Rotate(rotation) void
+Scale(scale) void
+Scale(axisScale) void
+Transform(transform) void
+BoundingBox(min, max) void
+Volume() float
+TriangleMesh() pair
+UnionAll() bool
+CreateBox(size) OcctModel
+CreateSphere(radius, subdivisions) OcctModel
+CreateCylinder(radius, height, segments) OcctModel
+CreateCone(radius, height, segments) OcctModel
+CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments) OcctModel
+CreatePrime(poly, direction) OcctModel
+CreatePrime(polys, direction) OcctModel
}
IModel <|-- OcctModel
```

**图表来源**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [OcctModel.hpp:22-124](file://cadmodel/OcctModel.hpp#L22-L124)

**章节来源**
- [OcctModel.cpp:213-271](file://cadmodel/OcctModel.cpp#L213-L271)
- [OcctModel.cpp:272-329](file://cadmodel/OcctModel.cpp#L272-L329)
- [OcctModel.cpp:331-407](file://cadmodel/OcctModel.cpp#L331-L407)
- [OcctModel.cpp:409-469](file://cadmodel/OcctModel.cpp#L409-L469)
- [OcctModel.cpp:471-539](file://cadmodel/OcctModel.cpp#L471-L539)
- [OcctModel.cpp:422-449](file://cadmodel/OcctModel.cpp#L422-L449)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)

### B-Rep 到三角网格的转换机制
- 遍历所有 Face，获取 Poly_Triangulation。
- 若 Face 无三角化则跳过。
- 应用 Face 的局部变换（Location），将节点坐标转换到全局坐标系。
- 根据 Face 方向调整三角形顶点顺序，确保法向一致。
- 累积顶点和面索引，返回 libigl 风格的数据结构。

```mermaid
flowchart TD
Start(["进入 TriangleMesh"]) --> IterateFaces["遍历所有 Face"]
IterateFaces --> HasTri{"Face 存在三角化?"}
HasTri --> |否| NextFace["跳过该 Face"]
HasTri --> |是| GetTri["获取 Poly_Triangulation"]
GetTri --> ApplyLoc["应用局部变换 Location"]
ApplyLoc --> AddVerts["添加顶点到顶点列表"]
AddVerts --> OrientCheck{"Face 方向是否反转?"}
OrientCheck --> |是| SwapIndices["交换三角形顶点索引"]
OrientCheck --> |否| KeepIndices["保持顶点索引顺序"]
SwapIndices --> AddFaces["添加面索引到面列表"]
KeepIndices --> AddFaces
AddFaces --> NextFace
NextFace --> End(["返回顶点与面数据"])
```

**图表来源**
- [OcctModel.cpp:353-399](file://cadmodel/OcctModel.cpp#L353-L399)

**章节来源**
- [OcctModel.cpp:353-399](file://cadmodel/OcctModel.cpp#L353-L399)

### 布尔运算与复杂拓扑关系处理
- 并集：BRepAlgoAPI_Fuse 合并两个 Shape。
- 交集：BRepAlgoAPI_Common 计算公共部分。
- 差集：BRepAlgoAPI_Cut 从左侧减去右侧。
- XOR：通过并集与交集组合得到对称差。
- 复合体合并：UnionAll 遍历所有 Solid 逐步 Fuse。

```mermaid
sequenceDiagram
participant Client as "调用方"
participant ModelA as "左模型"
participant ModelB as "右模型"
participant OCCT as "OCCT 布尔引擎"
Client->>ModelA : 准备左 Shape
Client->>ModelB : 准备右 Shape
Client->>OCCT : 执行布尔操作
OCCT-->>Client : 返回结果 Shape
Client->>ModelA : 构造新 OcctModel(结果)
```

**图表来源**
- [OcctModel.cpp:451-469](file://cadmodel/OcctModel.cpp#L451-L469)

**章节来源**
- [OcctModel.cpp:409-469](file://cadmodel/OcctModel.cpp#L409-L469)

### 抽壳与拉伸
- 整体抽壳：ThickSolid(model, thickness) 直接对整体 Shape 抽壳。
- 按面抽壳：ThickSolid(model, faces, thickness) 将输入面集转为 TopoDS_Face，匹配原模型面并移除，再执行 MakeThickSolidByJoin。
- 拉伸：CreatePrime 支持单多边形和多边形组（外轮廓+孔）沿方向拉伸为实体。

```mermaid
flowchart TD
Start(["进入 ThickSolid"]) --> Mode{"抽壳模式"}
Mode --> |整体| Simple["MakeThickSolidBySimple"]
Mode --> |按面| BuildFaces["构建 TopoDS_Face 列表"]
BuildFaces --> MatchFaces["匹配并移除待去除面"]
MatchFaces --> Join["MakeThickSolidByJoin"]
Simple --> Result["返回抽壳结果"]
Join --> Result
```

**图表来源**
- [OcctModel.cpp:471-539](file://cadmodel/OcctModel.cpp#L471-L539)

**章节来源**
- [OcctModel.cpp:471-539](file://cadmodel/OcctModel.cpp#L471-L539)

### 原始体工厂与拉伸
- 原始体：CreateBox/CreateSphere/CreateCylinder/CreateCone/CreateTorus 直接调用 BRepPrimAPI 生成 Shape。
- 拉伸：CreatePrime 将 PolygonD 或 PolygonsD 转换为 Wire/Face，再用 MakePrism 拉伸。

```mermaid
flowchart TD
Start(["进入 CreatePrime"]) --> Input{"输入类型"}
Input --> |单多边形| SingleWire["构建闭合 Wire"]
Input --> |多边形组| OuterWire["构建外轮廓 Wire"]
SingleWire --> FaceMaker["创建 Face"]
OuterWire --> HoleWires["添加孔洞 Wire"]
HoleWires --> FaceMaker
FaceMaker --> Prism["沿方向拉伸"]
Prism --> Result["返回实体模型"]
```

**图表来源**
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)

**章节来源**
- [OcctModel.cpp:422-449](file://cadmodel/OcctModel.cpp#L422-L449)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)

## 高级主题

### OpenCascade 后端配置与编译选项
- CMake 条件启用：当非 Android/iOS/游戏主机且启用 HSBA_COPL 时，查找 OpenCASCADE 并定义 USE_OCCT 与 USE_CAD_KERNEL。
- 模型对象池大小：HSBA_MODEL_POOL_SIZE 在可用 copyleft 内核时为 10，否则为 1，影响布尔运算时的中间模型数量。
- 调试模式警告：Debug 模式下布尔运算可能缓慢且内存占用高。

```mermaid
flowchart TD
Config["CMake 配置"] --> CheckPlatform{"平台检查"}
CheckPlatform --> |桌面| FindOCCT["find_package(OpenCASCADE)"]
CheckPlatform --> |移动/游戏| SkipOCCT["跳过 OpenCASCADE"]
FindOCCT --> DefineMacros["定义 USE_OCCT / USE_CAD_KERNEL"]
DefineMacros --> BuildTarget["链接 occt 相关目标"]
```

**图表来源**
- [CMakeLists.txt:308-318](file://CMakeLists.txt#L308-L318)

**章节来源**
- [CMakeLists.txt:297-318](file://CMakeLists.txt#L297-L318)

### 精度控制与性能调优
- 精度控制：
  - 抽壳容差：ThickSolid 使用固定 tolerance=1e-6 判断面距离与变换迹线差异。
  - 体积计算：GProp_GProps 基于几何属性计算，精度取决于底层 OCCT 实现。
- 性能调优：
  - 避免 Debug 模式下的复杂布尔运算。
  - 合理设置模型对象池大小（HSBA_MODEL_POOL_SIZE）。
  - 仅在必要时进行三角化；优先使用 Volume/BoundingBox 验证几何正确性。

**章节来源**
- [OcctModel.cpp:479-539](file://cadmodel/OcctModel.cpp#L479-L539)
- [OcctModel.cpp:402-407](file://cadmodel/OcctModel.cpp#L402-L407)
- [CMakeLists.txt:297-306](file://CMakeLists.txt#L297-L306)

### 平台兼容性指南
- 桌面平台（Windows/Linux/macOS）：OpenCASCADE 可正常安装与链接。
- Android/iOS：OpenCASCADE 被排除，因过于复杂不适合移动平台。
- 许可证：包含 CGAL/OpenCascade 时使用 GPL-3.0-or-later；不包含时使用 MIT。

**章节来源**
- [README.md:187-203](file://README.md#L187-L203)
- [vcpkg-dependencies.md:23-42](file://docs/zh/vcpkg-dependencies.md#L23-L42)

## API 参考文档

### IModel 接口
- Load/Save：模型文件读写。
- Translate/Rotate/Scale/Transform：几何变换。
- BoundingBox/Volume：几何查询。
- TriangleMesh：三角网格导出。

**章节来源**
- [IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)

### OcctModel 主要方法
- 文件读写：Load/Save（支持 STEP/IGES/VRML/BREP，网格格式转 IglModel）。
- 变换：Translate/Rotate/Scale/Transform（多种重载）。
- 几何查询：BoundingBox/Volume。
- 布尔运算：UnionAll；全局函数 Union/Intersection/Difference/Xor。
- 抽壳：ThickSolid（整体/按面）。
- 原始体：CreateBox/CreateSphere/CreateCylinder/CreateCone/CreateTorus。
- 拉伸：CreatePrime（单多边形/多边形组）。

**章节来源**
- [OcctModel.hpp:54-110](file://cadmodel/OcctModel.hpp#L54-L110)
- [OcctModel.cpp:213-271](file://cadmodel/OcctModel.cpp#L213-L271)
- [OcctModel.cpp:272-329](file://cadmodel/OcctModel.cpp#L272-L329)
- [OcctModel.cpp:331-407](file://cadmodel/OcctModel.cpp#L331-L407)
- [OcctModel.cpp:409-469](file://cadmodel/OcctModel.cpp#L409-L469)
- [OcctModel.cpp:471-539](file://cadmodel/OcctModel.cpp#L471-L539)
- [OcctModel.cpp:422-449](file://cadmodel/OcctModel.cpp#L422-L449)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)

### UserCustomCADModel 动态库适配
- 加载动态库：LoadDll/UnloadDll。
- 委托 IModel 操作：Load/Save/Translate/Rotate/Scale/Transform/BoundingBox/Volume/TriangleMesh。
- 布尔操作：BooleanOperation（需 DLL 暴露相应函数）。

**章节来源**
- [UserCustomCADModel.hpp:79-121](file://cadmodel/UserCustomCADModel.hpp#L79-L121)
- [UserCustomCADModel.cpp:134-173](file://cadmodel/UserCustomCADModel.cpp#L134-L173)
- [UserCustomCADModel.cpp:175-325](file://cadmodel/UserCustomCADModel.cpp#L175-L325)

## 集成指南

### 在项目中启用 OpenCascade
- 确保 HSBA_COPL 开启（默认 AUTO，可通过环境变量或 -DHSBA_COPL=ON/OFF 控制）。
- 在桌面平台，CMake 会自动 find_package(OpenCASCADE) 并定义 USE_OCCT/USE_CAD_KERNEL。
- 移动平台（Android/iOS）不启用 OpenCASCADE。

**章节来源**
- [CMakeLists.txt:308-318](file://CMakeLists.txt#L308-L318)
- [vcpkg-dependencies.md:15-21](file://docs/zh/vcpkg-dependencies.md#L15-L21)

### 使用示例流程
- 加载 CAD 模型：调用 OcctModel::Load，传入 STEP/IGES/VRML/BREP 文件路径。
- 执行几何操作：使用 Union/Intersection/Difference/Xor 进行布尔运算；使用 CreatePrime 进行拉伸；使用 ThickSolid 进行抽壳。
- 导出网格数据：调用 TriangleMesh 获取顶点与面索引，或使用 Save 导出为网格格式（STL/OBJ 等）。

**章节来源**
- [OcctModel.cpp:213-271](file://cadmodel/OcctModel.cpp#L213-L271)
- [OcctModel.cpp:451-469](file://cadmodel/OcctModel.cpp#L451-L469)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)
- [OcctModel.cpp:353-399](file://cadmodel/OcctModel.cpp#L353-L399)

## 开发测试与贡献

### 单元测试覆盖
- 基础实体与属性：CreateBox 体积与包围盒验证。
- 球体体积：CreateSphere 体积与理论值对比。
- 布尔运算：Union/Intersection/Difference/Xor 体积验证。
- 拉伸：单多边形与多边形组（含孔）拉伸体积与包围盒验证。
- 非平面方向拉伸：斜拉伸体积与包围盒验证。

**章节来源**
- [occt_model_test.cpp:8-48](file://tests/Models/occt_model_test.cpp#L8-L48)
- [occt_model_test.cpp:50-134](file://tests/Models/occt_model_test.cpp#L50-L134)

## 扩展开发指南

### 自定义 CAD 动态库接口
- 必须暴露的函数：create_model/destroy_model、boolean_operation、create_box/create_sphere/create_cylinder、set_thickness、create_prism/create_prism_ex。
- 命名约定：函数名以 addedFunName 前缀加后缀组成，例如 `{addedFunName}_create_model`。
- 生命周期管理：UserCustomCADModel 负责 DLL 加载与模型销毁。

**章节来源**
- [UserCustomCADModel.hpp:27-48](file://cadmodel/UserCustomCADModel.hpp#L27-L48)
- [UserCustomCADModel.cpp:35-115](file://cadmodel/UserCustomCADModel.cpp#L35-L115)

## 示例与教程

### 加载 CAD 模型
- 步骤：
  1. 创建 OcctModel 实例。
  2. 调用 Load 传入文件路径。
  3. 检查返回值确认加载成功。

**章节来源**
- [OcctModel.cpp:213-237](file://cadmodel/OcctModel.cpp#L213-L237)

### 执行几何操作
- 布尔运算：
  - 并集：Union(left, right)
  - 交集：Intersection(left, right)
  - 差集：Difference(left, right)
  - 异或：Xor(left, right)
- 拉伸：
  - 单多边形：CreatePrime(poly, direction)
  - 多边形组：CreatePrime(polys, direction)
- 抽壳：
  - 整体：ThickSolid(model, thickness)
  - 按面：ThickSolid(model, faces, thickness)

**章节来源**
- [OcctModel.cpp:451-469](file://cadmodel/OcctModel.cpp#L451-L469)
- [OcctModel.cpp:542-663](file://cadmodel/OcctModel.cpp#L542-L663)
- [OcctModel.cpp:471-539](file://cadmodel/OcctModel.cpp#L471-L539)

### 导出网格数据
- 步骤：
  1. 调用 TriangleMesh 获取顶点与面索引。
  2. 使用 IglModel 或直接写入网格文件（STL/OBJ 等）。

**章节来源**
- [OcctModel.cpp:353-399](file://cadmodel/OcctModel.cpp#L353-L399)
- [OcctModel.cpp:257-269](file://cadmodel/OcctModel.cpp#L257-L269)

## 部署故障排除与FAQ

### 常见问题
- 布尔运算失败：
  - 检查输入模型是否为有效 B-Rep。
  - 避免在 Debug 模式下执行复杂布尔运算。
  - 调整抽壳容差或简化模型拓扑。
- 三角网格为空：
  - CAD 原始体未自动三角化，需显式调用 TriangleMesh 或导出网格格式。
- 动态库找不到函数：
  - 确认 DLL 暴露的函数名符合 `{addedFunName}_xxx` 命名约定。
  - 检查 DLL 是否与主程序使用相同 ABI。

**章节来源**
- [OcctModel.cpp:451-469](file://cadmodel/OcctModel.cpp#L451-L469)
- [OcctModel.cpp:353-399](file://cadmodel/OcctModel.cpp#L353-L399)
- [UserCustomCADModel.cpp:108-115](file://cadmodel/UserCustomCADModel.cpp#L108-L115)

### 性能优化建议
- 减少不必要的三角化：优先使用 Volume/BoundingBox 验证。
- 合理设置 HSBA_MODEL_POOL_SIZE：布尔运算较多时增大池大小。
- 避免 Debug 模式：Release 模式显著提升布尔运算性能。

**章节来源**
- [CMakeLists.txt:297-306](file://CMakeLists.txt#L297-L306)

## 附录

### 数据类型与版本兼容
- OcctTypes 根据 OCCT 版本（8.0+ 或更早）映射 Integer/Boolean/Real/Byte/Character/ExtCharacter 为标准 C++ 类型或 Standard_XX 类型。

**章节来源**
- [OcctTypeAliases.hpp:14-41](file://cadmodel/OcctTypeAliases.hpp#L14-L41)

### 文件格式枚举
- ModelFormat 枚举包含 PLY/OBJ/STL/OFF/VRML/STEP/IGES/BREP/SLDPRT/CATPART/XYZ/VDB 等格式标识。

**章节来源**
- [IModel.hpp:65-109](file://base/IModel.hpp#L65-L109)
- [ModelFormat.hpp:19-59](file://base/ModelFormat.hpp#L19-L59)