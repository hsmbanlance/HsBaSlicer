# 网格模型处理 (Mesh Model Processing)

<cite>
**本文引用的文件**   
- [base/IModel.hpp](file://base/IModel.hpp)
- [meshmodel/FullTopoModel.hpp](file://meshmodel/FullTopoModel.hpp)
- [meshmodel/FullTopoModel.cpp](file://meshmodel/FullTopoModel.cpp)
- [meshmodel/CgalModel.hpp](file://meshmodel/CgalModel.hpp)
- [meshmodel/CgalModel.cpp](file://meshmodel/CgalModel.cpp)
- [meshmodel/IglModel.hpp](file://meshmodel/IglModel.hpp)
- [meshmodel/IglModel.cpp](file://meshmodel/IglModel.cpp)
- [cadmodel/OcctModel.hpp](file://cadmodel/OcctModel.hpp)
- [cadmodel/OcctModel.cpp](file://cadmodel/OcctModel.cpp)
- [cadmodel/OcctCsg.hpp](file://cadmodel/OcctCsg.hpp)
- [cadmodel/OcctCsg.cpp](file://cadmodel/OcctCsg.cpp)
- [preprocess/ModelLoader.hpp](file://preprocess/ModelLoader.hpp)
- [preprocess/ModelLoader.cpp](file://preprocess/ModelLoader.cpp)
- [LibHsBaSlicer/Preprocess/model_preprocess.hpp](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp)
- [DllHsBaSlicer/model_preprocess.h](file://DllHsBaSlicer/model_preprocess.h)
</cite>

## 更新摘要
**所做更改**   
- 新增 CSG 布尔运算功能章节，详细说明并集、交集、差集、异或操作
- 添加三角形网格导出功能说明，包括 C ABI 接口
- 扩展文本 CSG 支持，介绍 OCCT DRAW Test Harness 集成
- 更新预处理系统架构，展示统一的操作接口
- 增强模型池管理和线程安全机制

## 目录
1. [引言](#引言)
2. [项目结构](#项目结构)
3. [核心概念与统一接口](#核心概念与统一接口)
4. [架构总览](#架构总览)
5. [核心组件详解](#核心组件详解)
6. [CSG 布尔运算系统](#csg-布尔运算系统)
7. [三角形网格导出功能](#三角形网格导出功能)
8. [文本 CSG 支持](#文本-csg-支持)
9. [依赖关系分析](#依赖关系分析)
10. [性能与精度优化建议](#性能与精度优化建议)
11. [API 参考](#api-参考)
12. [使用示例与内核切换指南](#使用示例与内核切换指南)
13. [故障排除](#故障排除)
14. [结论](#结论)

## 引言
本技术文档聚焦 HsBaSlicer 的网格模型处理模块，系统说明 meshmodel 提供的几何建模能力。该模块围绕一个统一的 IModel 抽象，提供四类实现：

- FullTopoModel：面向切片的全拓扑重建网格模型，适合需要顶点、边、面完整拓扑关系的算法。
- CgalModel：基于 CGAL 的多面体网格模型，支持布尔运算、体积、包围盒、几何原语和表面测地线等高级几何操作。
- IglModel：基于 libigl 的三角形网格模型，提供轻量高效的网格加载、变换、法线计算、布尔回退和几何原语。
- OcctModel：基于 OpenCascade 的 B-Rep CAD 模型，支持 STEP/IGES/VRML/BRep 等格式、布尔运算、抽壳和实体原语。

**更新** 本次更新重点增强了预处理系统，新增了完整的 CSG 布尔运算功能和三角形网格导出能力，同时提供了文本 CSG 脚本构建模型的支持。

## 项目结构
meshmodel 模块位于仓库根目录下的 `meshmodel` 子目录，包含三个核心模型实现；OcctModel 位于 `cadmodel` 子目录，作为 B-Rep 扩展；IModel 定义在 `base` 中，是所有模型的统一抽象。

```mermaid
graph TB
IModel["IModel<br/>统一三维模型接口"] --> FullTopoModel["FullTopoModel<br/>全拓扑重建切片模型"]
IModel --> IglModel["IglModel<br/>libigl 三角形网格"]
IModel --> CgalModel["CgalModel<br/>CGAL 多面体网格"]
IModel --> OcctModel["OcctModel<br/>OpenCascade B-Rep"]
IglModel --> CgalModel["布尔回退到 CGAL"]
preprocess["预处理与模型池<br/>ModelLoader / model_preprocess"] --> IModel
preprocess --> OcctModel
boolean_ops["CSG 布尔运算系统"] --> preprocess
triangle_export["三角形网格导出"] --> preprocess
text_csg["文本 CSG 支持"] --> OcctModel
```

**图示来源**
- [base/IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [meshmodel/FullTopoModel.hpp:53-142](file://meshmodel/FullTopoModel.hpp#L53-L142)
- [meshmodel/IglModel.hpp:26-121](file://meshmodel/IglModel.hpp#L26-L121)
- [meshmodel/CgalModel.hpp:34-155](file://meshmodel/CgalModel.hpp#L34-L155)
- [cadmodel/OcctModel.hpp:30-108](file://cadmodel/OcctModel.hpp#L30-L108)
- [preprocess/ModelLoader.cpp:136-164](file://preprocess/ModelLoader.cpp#L136-L164)

**章节来源**
- [base/IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [meshmodel/FullTopoModel.hpp:53-142](file://meshmodel/FullTopoModel.hpp#L53-L142)
- [meshmodel/IglModel.hpp:26-121](file://meshmodel/IglModel.hpp#L26-L121)
- [meshmodel/CgalModel.hpp:34-155](file://meshmodel/CgalModel.hpp#L34-L155)
- [cadmodel/OcctModel.hpp:30-108](file://cadmodel/OcctModel.hpp#L30-L108)

## 核心概念与统一接口
IModel 是 HsBaSlicer 的三维模型抽象基类，定义了所有模型必须实现的通用能力：

- 文件 I/O：Load(fileName)、Save(fileName, format)
- 几何变换：Translate、Rotate、Scale（均匀与非均匀）、Transform（Isometry/Matrix4/Affine）
- 查询：BoundingBox(min,max)、Volume()
- 网格导出：TriangleMesh() 返回 Eigen 风格的顶点矩阵与三角面片索引矩阵

```mermaid
classDiagram
class IModel {
+bool Load(fileName)
+bool Save(fileName, format)
+void Translate(translation)
+void Rotate(rotation)
+void Scale(scale)
+void Scale(scaleVector)
+void Transform(isometry)
+void Transform(matrix4)
+void Transform(affine)
+void BoundingBox(min, max)
+float Volume()
+pair<TriangleMesh> TriangleMesh()
}
```

**图示来源**
- [base/IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)

**章节来源**
- [base/IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)

## 架构总览
HsBaSlicer 的模型处理采用"统一接口 + 多内核实现"的架构：

- 上层流水线通过 IModel 调用模型能力，不关心具体内核。
- 预处理层根据文件格式和可用宏选择内核：B-Rep 优先 OCCT，网格优先 IGL/CGAL。
- 布尔运算路由：OCCT 用于 BRep-BRep，IGL/CGAL 用于 Mesh-Mesh。
- FullTopoModel 不参与 IModel 体系，而是为切片算法提供完整的拓扑视图。

**更新** 新的预处理系统提供了统一的 API 接口，支持模型加载、变换、查询、布尔运算和三角形网格导出。

```mermaid
sequenceDiagram
participant App as "应用或流水线"
participant Pre as "预处理 ModelLoader"
participant IO as "文件加载"
participant Kernel as "内核选择"
participant Model as "IModel 实现"
participant Slice as "切片算法"
participant Export as "三角形导出"
App->>Pre : 请求加载模型
Pre->>IO : 读取文件名与格式
IO-->>Pre : 返回原始数据
Pre->>Kernel : 判断是否 B-Rep / Mesh
alt B-Rep 可用
Kernel->>Model : 创建 OcctModel
else Mesh 可用
Kernel->>Model : 创建 IglModel 或 CgalModel
end
Model-->>Slice : 提供 TriangleMesh / Volume / BoundingBox
Model-->>Export : 导出三角网格数据
Slice-->>App : 输出切片结果
Export-->>App : 返回三角网格数据
```

**图示来源**
- [preprocess/ModelLoader.cpp:136-164](file://preprocess/ModelLoader.cpp#L136-L164)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:213-225](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L213-L225)

**章节来源**
- [preprocess/ModelLoader.cpp:136-164](file://preprocess/ModelLoader.cpp#L136-L164)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:213-225](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L213-L225)

## 核心组件详解

### FullTopoModel：面向切片的全拓扑重建模型
FullTopoModel 不是 IModel 的实现，而是一个专为切片设计的拓扑重建模型。它从任意 IModel 或顶点/三角面片数据构建顶点、边、面的完整拓扑关系，并提供安全切片、不安全切片、快速切片和 Lua 脚本切片。

关键数据结构：
- Vertex：位置、关联的面与边
- Edge：两个顶点、两个相邻面
- Face：三角面、三条边、法向量

主要能力：
- CheckTopo：检查拓扑完整性
- EulerCharacteristic：欧拉示性数，辅助检测孔洞或非流形
- Intersection：线段与水平平面的交点计算
- Slice / UnSafeSlice / SliceFast：Z 向切片，分别返回闭合轮廓、可能开放轮廓、快速拓扑切片
- SliceLua / UnSafeSliceLua：通过 Lua 脚本自定义切片逻辑

```mermaid
flowchart TD
Start(["进入 Slice"]) --> BuildAdj["按高度扫描三角面<br/>生成二维整数键邻接表"]
BuildAdj --> MergeNodes["网格哈希 + 并查集合并相近节点"]
MergeNodes --> BuildAdjMerged["构建合并后的邻接表"]
BuildAdjMerged --> Traverse["角度启发式遍历提取闭环"]
Traverse --> Result{"路径长度 >= 3 ?"}
Result --> |是| AddPoly["加入结果多边形"]
Result --> |否| Skip["跳过无效路径"]
AddPoly --> End(["返回 Polygons"])
Skip --> End
```

**图示来源**
- [meshmodel/FullTopoModel.cpp:328-599](file://meshmodel/FullTopoModel.cpp#L328-L599)

**章节来源**
- [meshmodel/FullTopoModel.hpp:53-142](file://meshmodel/FullTopoModel.hpp#L53-L142)
- [meshmodel/FullTopoModel.cpp:50-158](file://meshmodel/FullTopoModel.cpp#L50-L158)
- [meshmodel/FullTopoModel.cpp:160-244](file://meshmodel/FullTopoModel.cpp#L160-L244)
- [meshmodel/FullTopoModel.cpp:246-324](file://meshmodel/FullTopoModel.cpp#L246-L324)
- [meshmodel/FullTopoModel.cpp:328-599](file://meshmodel/FullTopoModel.cpp#L328-L599)

### IglModel：libigl 三角形网格模型
IglModel 实现 IModel，存储顶点、三角面片和可选法向量，提供文件加载保存、几何变换、包围盒、体积、法线计算、布尔运算和几何原语构造。

特点：
- 轻量高效，适合大规模网格处理
- 布尔运算默认使用 igl::copyleft::cgal::mesh_boolean，失败时回退到 CgalModel 的 Nef 布尔
- 当未启用 USE_CGAL 编译宏时，布尔运算抛出 NotSupportedError

```mermaid
classDiagram
class IglModel {
+bool Load(fileName)
+bool Save(fileName, format)
+void Translate(translation)
+void Rotate(rotation)
+void Scale(scale)
+void Scale(scaleVector)
+void Transform(isometry)
+void Transform(matrix4)
+void Transform(affine)
+void BoundingBox(min, max)
+float Volume()
+pair<TriangleMesh> TriangleMesh()
+void ComputeNormals()
+MatrixXf ComputeVertexNormals()
+MatrixXf ComputeFaceNormals()
+static CreateBox(size)
+static CreateSphere(radius, subdivisions)
+static CreateCylinder(radius, height, segments)
+static CreateCone(radius, height, segments)
+static CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments)
+static CreatePrime(poly, direction)
+static CreatePrime(polys, direction)
}
```

**图示来源**
- [meshmodel/IglModel.hpp:26-121](file://meshmodel/IglModel.hpp#L26-L121)

**章节来源**
- [meshmodel/IglModel.hpp:26-121](file://meshmodel/IglModel.hpp#L26-L121)
- [meshmodel/IglModel.cpp:37-193](file://meshmodel/IglModel.cpp#L37-L193)
- [meshmodel/IglModel.cpp:195-409](file://meshmodel/IglModel.cpp#L195-L409)
- [meshmodel/IglModel.cpp:411-596](file://meshmodel/IglModel.cpp#L411-L596)
- [meshmodel/IglModel.cpp:598-800](file://meshmodel/IglModel.cpp#L598-L800)

### CgalModel：CGAL 多面体网格模型
CgalModel 也实现 IModel，内部维护 CGAL::Polyhedron_3，提供布尔运算、几何原语、表面测地线、螺旋线和螺旋路径等高级几何功能。

特点：
- 使用 Exact_predicates_inexact_constructions_kernel，兼顾判定精确性与构造效率
- 布尔运算通过 Nef_polyhedron_3 实现 Union/Intersection/Difference/Xor
- 支持 CreateBox/Sphere/Cylinder/Cone/Torus 和 CreatePrime（单多边形或多多边形拉伸）
- 提供 GeodesicPath、GeodesicDistance、ProjectPointOnSurface、SurfaceSpiral、SurfaceHelix

```mermaid
classDiagram
class CgalModel {
+bool Load(fileName)
+bool Save(fileName, format)
+void Translate(translation)
+void Rotate(rotation)
+void Scale(scale)
+void Scale(scaleVector)
+void Transform(isometry)
+void Transform(matrix4)
+void Transform(affine)
+void BoundingBox(min, max)
+float Volume()
+pair<TriangleMesh> TriangleMesh()
+vector<Point> GeodesicPath(source, target)
+vector<float> GeodesicDistance(source)
+Point ProjectPointOnSurface(point)
+vector<Point> SurfaceSpiral(axisOrigin, axisDirection, turns, samplesPerTurn, startRadius, endRadius)
+vector<Point> SurfaceHelix(axisOrigin, axisDirection, turns, pitch, radius, samplesPerTurn)
+static CreateBox(size)
+static CreateSphere(radius, subdivisions)
+static CreateCylinder(radius, height, segments)
+static CreateCone(radius, height, segments)
+static CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments)
+static CreatePrime(poly, direction)
+static CreatePrime(polys, direction)
}
```

**图示来源**
- [meshmodel/CgalModel.hpp:34-155](file://meshmodel/CgalModel.hpp#L34-L155)

**章节来源**
- [meshmodel/CgalModel.hpp:34-155](file://meshmodel/CgalModel.hpp#L34-L155)
- [meshmodel/CgalModel.cpp:42-119](file://meshmodel/CgalModel.cpp#L42-L119)
- [meshmodel/CgalModel.cpp:121-237](file://meshmodel/CgalModel.cpp#L121-L237)
- [meshmodel/CgalModel.cpp:238-276](file://meshmodel/CgalModel.cpp#L238-L276)
- [meshmodel/CgalModel.cpp:278-445](file://meshmodel/CgalModel.cpp#L278-L445)
- [meshmodel/CgalModel.cpp:625-733](file://meshmodel/CgalModel.cpp#L625-L733)
- [meshmodel/CgalModel.cpp:735-955](file://meshmodel/CgalModel.cpp#L735-L955)
- [meshmodel/CgalModel.cpp:957-992](file://meshmodel/CgalModel.cpp#L957-L992)

### OcctModel：OpenCascade B-Rep 建模
OcctModel 实现 IModel，封装 TopoDS_Shape，提供 STEP/IGES/VRML/BRep 加载保存、布尔运算、抽壳、几何原语和实体拉伸。

特点：
- 适合 CAD 流程中的 B-Rep 建模与布尔运算
- 支持 ThickSolid（抽壳），可指定开口面
- 提供 CreateBox/Sphere/Cylinder/Cone/Torus 和 CreatePrime

```mermaid
classDiagram
class OcctModel {
+bool Load(fileName)
+bool Save(fileName, format)
+void Translate(translation)
+void Rotate(rotation)
+void Scale(scale)
+void Scale(scaleVector)
+void Transform(isometry)
+void Transform(matrix4)
+void Transform(affine)
+void BoundingBox(min, max)
+float Volume()
+pair<TriangleMesh> TriangleMesh()
+bool UnionAll()
+static CreateBox(size)
+static CreateSphere(radius, subdivisions)
+static CreateCylinder(radius, height, segments)
+static CreateCone(radius, height, segments)
+static CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments)
+static CreatePrime(poly, direction)
+static CreatePrime(polys, direction)
}
```

**图示来源**
- [cadmodel/OcctModel.hpp:30-108](file://cadmodel/OcctModel.hpp#L30-L108)

**章节来源**
- [cadmodel/OcctModel.hpp:30-108](file://cadmodel/OcctModel.hpp#L30-L108)
- [cadmodel/OcctModel.cpp:405-444](file://cadmodel/OcctModel.cpp#L405-L444)
- [preprocess/ModelLoader.cpp:193-212](file://preprocess/ModelLoader.cpp#L193-L212)

## CSG 布尔运算系统

**新增** HsBaSlicer 现在提供了完整的 CSG（计算机固体几何）布尔运算功能，支持四种基本操作：并集（Union）、交集（Intersection）、差集（Difference）和异或（Xor）。

### 布尔运算架构
布尔运算系统采用智能路由策略，根据输入模型类型自动选择最优的内核：

```mermaid
flowchart TD
InputA["左模型 A"] --> Router["布尔运算路由器"]
InputB["右模型 B"] --> Router
Router --> CheckType{"检查模型类型"}
CheckType --> |两者都是 OcctModel| UseOCCT["使用 OCCT B-Rep 布尔"]
CheckType --> |至少一个是网格模型| UseIGL["使用 IGL/CGAL 网格布尔"]
UseOCCT --> OCCTResult["返回 OcctModel 结果"]
UseIGL --> IGLResult["返回 IglModel 结果"]
```

**图示来源**
- [preprocess/ModelLoader.cpp:218-243](file://preprocess/ModelLoader.cpp#L218-L243)
- [preprocess/ModelLoader.cpp:245-269](file://preprocess/ModelLoader.cpp#L245-L269)
- [preprocess/ModelLoader.cpp:271-295](file://preprocess/ModelLoader.cpp#L271-L295)
- [preprocess/ModelLoader.cpp:297-321](file://preprocess/ModelLoader.cpp#L297-L321)

### 支持的布尔操作

#### 并集运算 (BooleanUnion)
将两个模型合并为一个整体，保留所有几何特征。

```cpp
auto result = loader.BooleanUnion("model_a", "model_b", "union_result");
```

#### 交集运算 (BooleanIntersection) 
仅保留两个模型的重叠部分。

```cpp
auto result = loader.BooleanIntersection("model_a", "model_b", "intersection_result");
```

#### 差集运算 (BooleanDifference)
从第一个模型中减去第二个模型的部分。

```cpp
auto result = loader.BooleanDifference("model_a", "model_b", "difference_result");
```

#### 异或运算 (BooleanXor)
保留两个模型的非重叠部分。

```cpp
auto result = loader.BooleanXor("model_a", "model_b", "xor_result");
```

### 内核选择策略
- **B-Rep 优先**：如果两个输入都是 OcctModel，直接使用 OCCT 的 B-Rep 布尔运算，提供最高精度。
- **网格回退**：如果任一输入是网格模型，转换为 IglModel 并使用 IGL/CGAL 进行布尔运算。
- **自动转换**：ToIglModel 函数自动将任何 IModel 转换为 IglModel，必要时通过 TriangleMesh 提取网格数据。

**章节来源**
- [preprocess/ModelLoader.cpp:137-146](file://preprocess/ModelLoader.cpp#L137-L146)
- [preprocess/ModelLoader.cpp:218-321](file://preprocess/ModelLoader.cpp#L218-L321)

## 三角形网格导出功能

**新增** 系统现在支持将任意模型导出为三角形网格数据，这对于可视化、分析和与其他几何库集成非常有用。

### 导出接口
系统提供了两种导出方式：

#### C++ API
```cpp
std::pair<Eigen::MatrixXf, Eigen::MatrixXi> triangles = model->TriangleMesh();
```

#### C ABI 接口
```c
HsBaModelTriangles_t tris;
int success = HsBaGetModelTriangles("model_name", &tris);
// 使用 tris.xyz 和 tris.indices 访问数据
HsBaFreeModelTriangles(&tris); // 释放内存
```

### 内存管理
三角形导出使用统一的内存分配策略：
- 顶点和索引数据存储在单个连续内存块中
- 顶点坐标以交错格式存储（x,y,z,x,y,z,...）
- 索引值从零开始，指向顶点数组
- 通过 HsBaFreeModelTriangles 统一释放内存

```mermaid
flowchart LR
Model["模型对象"] --> Extract["提取三角网格"]
Extract --> MemoryBlock["连续内存块"]
MemoryBlock --> Vertices["顶点数组 xyz"]
MemoryBlock --> Indices["索引数组 indices"]
Vertices --> User["用户代码"]
Indices --> User
User --> Free["释放内存"]
```

**图示来源**
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:213-225](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L213-L225)
- [DllHsBaSlicer/model_preprocess.h:134-159](file://DllHsBaSlicer/model_preprocess.h#L134-L159)

**章节来源**
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:213-225](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L213-L225)
- [DllHsBaSlicer/model_preprocess.h:134-159](file://DllHsBaSlicer/model_preprocess.h#L134-L159)

## 文本 CSG 支持

**新增** 系统现在支持通过 OCCT DRAW Test Harness 脚本构建 CSG 模型，提供了一种程序化的几何建模方式。

### 文本 CSG 脚本语法
文本 CSG 使用 DRAW Test Harness 命令语言，支持以下操作：

#### 几何原语
- `box x y z` - 创建长方体
- `pcylinder r h` - 创建圆柱体
- `pcone r1 r2 h` - 创建圆锥体
- `psphere r` - 创建球体
- `ptorus R r` - 创建环面

#### 变换操作
- `ttranslate x y z` - 平移变换
- `trotate angle x y z` - 旋转变换
- `tscale s` - 缩放变换

#### 布尔运算
- `bfuse a b` - 并集运算
- `bcut a b` - 差集运算
- `bcommon a b` - 交集运算

### 脚本执行流程
```mermaid
sequenceDiagram
participant Script as "CSG 脚本"
participant Interpreter as "DRAW 解释器"
participant Bank as "DRAW 银行"
participant Shape as "形状对象"
Script->>Interpreter : 执行命令
Interpreter->>Bank : 注册几何原语
Bank->>Shape : 创建形状对象
Shape->>Bank : 存储到命名空间
Interpreter->>Script : 返回执行结果
```

**图示来源**
- [cadmodel/OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

### 安全性考虑
- 脚本在完整的 Tcl 解释器中执行，具有完全的 Tcl 功能
- 不提供沙箱保护，只应信任来自可信源的脚本
- 每个脚本调用都保持隔离，不会污染全局状态

**章节来源**
- [cadmodel/OcctCsg.hpp:19-56](file://cadmodel/OcctCsg.hpp#L19-L56)
- [cadmodel/OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:327-338](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L327-L338)

## 依赖关系分析
各模型之间的依赖与协作如下：

- IModel 是所有模型的基础接口。
- IglModel 在启用 USE_CGAL 时依赖 CgalModel 进行布尔回退。
- CgalModel 依赖 CGAL 库进行布尔、体积、AABB 树、测地线等计算。
- OcctModel 依赖 OpenCascade 进行 B-Rep 布尔、抽壳和实体建模。
- FullTopoModel 依赖 IModel 以获取三角网格数据，并依赖 libigl 计算面法线。
- ModelLoader 统一管理所有模型操作，提供布尔运算和网格导出功能。

```mermaid
graph LR
IModel --> IglModel
IModel --> CgalModel
IModel --> OcctModel
IglModel --> CgalModel
FullTopoModel --> IModel
ModelLoader --> IModel
ModelLoader --> OcctModel
OcctCsg --> OcctModel
```

**图示来源**
- [base/IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)
- [meshmodel/IglModel.cpp:195-409](file://meshmodel/IglModel.cpp#L195-L409)
- [meshmodel/CgalModel.cpp:238-276](file://meshmodel/CgalModel.cpp#L238-L276)
- [cadmodel/OcctModel.cpp:405-444](file://cadmodel/OcctModel.cpp#L405-L444)
- [meshmodel/FullTopoModel.cpp:32-48](file://meshmodel/FullTopoModel.cpp#L32-L48)
- [preprocess/ModelLoader.cpp:137-146](file://preprocess/ModelLoader.cpp#L137-L146)

**章节来源**
- [meshmodel/IglModel.cpp:195-409](file://meshmodel/IglModel.cpp#L195-L409)
- [meshmodel/CgalModel.cpp:238-276](file://meshmodel/CgalModel.cpp#L238-L276)
- [cadmodel/OcctModel.cpp:405-444](file://cadmodel/OcctModel.cpp#L405-L444)
- [meshmodel/FullTopoModel.cpp:32-48](file://meshmodel/FullTopoModel.cpp#L32-L48)
- [preprocess/ModelLoader.cpp:137-146](file://preprocess/ModelLoader.cpp#L137-L146)

## 性能与精度优化建议
- 网格规模与内存：
  - IglModel 使用 Eigen 矩阵存储顶点与面片，适合大规模网格；避免频繁复制，尽量使用移动语义。
  - CgalModel 内部维护 Polyhedron_3，布尔运算会创建 Nef 对象，注意内存峰值。
  - OcctModel 的 B-Rep 布尔和抽壳可能产生复杂拓扑，建议在布尔前简化模型。

- 布尔运算稳定性：
  - IglModel 的布尔先尝试 igl::copyleft::cgal::mesh_boolean，失败后回退到 CgalModel 的 Nef 布尔。
  - CgalModel 的布尔基于 Nef_polyhedron_3，对浮点坐标敏感，建议输入网格具有外法线一致性和良好定向。
  - OcctModel 的布尔适合 B-Rep 输入，若输入为网格，应先 tessellation 再布尔。
  - **新增** ModelLoader 的智能路由确保 B-Rep 模型优先使用 OCCT 布尔运算，提供更好的精度和性能。

- 精度控制：
  - FullTopoModel 的切片使用整数化坐标与容差 tolerance 控制点合并；tolerance 越大，越容易合并近似点，但可能丢失细节。
  - CgalModel 使用 EpicKernel，判定精确而构造近似；对于极端小尺寸或大尺度模型，建议归一化单位。
  - IglModel 的体积计算使用绝对值，确保正体积；布尔失败时返回空网格，应检查输入有效性。
  - **新增** 三角形网格导出时，B-Rep 模型的三角化精度取决于内核的默认设置，可通过调整内核参数控制。

- 法线与方向：
  - IglModel 和 CgalModel 都强调外法线一致性；CreatePrime 和 Create* 系列均保证外法线朝外。
  - CgalModel 在拉伸后检查体积符号并翻转面方向，确保体积为正。

- 性能优化：
  - 批量操作时复用模型实例，避免重复构造。
  - 对大型网格，优先使用 IglModel 进行快速变换与查询，仅在需要布尔或高级几何时使用 CgalModel。
  - 对 CAD 流程，优先使用 OcctModel 进行布尔与抽壳，减少网格化误差。
  - **新增** 使用 ModelLoader 的统一接口可以减少内核切换开销，提高批量操作效率。

[本节为通用指导，不直接分析具体文件]

## API 参考

### IModel 统一接口
- Load(fileName): 从文件加载模型
- Save(fileName, format): 保存模型到指定格式
- Translate(translation): 平移
- Rotate(rotation): 旋转
- Scale(scale): 均匀缩放
- Scale(scaleVector): 非均匀缩放
- Transform(isometry/matrix4/affine): 仿射变换
- BoundingBox(min, max): 轴对齐包围盒
- Volume(): 有符号体积
- TriangleMesh(): 返回顶点矩阵与三角面片索引矩阵

**章节来源**
- [base/IModel.hpp:110-139](file://base/IModel.hpp#L110-L139)

### FullTopoModel
- 构造函数：从 IModel 或顶点/三角面片构建
- CheckTopo(): 检查拓扑完整性
- EulerCharacteristic(): 欧拉示性数
- Intersection(v1, v2, height, intersection): 线段与水平面交点
- Slice(height, tolerance): 安全切片，仅闭合轮廓
- UnSafeSlice(height, tolerance): 不安全切片，含开放轮廓
- SliceFast(height): 快速拓扑切片
- SliceLua(script/funcName/file, height, ext_regs): Lua 脚本切片
- UnSafeSliceLua(...): Lua 脚本不安全切片

**章节来源**
- [meshmodel/FullTopoModel.hpp:74-142](file://meshmodel/FullTopoModel.hpp#L74-L142)
- [meshmodel/FullTopoModel.cpp:27-48](file://meshmodel/FullTopoModel.cpp#L27-L48)
- [meshmodel/FullTopoModel.cpp:160-244](file://meshmodel/FullTopoModel.cpp#L160-L244)
- [meshmodel/FullTopoModel.cpp:328-599](file://meshmodel/FullTopoModel.cpp#L328-L599)
- [meshmodel/FullTopoModel.cpp:604-800](file://meshmodel/FullTopoModel.cpp#L604-L800)

### IglModel
- 构造函数：从顶点/面片/法线构建，支持移动语义
- Load/Save: STL/OBJ/PLY/OFF 等网格格式
- 变换：Translate/Rotate/Scale/Transform
- BoundingBox/Volume/TriangleMesh
- ComputeNormals/ComputeVertexNormals/ComputeFaceNormals
- 布尔：Union/Intersection/Difference/Xor（需 USE_CGAL）
- 几何原语：CreateBox/Sphere/Cylinder/Cone/Torus/CreatePrime

**章节来源**
- [meshmodel/IglModel.hpp:29-121](file://meshmodel/IglModel.hpp#L29-L121)
- [meshmodel/IglModel.cpp:37-193](file://meshmodel/IglModel.cpp#L37-L193)
- [meshmodel/IglModel.cpp:195-409](file://meshmodel/IglModel.cpp#L195-L409)
- [meshmodel/IglModel.cpp:411-596](file://meshmodel/IglModel.cpp#L411-L596)
- [meshmodel/IglModel.cpp:598-800](file://meshmodel/IglModel.cpp#L598-L800)

### CgalModel
- 构造函数：从 Polyhedron_3 或顶点/面片构建
- Load/Save: STL/PLY/OBJ/OFF 等网格格式
- 变换：Translate/Rotate/Scale/Transform
- BoundingBox/Volume/TriangleMesh
- 布尔：Union/Intersection/Difference/Xor（Nef 布尔）
- 几何原语：CreateBox/Sphere/Cylinder/Cone/Torus/CreatePrime
- 表面几何：GeodesicPath/GeodesicDistance/ProjectPointOnSurface/SurfaceSpiral/SurfaceHelix

**章节来源**
- [meshmodel/CgalModel.hpp:37-155](file://meshmodel/CgalModel.hpp#L37-L155)
- [meshmodel/CgalModel.cpp:42-119](file://meshmodel/CgalModel.cpp#L42-L119)
- [meshmodel/CgalModel.cpp:121-237](file://meshmodel/CgalModel.cpp#L121-L237)
- [meshmodel/CgalModel.cpp:238-276](file://meshmodel/CgalModel.cpp#L238-L276)
- [meshmodel/CgalModel.cpp:278-445](file://meshmodel/CgalModel.cpp#L278-L445)
- [meshmodel/CgalModel.cpp:625-733](file://meshmodel/CgalModel.cpp#L625-L733)
- [meshmodel/CgalModel.cpp:735-955](file://meshmodel/CgalModel.cpp#L735-L955)
- [meshmodel/CgalModel.cpp:957-992](file://meshmodel/CgalModel.cpp#L957-L992)

### OcctModel
- 构造函数：从 TopoDS_Shape 构建
- Load/Save: STEP/IGES/VRML/BREP 等 B-Rep 格式
- 变换：Translate/Rotate/Scale/Transform
- BoundingBox/Volume/TriangleMesh
- 布尔：Union/Intersection/Difference/Xor（B-Rep 布尔）
- 抽壳：ThickSolid（厚度与开口面）
- 几何原语：CreateBox/Sphere/Cylinder/Cone/Torus/CreatePrime

**章节来源**
- [cadmodel/OcctModel.hpp:33-108](file://cadmodel/OcctModel.hpp#L33-L108)
- [cadmodel/OcctModel.cpp:405-444](file://cadmodel/OcctModel.cpp#L405-L444)
- [preprocess/ModelLoader.cpp:193-212](file://preprocess/ModelLoader.cpp#L193-L212)

### ModelLoader 统一接口
- LoadModel(name, filePath): 从文件加载模型
- InsertModel(name, model): 插入预构建模型
- GetModel(name): 获取模型引用
- RemoveModel(name): 移除模型
- ContainsModel(name): 检查模型存在性
- ModelCount(): 获取模型数量
- GetModelNames(): 获取所有模型名称
- Cleanup(): 清理未引用模型

**新增** 布尔运算接口：
- BooleanUnion(leftName, rightName, resultName): 并集运算
- BooleanIntersection(leftName, rightName, resultName): 交集运算
- BooleanDifference(leftName, rightName, resultName): 差集运算
- BooleanXor(leftName, rightName, resultName): 异或运算

**新增** 网格导出接口：
- GetModelTriangles(name): 导出三角网格数据

**章节来源**
- [preprocess/ModelLoader.hpp:47-92](file://preprocess/ModelLoader.hpp#L47-L92)
- [preprocess/ModelLoader.hpp:94-133](file://preprocess/ModelLoader.hpp#L94-L133)
- [preprocess/ModelLoader.cpp:45-84](file://preprocess/ModelLoader.cpp#L45-L84)
- [preprocess/ModelLoader.cpp:218-321](file://preprocess/ModelLoader.cpp#L218-L321)

### 预处理系统 API
**新增** 高级模型操作接口：
- ThickSolidModel(sourceName, resultName, thickness): 抽壳操作
- GetModelInfo(name): 获取模型信息（包围盒、体积）
- HasTextCsgSupport(): 检查文本 CSG 支持
- BuildCsgModel(name, script, shapeName): 从 CSG 脚本构建模型

**章节来源**
- [LibHsBaSlicer/Preprocess/model_preprocess.hpp:167-266](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L167-L266)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:275-338](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L275-L338)

## 使用示例与内核切换指南

### 如何在不同几何内核之间切换
- 如果输入是 STEP/IGES/VRML/BREP，且启用 USE_OCCT，则优先使用 OcctModel。
- 如果输入是 STL/OBJ/PLY/OFF，则优先使用 IglModel；如需布尔运算，启用 USE_CGAL 以获得稳定结果。
- 如果需要表面测地线、螺旋线等高级几何，使用 CgalModel。
- 如果需要完整拓扑关系进行切片，使用 FullTopoModel。
- **新增** 使用 ModelLoader 自动选择合适的内核，无需手动干预。

```mermaid
flowchart TD
Input["输入模型"] --> Format{"文件格式？"}
Format --> |STEP/IGES/VRML/BREP| UseOCCT["使用 OcctModel"]
Format --> |STL/OBJ/PLY/OFF| UseIGL["使用 IglModel"]
UseIGL --> NeedBoolean{"需要布尔？"}
NeedBoolean --> |是| EnableCGAL["启用 USE_CGAL"]
EnableCGAL --> UseCgal["布尔回退到 CgalModel"]
NeedBoolean --> |否| KeepIGL["保持 IglModel"]
UseOCCT --> NeedShell{"需要抽壳？"}
NeedShell --> |是| Shell["执行 ThickSolid"]
NeedShell --> |否| KeepOCCT["保持 OcctModel"]
```

**图示来源**
- [preprocess/ModelLoader.cpp:45-84](file://preprocess/ModelLoader.cpp#L45-L84)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:119-133](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L119-L133)

**章节来源**
- [preprocess/ModelLoader.cpp:45-84](file://preprocess/ModelLoader.cpp#L45-L84)
- [LibHsBaSlicer/Preprocess/model_preprocess.cpp:119-133](file://LibHsBaSlicer/Preprocess/model_preprocess.cpp#L119-L133)

### 典型使用场景
- 切片流水线：
  - 加载模型为 IModel。
  - 转换为 FullTopoModel 以进行拓扑切片。
  - 使用 Slice/SliceFast 输出 Polygons。

- 布尔流水线：
  - 若两端均为 OcctModel，直接使用 OCCT 布尔。
  - 若两端为 IglModel，启用 USE_CGAL 并使用 IglModel 的布尔函数。
  - 若布尔失败，回退到 CgalModel 的 Nef 布尔。
  - **新增** 使用 ModelLoader 的布尔运算接口，自动选择最优内核。

- 几何原语与拉伸：
  - 使用 CreateBox/Sphere/Cylinder/Cone/Torus 创建基础实体。
  - 使用 CreatePrime 将 2D 多边形沿方向拉伸为 3D 实体。

- **新增** 三角形网格导出：
  - 使用 GetModelTriangles 获取三角网格数据。
  - 适用于可视化、分析和与其他几何库集成。

- **新增** 文本 CSG 建模：
  - 使用 BuildCsgModel 从脚本构建几何模型。
  - 支持程序化几何设计和参数化建模。

[本节为概念性指导，不直接分析具体文件]

## 故障排除
- 空网格构造异常：
  - CgalModel 构造函数在输入为空时会抛出 RuntimeError。
  - IglModel 构造函数允许空网格，但布尔运算会返回空模型。

- 布尔运算失败：
  - IglModel 在未启用 USE_CGAL 时抛出 NotSupportedError。
  - IglModel 的布尔先尝试 igl::copyleft::cgal::mesh_boolean，失败后回退到 CgalModel 的 Nef 布尔。
  - CgalModel 的布尔基于 Nef_polyhedron_3，对输入网格质量敏感。
  - **新增** ModelLoader 的布尔运算会自动处理内核选择，但仍可能因输入几何问题失败。

- 抽壳不支持：
  - 如果源模型不是 OcctModel，ThickSolidModel 会抛出 RuntimeError。
  - 如果未启用 USE_OCCT，ThickSolidModel 会抛出 NotSupportedError。

- 文件格式不支持：
  - IglModel.Save 和 CgalModel.Save 对不支持的格式抛出 NotSupportedError。

- **新增** 三角形网格导出错误：
  - 未知模型名称会返回 NotFoundError。
  - 某些 B-Rep 模型可能无法正确三角化，导致导出失败。

- **新增** 文本 CSG 脚本错误：
  - 脚本语法错误或命令不存在会抛出 InvalidArgumentError。
  - 缺少 DRAW Test Harness 支持会抛出 NotSupportedError。
  - 脚本执行过程中出现 OCCT 错误会抛出 Standard_Failure。

**章节来源**
- [meshmodel/CgalModel.cpp:42-47](file://meshmodel/CgalModel.cpp#L42-L47)
- [meshmodel/IglModel.cpp:393-409](file://meshmodel/IglModel.cpp#L393-L409)
- [meshmodel/IglModel.cpp:195-246](file://meshmodel/IglModel.cpp#L195-L246)
- [preprocess/ModelLoader.cpp:193-212](file://preprocess/ModelLoader.cpp#L193-L212)
- [meshmodel/IglModel.cpp:72-99](file://meshmodel/IglModel.cpp#L72-L99)
- [meshmodel/CgalModel.cpp:133-157](file://meshmodel/CgalModel.cpp#L133-L157)
- [cadmodel/OcctCsg.cpp:238-290](file://cadmodel/OcctCsg.cpp#L238-L290)

## 结论
HsBaSlicer 的网格模型处理模块通过 IModel 统一接口，将 libigl、CGAL 和 OpenCascade 三种几何内核整合到一个一致的 API 中。FullTopoModel 为切片算法提供完整拓扑视图，IglModel 提供轻量高效的网格处理能力，CgalModel 提供高级几何与布尔运算，OcctModel 提供 CAD 级别的 B-Rep 建模与布尔。

**更新** 本次更新显著增强了预处理系统，新增了完整的 CSG 布尔运算功能、三角形网格导出能力和文本 CSG 支持。ModelLoader 提供了统一的接口，自动选择合适的几何内核，简化了开发者的工作流程。在实际工程中，应根据文件格式、几何需求和性能约束选择合适的内核，并通过预处理层自动路由。对于精度敏感的场景，建议使用 CgalModel 或 OcctModel；对于大规模网格处理，建议使用 IglModel；对于切片拓扑分析，建议使用 FullTopoModel；对于程序化几何设计，可以使用文本 CSG 功能。

[本节为总结性内容，不直接分析具体文件]