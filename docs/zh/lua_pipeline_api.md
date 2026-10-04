# 流水线 Lua API 参考

本文档汇总 **各类切片流水线中可直接调用的 Lua API**，面向编写 Lua 脚本的用户。它不同于「Lua 扩展函数注册」章节——后者讲的是如何从 C++ 侧*注入*新函数，而本文讲的是脚本内部*已可使用*的函数、全局变量与调用约定。

> 相关源码：`LibHsBaSlicer/Extends/lua_pipeline.cpp`（`HsBa` 算子表与自定义流水线环境）、`2D/LuaAdapter.cpp`、`2D/PolygonFill.cpp`、`support/LuaAdapter.cpp`、`LibHsBaSlicer/Path/path_optimizer.cpp`、`cipher/LuaAdapter.cpp`、`fileoperator/LuaAdapter.cpp`。

## 目录

- [两种 Lua 运行环境](#两种-lua-运行环境)
- [通用几何数据类型](#通用几何数据类型)
- [A. 自定义流水线环境（`HsBa` 算子表）](#a-自定义流水线环境hsba-算子表)
- [B. 通用共享库（所有环境均可用）](#b-通用共享库所有环境均可用)
- [C. 内置流水线的阶段替换脚本](#c-内置流水线的阶段替换脚本)
- [流水线可用性矩阵](#流水线可用性矩阵)
- [完整示例](#完整示例)

---

## 两种 Lua 运行环境

HsBaSlicer 中的 Lua 脚本按用途分为两类：

| 环境 | 用途 | 触发方式 | 特征 API |
|------|------|----------|----------|
| **自定义流水线（Custom Pipeline）** | 由脚本自行决定「加载→切片→支撑→填充→路径→写文件」的全部阶段与顺序 | C++ 侧调用 `run_pipeline`（或 `pipeline_entry` 指定的入口名） | 全局 `HsBa` 算子表 + 上下文全局变量 |
| **内置流水线阶段替换（Stage Hook）** | 仅替换内置流水线中某个阶段（支撑/填充/地板/导出）的算法，阶段顺序仍由 C++ 固定 | 由对应内置流水线在阶段处加载脚本并调用约定的函数 | 该阶段注入的少量全局变量 + 通用共享库 |

两类环境都会注册 [通用共享库](#b-通用共享库所有环境均可用)（`PolygonOperations`、`Support`、`PolygonFill`、`PathOptimize`、`Cipher`、`Zipper`、数据库适配器等）。区别仅在于 `HsBa` 算子表**只在自定义流水线环境**中存在。

---

## 通用几何数据类型

所有 API 使用统一的 Lua table 表示几何数据：

| 类型 | Lua 表示 | 说明 |
|------|----------|------|
| 点 Point | `{ x = 1.0, y = 2.0 }` | 字段名固定为 `x` / `y`（单位 mm，double） |
| 多边形 Polygon | `{ {x=..,y=..}, {x=..,y=..}, ... }` | 点的有序数组，闭合轮廓 |
| 多边形集合 Polygons | `{ polygon1, polygon2, ... }` | 多边形的数组；孔洞作为独立多边形存在 |
| 层列表 Layers | `{ polygons_layer0, polygons_layer1, ... }` | 逐层的多边形集合数组 |
| 3D 点 | `{ x = .., y = .., z = .. }` | 用于 `spiralize` 返回的路径 |

> **整数化说明**：布尔/填充运算内部按整数精度处理。`HsBa.toInt` / `HsBa.toDouble` 用于在整型多边形与浮点多边形之间转换；`PolygonOperations`、`PolygonFill` 接收并返回整型化数据，`HsBa.slice` / `HsBa.fill` 返回浮点多边形表。

---

## A. 自定义流水线环境（`HsBa` 算子表）

自定义流水线是唯一拥有全局 `HsBa` 表的环境。脚本必须定义一个**入口函数**，C++ 侧调用它并据其返回值判定成败：

- 入口函数名默认为 `run_pipeline`，可由 C++ 侧的 `pipeline_entry` 全局变量覆盖。
- 返回**非空字符串** → 成功，该字符串作为结果上报 C++；返回 `true` → 成功但无结果文本；返回 `false`/`nil` 或抛出 Lua 错误 → 失败。

### A.1 C++ 注入的上下文全局变量

| 全局变量 | 类型 | 含义 |
|----------|------|------|
| `pipeline_config` | string | C++ 传入的 JSON 配置字符串（原样透传） |
| `output_path` | string | C++ 传入的默认输出路径（可能为空串） |
| `model_name` | string | C++ 传入的模型名（可能为空串） |
| `model_path` | string | C++ 传入的模型文件路径（可能为空串） |
| `pipeline_entry` | string | 入口函数名（默认 `run_pipeline`） |

> C++ 还可通过 `pipeline_lua_source` 预置一段前置脚本（prelude），在其中定义如 `machine` 等自定义全局表，随后再加载主脚本文件。

### A.2 控制与文件 IO

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.progress(percent[, stage])` | — | 向 C++ 上报进度百分比与阶段名 |
| `HsBa.setLayers(n)` | — | 上报总层数（回填到 C++ 结果 `total_layers`） |
| `HsBa.setOutputPath(path)` | — | 上报实际输出路径（回填到 C++ 结果 `output_path`） |
| `HsBa.readFile(path)` | string \| nil | 读取文件内容，失败返回 `nil` |
| `HsBa.writeFile(path, content)` | true | 以二进制覆盖写入文件，无法打开时抛出 Lua 错误 |

### A.3 模型管理

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.loadModel(name, path)` | true | 加载模型到模型池（同名已存在则复用）；失败抛出错误 |
| `HsBa.modelInfo(name)` | table | `{ bbox_min={x,y,z}, bbox_max={x,y,z}, volume }` |
| `HsBa.translateModel(name, {x,y,z})` | — | 平移模型 |
| `HsBa.rotateModel(name, {x,y,z,w})` | — | 按四元数旋转模型 |
| `HsBa.scaleModel(name, s)` 或 `(name, {x,y,z})` | — | 缩放模型（标量或逐轴） |
| `HsBa.removeModel(name)` | — | 从模型池移除 |
| `HsBa.modelNames()` | table | 返回模型池中所有模型名的数组 |

### A.4 切片

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.layerCount(name, layerHeight, firstLayerHeight)` | int | 依据模型高度与层高计算总层数 |
| `HsBa.layerZ(index0based, layerHeight, firstLayerHeight)` | number | 第 `index`（0 基）层的 Z 高度 |
| `HsBa.slice(name, z)` | polygons | 在高度 `z` 处切片，返回安全闭合轮廓（浮点多边形） |
| `HsBa.sliceUnsafe(name, z)` | polygons | 归一化的非安全切片，可能丢弃开口轮廓 |

### A.5 坐标转换与填充

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.toInt(polygons)` | polygons | 浮点多边形 → 整型多边形 |
| `HsBa.toDouble(intPolygons)` | polygons | 整型多边形 → 浮点多边形 |
| `HsBa.fill(polygons[, cfg])` | polygons | 填充。`cfg = { spacing=0.4, mode="zigzag"\|"line"\|"simpleZigzag", angle=45.0, borderCount=0 }`；`borderCount>0` 时生成壁厚+填充 |

### A.6 支撑与地板

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.fdmSupport(layers, cfg)` | layers | 逐层 FDM 支撑。`cfg` 字段见下 |
| `HsBa.slaSupport(layers, cfg)` | layers | 逐层 SLA 支撑 |
| `HsBa.floor(bottomPolygons, cfg)` | polygons | 生成 SLA 底部地板/raft |

`fdmSupport` / `slaSupport` 的 `cfg` 常用字段：`overhang_angle`(45)、`layer_height`(0.2)、`support_gap`(0.5)、`support_diameter`(2.0)、`support_density`(0.3)、`support_pattern`(int)、`tree_branch_angle`(30)、`tree_max_branch_radius`(5)、`honeycomb_cell_size`(5)。FDM 额外支持 `interface_layers`、`interface_density`；SLA 额外支持 `tip_diameter`、`raft_thickness`。

`floor` 的 `cfg` 字段：`raft_offset`、`border_width`、`fill_spacing`、`fill_angle_deg`、`border_count`、`use_convex_hull`、`concave_hull_points`。

### A.7 路径与 G-code

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.spiralize(sections[, cfg])` | path | 将逐层闭合轮廓化为单一连续 3D 螺旋路径（{x,y,z} 数组）。`cfg = { layerHeight=0.4, startZ=0, zHeights={...} }`（`zHeights` 优先）。适用于 FDM/WAAM/3DP 连续挤出 |
| `HsBa.toGcode(layers, cfg)` | string | 生成 G-code。`layers` 每元素为 `{ outlines=, fills=, supports=, zHeight= }`；`cfg = { layerHeight, lineWidth, printSpeed, travelSpeed, extrusionMultiplier, firmware="marlin"\|"reprap"\|"klipper", nozzleDiameter, filamentDiameter, nozzleTemp, bedTemp, retractLength, retractSpeed }` |

### A.8 打包与导出

| 调用 | 返回 | 说明 |
|------|------|------|
| `HsBa.saveSlsPackage(tbl)` | bool | 打包 SLS 导出。`tbl = { outlines=layers, zHeights={...}, config="json", output="x.zip", script="export.lua", func="export_sls" }`（`output`、`script` 必填，`func` 默认 `export_sls`） |
| `HsBa.saveWaamPackage(tbl)` | bool[, err] | 打包 WAAM 机器人程序。`tbl = { outlines=layers, zHeights={...}, weld={current,voltage,wireFeedSpeed,gasFlowRate,travelSpeed,process}, robotType=0, beadWidth=1.2, config="json", output="x.txt", script=, func="export_waam" }`（`output` 必填） |
| `HsBa.saveSlaPackage(tbl)` | bool | 打包 SLA 图像。`tbl = { outlines=layers, supports=layers, floor=polygons, config="json", output="x.zip", imageWidth, imageHeight, imageExtension=".png" }`（`output` 必填） |
| `HsBa.renderImage(polygons, width, height, path)` | bool | 将多边形渲染为图像文件 |

---

## B. 通用共享库（所有环境均可用）

以下全局库在**自定义流水线与内置阶段脚本中均可调用**。

### B.1 `PolygonOperations`（2D 布尔与构造）

| 函数 | 说明 |
|------|------|
| `booleanOperation(a, b, op)` | 通用布尔运算，`op` 为 union/intersection/difference/xor |
| `union(a, b)` / `intersection(a, b)` / `difference(a, b)` / `xor(a, b)` | 并/交/差/异或 |
| `offsetOperation(polys, delta)` | 内外偏移（delta 正为外扩） |
| `convexHullOperation(polys)` / `concaveHullOperation(polys[, ...])` | 凸包 / 凹包 |
| `area(polygon)` | 计算面积 |
| `makeRectangle(xmin, ymin, xmax, ymax)` | 生成矩形 |
| `makeCircle(cx, cy, r)` | 生成圆形 |
| `makeEllipse(...)` | 生成椭圆 |
| `makeRegularPolygon(...)` | 生成正多边形 |
| `textToPolygons(...)` | 文本转多边形轮廓 |

> `dumpPolygon` / `dumpPolygons` 仅在启用 `HSBA_POLYGON_DUMP` 编译时可用。

### B.2 `PolygonFill`（填充算法）

`offsetFill` / `lineFill` / `simpleZigzagFill` / `zigzagFill` / `compositeOffsetFill` / `hybridFill` / `offsetOnly` —— 输入整型多边形集合与间距/角度参数，返回填充路径多边形。

### B.3 `PathOptimize`（路径顺序优化）

| 调用 | 说明 |
|------|------|
| `PathOptimize.new()` | 创建优化器对象，方法：`addRegion(id, paths)`、`addPolygons(id, polys)`、`addRoute(...)`、`optimizeOrder()`、`buildPaths()`、`buildPolygons()` |
| `PathOptimize.optimizeRegions(regions)` | 填充结果模式一键优化，返回完整填充路径 |
| `PathOptimize.optimizePolygons(regions)` | 多边形模式一键优化（填充前执行），返回优化顺序的多边形集合 |

### B.4 `Support`（支撑生成工具）

| 调用 | 说明 |
|------|------|
| `Support.new_plane()` / `new_tree()` / `new_honeycomb()` / `new_sla()` | 创建对应类型的支撑生成器 |
| `Support.new_lua(path, func)` / `new_lua_file(...)` | 基于 Lua 脚本的自定义支撑生成器 |
| `Support.generate(obj, current_layer, prev_layer, layer_height, cfg)` | 用指定生成器生成支撑，返回多边形 |
| `Support.detect_overhang(current, prev, height, angle)` | 检测悬垂区域 |
| `Support.default_config()` | 返回默认支撑配置表 |

### B.5 `Cipher`（编解码）

`Cipher.base64_encode(s)` / `base64_decode(s)` / `hex_encode(s)` / `hex_decode(s)`。

### B.6 `Zipper` / `Bit7zZipper`（压缩打包）

| 调用 | 说明 |
|------|------|
| `Zipper.new()` | 创建 zip 打包对象 |
| `z:AddFile(name, path)` | 添加磁盘文件 |
| `z:AddByteFile(name, data)` | 添加内存字节内容 |
| `z:Save(path)` | 写出压缩包 |
| `Bit7zZipper.new(format, dll_path)` | 7z 格式压缩，`format` ∈ `Zip`/`SevenZip`/`XZ`/`BZIP2`/`GZIP`/`TAR`/`TarGz`/`TarXz`（需编译启用 `HSBA_USE_BIT7Z`）；`TarGz`/`TarXz` 可在一次 `Save` 调用中直接生成 `.tar.gz`/`.tar.xz` 压缩包 |

### B.7 数据库适配器（`SQLiteAdapter` / `MySQLAdapter` / `PostgreSQLAdapter`）

三者方法一致（MySQL/PostgreSQL 需编译启用对应宏）：

```lua
local db = SQLiteAdapter.new()
db:Connect("path_or_dsn")
db:CreateTable("...")          -- 或 db:Execute("SQL...")
db:Insert("table", {col = val})
db:Update("table", {col = val}, "where...")
local rows = db:Query("SELECT ...")   -- 返回行数组
db:Delete("table", "where...")
```

方法：`Connect`、`Execute`、`Query`、`Insert`、`Update`、`Delete`、`CreateTable`。

### B.8 `ParamStore`（参数持久化）

基于反射的流水线配置存取。`ParamStore.new(...)` 创建实例，实例方法：`EnsureSchema`、`Save`、`Load`、`List`、`Update`、`Delete`。

---

## C. 内置流水线的阶段替换脚本

内置流水线在特定阶段加载用户脚本以替换默认算法。**每个阶段注入不同的全局变量、约定不同的入口函数**。

### C.1 支撑阶段（FDM/SLA 自定义支撑）

由 `LuaSupport` 环境提供，注入全局变量：

| 全局变量 | 类型 | 含义 |
|----------|------|------|
| `current_layer` | polygons | 当前层多边形 |
| `prev_layer` | polygons | 上一层多边形（首层为空） |
| `layer_height` | number | 层高（mm） |
| `config` | table | 支撑配置（字段同 [A.6](#a6-支撑与地板)） |

- **约定入口函数**：`generate_support()`，返回支撑截面多边形表（格式同 `current_layer`）。
- 可用库：`PolygonOperations`、`Support`，以及 C++ 注入的 2D/3D 扩展函数池。
- 结果也可通过全局 `support_polys` 返回。

### C.2 填充阶段

- **约定入口函数**：`generate_fill(current_layer)`——当前层多边形以**参数**传入（已扣除壁厚区域），返回填充多边形表。
- 该阶段**不注入任何全局变量**（无层号/配置），请使用脚本内默认值。
- 脚本 chunk 只定义函数，**不要末尾自调用**。
- 可用库：`PolygonOperations`、`PolygonFill`、`PathOptimize`。

### C.3 地板阶段（SLA）

- 可用库：`PolygonOperations`、`PolygonFill`。
- 用于自定义 SLA 底部地板/raft 生成逻辑。

### C.4 导出阶段（SLS / SLA / SLM / LOM / 3DP / WAAM）

导出脚本在打包阶段被调用，注入全局变量：

| 全局变量 | 类型 | 含义 |
|----------|------|------|
| `config` | table | `{ path="config.json", configStr="<JSON 内容>" }` |
| `images` | array | 每元素为 `{ path="layers/N.json", data="<多边形 JSON>" }` |
| `output_path` | string | 输出文件路径 |

- **约定入口函数**：SLS 为 `export_sls()`（默认函数名，可由 `func` 字段覆盖），WAAM 为 `export_waam()`，其余按各工艺导出约定。
- 返回值：table `{ success = true/false, export_path = "..." }`。
- 可用库：`Zipper`、`Cipher`、`Bit7zZipper`（可选）、`SQLiteAdapter`/`MySQLAdapter`/`PostgreSQLAdapter`、`ParamStore`。

> **常见陷阱**：导出脚本返回值类型错误（例如误返回非预期结构）会导致 zip 文件被覆盖或写空；请严格返回 `{ success=..., export_path=... }`，并保证内部对象方法名大小写与 C++ 绑定一致。

---

## 流水线可用性矩阵

| API | Custom 自定义流水线 | 内置阶段脚本 |
|-----|:-------------------:|:------------:|
| `HsBa` 算子表 | ✅（完整） | ❌ |
| 上下文全局（`pipeline_config` 等） | ✅ | ❌ |
| `PolygonOperations` | ✅ | ✅（支撑/填充/地板/导出视阶段而定） |
| `Support` | ✅ | ✅（支撑阶段） |
| `PolygonFill` | ✅ | ✅（填充/地板阶段） |
| `PathOptimize` | ✅ | ✅（填充阶段） |
| `Cipher` | ✅ | ✅（导出阶段） |
| `Zipper` / `Bit7zZipper` | ✅ | ✅（导出阶段） |
| `SQLiteAdapter` / `MySQLAdapter` / `PostgreSQLAdapter` | ✅ | ✅（导出阶段） |
| `ParamStore` | ✅ | ✅（导出阶段） |

> `MySQLAdapter`、`PostgreSQLAdapter`、`Bit7zZipper` 分别在启用 `HSBA_USE_MYSQL`、`HSBA_USE_PGSQL`、`HSBA_USE_BIT7Z` 时注册。

---

## 完整示例

一个最小可运行的自定义流水线脚本：

```lua
-- my_pipeline.lua —— 由 Lua 完全驱动的流水线
function run_pipeline()
    local name = (model_name and #model_name > 0) and model_name or "part"
    local path = (model_path and #model_path > 0) and model_path or "models/bunny.stl"
    local out  = (output_path and #output_path > 0) and output_path or "output/part.gcode"

    HsBa.progress(1, "加载模型")
    HsBa.loadModel(name, path)
    local info  = HsBa.modelInfo(name)
    local zbase = info.bbox_min.z

    local lh, flh = 0.2, 0.25
    local layers  = HsBa.layerCount(name, lh, flh)
    HsBa.setLayers(layers)

    local outlines, fills, zs = {}, {}, {}
    for i = 0, layers - 1 do
        local z = HsBa.layerZ(i, lh, flh)
        local polys = HsBa.slice(name, zbase + z)
        outlines[i + 1] = polys
        zs[i + 1]       = z
        fills[i + 1]    = HsBa.fill(polys, { spacing = 0.4, angle = 45, mode = "zigzag", borderCount = 2 })
    end

    local path_layers = {}
    for i = 1, layers do
        path_layers[i] = { outlines = outlines[i], fills = fills[i], supports = {}, zHeight = zs[i] }
    end

    local gcode = HsBa.toGcode(path_layers, {
        layerHeight = lh, lineWidth = 0.45, printSpeed = 60, travelSpeed = 150,
        firmware = "marlin", nozzleDiameter = 0.4, filamentDiameter = 1.75,
    })

    HsBa.writeFile(out, gcode)
    HsBa.setOutputPath(out)
    HsBa.removeModel(name)
    HsBa.progress(100, "完成")

    return string.format("完成：%d 层 → %s", layers, out)
end
```

各工艺内置流水线的阶段脚本示例可参考仓库：`samples/Custom/scripts/`、`samples/FDM/scripts/`、`samples/SLA/scripts/`、`samples/SLS/scripts/`、`samples/WAAM/scripts/` 等目录下的 `.lua` 脚本。
