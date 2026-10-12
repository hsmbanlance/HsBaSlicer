--[[
    HsBaSlicer Pipeline Lua API — IDE Type Definitions (LuaLS / EmmyLua)
    =====================================================================
    按流水线步骤组织的类型提示组，供 sumneko-lua / LuaLS 语言服务器使用。

    文件: types/hsba_pipeline.d.lua
    配置: .luarc.json 引用本文件，samples/ 目录下脚本自动获得补全。

    工艺流水线步骤一览:
      FDM  : Preprocess → Slice → Support → Fill → PathOutput(G-code)
      SLA  : Preprocess → Slice → Floor → Support → Export(Zip)
      SLS  : Preprocess → Slice → Export(Zip)
      SLM  : Preprocess → Slice → Export(Zip)
      LOM  : Preprocess → Slice → Export(Zip)
      TDP  : Preprocess → Slice → Export(Zip)
      WAAM : Preprocess → Slice → RobotPath
      Custom: 由 Lua 脚本自由编排（所有步骤通过 HsBa 表暴露）
--]]

-- ============================================================================
-- 基础几何类型
-- ============================================================================

---@alias Point2D  { x: number, y: number }
---@alias Point3D  { x: number, y: number, z: number }
---@alias Quaternion { x: number, y: number, z: number, w: number }
---@alias Polygon   Point2D[]        -- 单个多边形（点数组）
---@alias Polygons  Polygon[]        -- 多边形集合（一层轮廓）
---@alias LayerList Polygons[]       -- 多层多边形集合

-- ============================================================================
-- 共享库 — 在 ALL 流水线环境中均可用
-- ============================================================================

---多边形布尔运算与几何操作
---@class PolygonOperations
---@field booleanOperation       fun(a: Polygons, b: Polygons, op: string): Polygons
---@field union                  fun(a: Polygons, b: Polygons): Polygons
---@field intersection           fun(a: Polygons, b: Polygons): Polygons
---@field difference             fun(a: Polygons, b: Polygons): Polygons
---@field xor                    fun(a: Polygons, b: Polygons): Polygons
---@field offsetOperation        fun(polys: Polygons, delta: number): Polygons
---@field convexHullOperation    fun(polys: Polygons): Polygons
---@field concaveHullOperation   fun(polys: Polygons, extraPoints: integer): Polygons
---@field area                   fun(poly: Polygon): number
---@field makeRectangle          fun(x: number, y: number, w: number, h: number): Polygons
---@field makeCircle             fun(cx: number, cy: number, r: number, segments?: integer): Polygons
---@field makeEllipse            fun(cx: number, cy: number, rx: number, ry: number, segments?: integer): Polygons
---@field makeRegularPolygon     fun(cx: number, cy: number, r: number, sides: integer): Polygons
---@field textToPolygons         fun(text: string, fontPath: string, size: number): Polygons
PolygonOperations = {}

---多边形填充模式生成器
---@class PolygonFill
---@field offsetFill         fun(polys: Polygons, spacing: number): Polygons
---@field lineFill           fun(polys: Polygons, spacing: number, angle: number, lineThickness: number): Polygons
---@field simpleZigzagFill   fun(polys: Polygons, spacing: number, angle: number, lineThickness: number): Polygons
---@field zigzagFill         fun(polys: Polygons, spacing: number, angle: number, lineThickness: number): Polygons
---@field compositeOffsetFill fun(polys: Polygons, spacing: number, offsetStep: number, outwardCount: integer, inwardCount: integer, mode: string, angle: number, lineThickness: number, opts?: table): Polygons
---@field hybridFill         fun(polys: Polygons, spacing: number, offsetStep: number, outwardCount: integer, inwardCount: integer, mode: string, angle: number, lineThickness: number, opts?: table): Polygons
---@field offsetOnly         fun(polys: Polygons, spacing: number): Polygons
PolygonFill = {}

---路径顺序优化器（减少空行程）

---区域级全组合图的一条边（i->j 的最短路径代价）
---@class FullGraphEdge
---@field from  integer   源区域 id
---@field to    integer   目标区域 id
---@field cost  number    最短路径代价

---区域级全组合图：所有区域两两之间的门径感知最短路径代价（供自定义排序算法）
---@class FullGraph
---@field vertices    integer[]           区域 id，按矩阵下标顺序
---@field distMatrix  (number|false)[][]  [i][j] 代价；false 表示不可达；对角为 0
---@field edges       FullGraphEdge[]     扁平可达边表（i ~= j）
---@field areaCount   integer             区域数量

---门级展开图的一个顶点（区域的入口或出口侧）
---@class ExpandedGraphVertex
---@field areaId  integer   所属区域 id
---@field gateId  integer   门禁索引
---@field isExit  boolean   true=出口侧，false=入口侧

---门级展开图的一条边（from->to，索引为 1 基）
---@class ExpandedGraphEdge
---@field from    integer   起点顶点下标（1 基）
---@field to      integer   终点顶点下标（1 基）
---@field weight  number    边代价

---门级展开图：所有入口/出口顶点及内部迁移边、区域间路由边（供细粒度自定义规划）
---@class ExpandedGraph
---@field vertices     ExpandedGraphVertex[]  全部入口/出口顶点
---@field edges        ExpandedGraphEdge[]    全部迁移/路由边（1 基端点下标）
---@field vertexCount  integer                顶点数量
---@field edgeCount    integer                边数量

---@class PathOptimizeInstance
---@field addRegion        fun(self: PathOptimizeInstance, regionId: integer, paths: Polygons)
---@field addPolygons      fun(self: PathOptimizeInstance, regionId: integer, polygons: Polygons)
---@field addRoute         fun(self: PathOptimizeInstance, fromId: integer, toId: integer, cost: number)
---@field optimizeOrder    fun(self: PathOptimizeInstance): integer[]
---@field buildPaths       fun(self: PathOptimizeInstance): Polygons
---@field buildPolygons    fun(self: PathOptimizeInstance): Polygons
---@field getFullGraph     fun(self: PathOptimizeInstance): FullGraph
---@field getExpandedGraph fun(self: PathOptimizeInstance): ExpandedGraph
---@field setCustomTour    fun(self: PathOptimizeInstance, tour: integer[], entryGates?: table<integer, integer>)

---@class PathOptimize
---@field new               fun(): PathOptimizeInstance
---@field optimizeRegions   fun(regions: Polygons[]): Polygons
---@field optimizePolygons  fun(regions: Polygons[]): Polygons
PathOptimize = {}

---支撑配置表（内置支撑阶段 config 全局变量的结构）
---@class SupportConfig
---@field overhang_angle_threshold  number
---@field layer_height              number
---@field support_gap               number
---@field support_diameter          number
---@field support_density           number
---@field support_pattern           integer   -- 0=Plane, 1=Tree, 2=Honeycomb
---@field tree_branch_angle         number
---@field tree_max_branch_radius    number
---@field honeycomb_cell_size       number

---支撑模块 userdata
---@class SupportObject

---支撑生成与检测工具表
---@class Support
---@field new_plane        fun(): SupportObject
---@field new_tree         fun(): SupportObject
---@field new_honeycomb    fun(): SupportObject
---@field new_sla          fun(): SupportObject
---@field new_lua          fun(script: string, funcName?: string): SupportObject
---@field new_lua_file     fun(path: string, funcName?: string): SupportObject
---@field generate         fun(obj: SupportObject, current: Polygons, prev: Polygons, height: number, cfg: SupportConfig): Polygons
---@field detect_overhang  fun(current: Polygons, prev: Polygons, height: number, angle: number): Polygons
---@field default_config   fun(): SupportConfig
Support = {}

---Zip 压缩包操作
---@class ZipperInstance
---@field AddFile      fun(self: ZipperInstance, name: string, filePath: string)
---@field AddByteFile  fun(self: ZipperInstance, name: string, data: string)
---@field Save         fun(self: ZipperInstance, path: string)

---@class Zipper
---@field new  fun(): ZipperInstance
Zipper = {}

---7z 压缩包操作（需编译启用 HSBA_USE_BIT7Z）
---@class Bit7zZipperInstance
---@field AddFile      fun(self: Bit7zZipperInstance, name: string, filePath: string)
---@field AddByteFile  fun(self: Bit7zZipperInstance, name: string, data: string)
---@field Save         fun(self: Bit7zZipperInstance, path: string)

---@class Bit7zZipper
---@field new  fun(): Bit7zZipperInstance
Bit7zZipper = {}

---加密/编码工具
---@class Cipher
---@field base64_encode  fun(data: string): string
---@field base64_decode  fun(data: string): string
---@field hex_encode     fun(data: string): string
---@field hex_decode     fun(data: string): string
Cipher = {}

---SQLite 数据库适配器
---@class SQLiteAdapterInstance
---@field Connect      fun(self: SQLiteAdapterInstance, path: string)
---@field Execute      fun(self: SQLiteAdapterInstance, sql: string)
---@field Query        fun(self: SQLiteAdapterInstance, sql: string): table[]
---@field Insert       fun(self: SQLiteAdapterInstance, table: string, data: table): integer
---@field Update       fun(self: SQLiteAdapterInstance, table: string, data: table, where: string): integer
---@field Delete       fun(self: SQLiteAdapterInstance, table: string, where: string): integer
---@field CreateTable  fun(self: SQLiteAdapterInstance, sql: string)

---@class SQLiteAdapter
---@field new  fun(): SQLiteAdapterInstance
SQLiteAdapter = {}

---MySQL 数据库适配器（需编译启用 HSBA_USE_MYSQL）
---@class MySQLAdapterInstance
---@field Connect      fun(self: MySQLAdapterInstance, host: string, user: string, pass: string, db: string, port?: integer)
---@field Execute      fun(self: MySQLAdapterInstance, sql: string)
---@field Query        fun(self: MySQLAdapterInstance, sql: string): table[]
---@field Insert       fun(self: MySQLAdapterInstance, table: string, data: table): integer
---@field Update       fun(self: MySQLAdapterInstance, table: string, data: table, where: string): integer
---@field Delete       fun(self: MySQLAdapterInstance, table: string, where: string): integer
---@field CreateTable  fun(self: MySQLAdapterInstance, sql: string)

---@class MySQLAdapter
---@field new  fun(): MySQLAdapterInstance
MySQLAdapter = {}

---PostgreSQL 数据库适配器（需编译启用 HSBA_USE_PGSQL）
---@class PostgreSQLAdapterInstance
---@field Connect      fun(self: PostgreSQLAdapterInstance, host: string, user: string, pass: string, db: string, port?: integer)
---@field Execute      fun(self: PostgreSQLAdapterInstance, sql: string)
---@field Query        fun(self: PostgreSQLAdapterInstance, sql: string): table[]
---@field Insert       fun(self: PostgreSQLAdapterInstance, table: string, data: table): integer
---@field Update       fun(self: PostgreSQLAdapterInstance, table: string, data: table, where: string): integer
---@field Delete       fun(self: PostgreSQLAdapterInstance, table: string, where: string): integer
---@field CreateTable  fun(self: PostgreSQLAdapterInstance, sql: string)

---@class PostgreSQLAdapter
---@field new  fun(): PostgreSQLAdapterInstance
PostgreSQLAdapter = {}

---工艺参数持久化存储
---@class ParamStoreInstance
---@field Save     fun(self: ParamStoreInstance, table: string, key: string, config: table): integer
---@field Load     fun(self: ParamStoreInstance, table: string, key: string): boolean, table|string
---@field List     fun(self: ParamStoreInstance, table: string): string[]
---@field Update   fun(self: ParamStoreInstance, table: string, key: string, config: table): integer
---@field Delete   fun(self: ParamStoreInstance, table: string, key: string): boolean

---@class ParamStore
---@field new  fun(dbPath: string): ParamStoreInstance
ParamStore = {}

-- ============================================================================
-- HsBa 算子表 — 仅 Custom 流水线环境可用
-- ============================================================================

---@class ModelInfo
---@field bbox_min  { x: number, y: number, z: number }
---@field bbox_max  { x: number, y: number, z: number }
---@field volume    number

---@class FillOptions
---@field spacing      number?   -- 填充间距, 默认 0.4
---@field mode         string?   -- "line"|"zigzag"|"simpleZigzag", 默认 "zigzag"
---@field angle        number?   -- 填充角度(度), 默认 45.0
---@field borderCount  integer?  -- 壁厚圈数, 默认 0

---@class SupportOptions
---@field layer_height         number?
---@field overhang_angle       number?
---@field support_gap          number?
---@field support_diameter     number?
---@field support_density      number?
---@field support_pattern      integer?
---@field tree_branch_angle    number?
---@field tree_max_branch_radius number?
---@field honeycomb_cell_size  number?
---@field interface_layers     integer?   -- FDM 专用
---@field interface_density    number?    -- FDM 专用
---@field tip_diameter         number?    -- SLA 专用
---@field raft_thickness       number?    -- SLA 专用

---@class FloorConfig
---@field raft_offset        number?
---@field border_width       number?
---@field fill_spacing       number?
---@field fill_angle_deg     number?
---@field border_count       integer?
---@field use_convex_hull    boolean?
---@field concave_hull_points integer?

---@class GcodeOptions
---@field layerHeight          number?
---@field lineWidth            number?
---@field printSpeed           number?
---@field travelSpeed          number?
---@field extrusionMultiplier  number?
---@field firmware             string?   -- "marlin"|"reprap"|"klipper"
---@field nozzleDiameter       number?
---@field filamentDiameter     number?
---@field nozzleTemp           number?
---@field bedTemp              number?
---@field retractLength        number?
---@field retractSpeed         number?

---@class LayerPathInput
---@field outlines   Polygons?
---@field fills      Polygons?
---@field supports   Polygons?
---@field zHeight    number?

---@class SpiralizeOptions
---@field layerHeight  number?
---@field startZ       number?
---@field zHeights     number?[]

---@class SlsPackageInput
---@field outlines   LayerList
---@field zHeights   number[]
---@field config     string       -- JSON
---@field output     string       -- 输出 zip 路径
---@field script     string       -- 导出 Lua 脚本路径
---@field func       string?      -- 导出函数名, 默认 "export_sls"

---@class WaamPackageInput
---@field outlines   LayerList
---@field zHeights   number[]
---@field weld       WaamWeldParams?
---@field robotType  integer?     -- 0=ABB, 1=KUKA, 2=FANUC
---@field beadWidth  number?
---@field config     string?      -- JSON
---@field output     string       -- 输出文件路径
---@field script     string?      -- 自定义代码生成脚本
---@field func       string?      -- 导出函数名, 默认 "export_waam"

---@class WaamWeldParams
---@field current        number?
---@field voltage        number?
---@field wireFeedSpeed  number?
---@field gasFlowRate    number?
---@field travelSpeed    number?
---@field process        integer?

---@class SlaPackageInput
---@field outlines        LayerList
---@field supports        LayerList?
---@field floor           Polygons?
---@field config          string?      -- JSON
---@field output          string       -- 输出 zip 路径
---@field imageWidth      integer?
---@field imageHeight     integer?
---@field imageExtension  string?      -- 默认 ".png"

---HsBa 全局算子表（Custom 流水线环境）
---@class HsBa
HsBa = {}

-- === 通用步骤 ===

---上报进度
---@param percent integer
---@param stage? string
function HsBa.progress(percent, stage) end

---设置总层数
---@param n integer
function HsBa.setLayers(n) end

---设置输出路径
---@param path string
function HsBa.setOutputPath(path) end

---读取文件内容
---@param path string
---@return string|nil
function HsBa.readFile(path) end

---写入文件
---@param path string
---@param content string
---@return boolean
function HsBa.writeFile(path, content) end

-- === Preprocess 步骤 ===

---加载 3D 模型到模型池
---@param name string
---@param path string
---@return boolean
function HsBa.loadModel(name, path) end

---获取模型信息
---@param name string
---@return ModelInfo
function HsBa.modelInfo(name) end

---平移模型
---@param name string
---@param offset Point3D
function HsBa.translateModel(name, offset) end

---旋转模型（四元数）
---@param name string
---@param quaternion Quaternion
function HsBa.rotateModel(name, quaternion) end

---缩放模型
---@param name string
---@param scale number|Point3D   -- 统一比例或逐轴
function HsBa.scaleModel(name, scale) end

---从模型池移除模型
---@param name string
function HsBa.removeModel(name) end

---获取当前模型池中所有模型名
---@return string[]
function HsBa.modelNames() end

-- === Slice 步骤 ===

---计算模型总层数
---@param name string
---@param layerHeight number
---@param firstLayerHeight number
---@return integer
function HsBa.layerCount(name, layerHeight, firstLayerHeight) end

---计算第 index 层的 Z 高度（0-based）
---@param index integer
---@param layerHeight number
---@param firstLayerHeight number
---@return number
function HsBa.layerZ(index, layerHeight, firstLayerHeight) end

---安全切片（闭合轮廓）
---@param name string
---@param z number
---@return Polygons
function HsBa.slice(name, z) end

---不安全切片（可能丢弃开放轮廓）
---@param name string
---@param z number
---@return Polygons
function HsBa.sliceUnsafe(name, z) end

-- === 坐标转换 ===

---浮点多边形转整数
---@param polys Polygons
---@return Polygons
function HsBa.toInt(polys) end

---整数多边形转浮点
---@param polys Polygons
---@return Polygons
function HsBa.toDouble(polys) end

-- === Fill 步骤 ===

---填充多边形
---@param polys Polygons
---@param options? FillOptions
---@return Polygons
function HsBa.fill(polys, options) end

-- === Support 步骤 ===

---生成 FDM 支撑
---@param layers LayerList
---@param config? SupportOptions
---@return LayerList
function HsBa.fdmSupport(layers, config) end

---生成 SLA 支撑
---@param layers LayerList
---@param config? SupportOptions
---@return LayerList
function HsBa.slaSupport(layers, config) end

-- === Floor 步骤 (SLA) ===

---生成 SLA 地板/Raft
---@param bottomPolygons Polygons
---@param config? FloorConfig
---@return Polygons
function HsBa.floor(bottomPolygons, config) end

-- === PathOutput 步骤 ===

---螺旋化外壁（FDM 花瓶模式）
---@param sections LayerList
---@param options? SpiralizeOptions
---@return Point3D[]
function HsBa.spiralize(sections, options) end

---生成 G-code
---@param layers LayerPathInput[]
---@param config GcodeOptions
---@return string
function HsBa.toGcode(layers, config) end

-- === Export 步骤 ===

---打包 SLS/SLM/LOM/TDP 输出（zip + Lua 导出脚本）
---@param input SlsPackageInput
---@return boolean
function HsBa.saveSlsPackage(input) end

---打包 WAAM 机器人路径输出
---@param input WaamPackageInput
---@return boolean, string?
function HsBa.saveWaamPackage(input) end

---打包 SLA 输出（层图 + 地板 + 支撑 + config → zip）
---@param input SlaPackageInput
---@return boolean
function HsBa.saveSlaPackage(input) end

---渲染多边形为图片
---@param polys Polygons
---@param width integer
---@param height integer
---@param path string
---@return boolean
function HsBa.renderImage(polys, width, height, path) end

-- ============================================================================
-- Custom 流水线环境 — 注入的全局变量
-- ============================================================================

---入口函数名（默认 "run_pipeline"，可通过 C++ entry_func 覆盖）
---@type string
pipeline_entry = "run_pipeline"

---C++ 传入的 JSON 配置字符串
---@type string
pipeline_config = ""

---默认输出路径
---@type string
output_path = ""

---模型名
---@type string
model_name = ""

---模型文件路径
---@type string
model_path = ""

-- ============================================================================
-- 内置阶段脚本环境 — FDM Support 步骤
-- ============================================================================
-- 入口函数: generate_support()
-- 可用库: PolygonOperations, Support, PathOptimize

---FDM/SLA Support 阶段: 当前层多边形
---@type Polygons
current_layer = {}

---FDM/SLA Support 阶段: 上一层多边形（首层为空）
---@type Polygons
prev_layer = {}

---FDM/SLA Support 阶段: 层高 (mm)
---@type number
layer_height = 0.2

---FDM/SLA Support 阶段: 支撑配置（Support 阶段中为 SupportConfig, Export 阶段中为 ExportConfig）
---@type SupportConfig|ExportConfig
config = {}

---[FDM Support 步骤] 自定义支撑入口函数
---@return Polygons 支撑截面多边形
function generate_support() end

-- ============================================================================
-- 内置阶段脚本环境 — FDM Fill 步骤
-- ============================================================================
-- 入口函数: generate_fill(current_layer)
-- 可用库: PolygonOperations, PolygonFill, PathOptimize
-- 注意: 胶水以函数参数传入, 不使用全局变量

---[FDM Fill 步骤] 自定义填充入口函数
---@param polys Polygons 当前层多边形（已扣除壁厚区域）
---@return Polygons 填充多边形
function generate_fill(polys) end

-- ============================================================================
-- 内置阶段脚本环境 — SLA Floor 步骤
-- ============================================================================
-- 入口函数: generate_floor(bottom_layer, config)
-- 可用库: PolygonOperations, PolygonFill
-- 注意: 胶水以函数参数传入, 不使用全局变量

---[SLA Floor 步骤] 自定义地板入口函数
---@param bottomLayer Polygons 底层多边形
---@param floorCfg FloorConfig 地板配置
---@return Polygons 地板多边形
function generate_floor(bottomLayer, floorCfg) end

-- ============================================================================
-- 内置阶段脚本环境 — SLS/SLM/LOM/TDP Export 步骤
-- ============================================================================
-- 入口函数: export_sls() / export_slm() / export_lom() / export_tdp()
-- 可用库: Zipper, Bit7zZipper, Cipher, SQLiteAdapter, MySQLAdapter, PostgreSQLAdapter
-- 注意: 必须返回 table 或 nil, 切勿返回字符串（会覆盖 zip 文件）

---@class ExportConfig
---@field path string        -- "config.json"
---@field configStr string   -- JSON 配置内容

---@class ExportImage
---@field path string        -- "layers/0.json" 等
---@field data string        -- 多边形 JSON 数据

---Export 步骤: 配置信息 —— 与 Support 阶段共享 config 全局, 类型为 ExportConfig|SupportConfig
---@see config

---Export 步骤: 层图片数据数组
---@type ExportImage[]
images = {}

---Export 步骤: 函数名标识
---@type string
funcName = ""

---[SLS Export 步骤] SLS 导出入口函数
---@return table { success: boolean, export_path: string }
function export_sls() end

---[SLM Export 步骤] SLM 导出入口函数
---@return table { success: boolean, export_path: string }
function export_slm() end

---[LOM Export 步骤] LOM 导出入口函数
---@return table { success: boolean, export_path: string }
function export_lom() end

---[TDP Export 步骤] TDP/3DP 导出入口函数
---@return table { success: boolean, export_path: string }
function export_tdp() end

-- ============================================================================
-- 内置阶段脚本环境 — SLA Export 步骤
-- ============================================================================
-- SLA 导出通过 ImagesPath.Save 运行, 与 SLS Export 环境一致:
--   config, images, output_path, Zipper, Cipher 等
-- 入口函数: export_sla()

---[SLA Export 步骤] SLA 导出入口函数
---@return table { success: boolean, export_path: string }
function export_sla() end

-- ============================================================================
-- 内置阶段脚本环境 — WAAM RobotPath 步骤
-- ============================================================================
-- 入口函数: export_waam()
-- 返回值: string（完整的机器人程序文本）

---WAAM 步骤: 头部说明
---@type string
header = ""

---WAAM 步骤: 起始程序名
---@type string
startProgramFunc = ""

---WAAM 步骤: 结束程序名
---@type string
endProgramFunc = ""

---@class WaamPoint
---@field end       Point3D
---@field middle    Point3D
---@field velocity  number
---@field type      string   -- "MoveJ"|"MoveL"

---WAAM 步骤: 路径点数组
---@type WaamPoint[]
points = {}

---[WAAM RobotPath 步骤] WAAM 机器人代码生成入口函数
---@return string 完整的机器人程序文本
function export_waam() end

-- ============================================================================
-- 流水线步骤汇总（IDE 导航用注解组）
-- ============================================================================
-- 以下虚拟类不对应实际运行时对象，仅用于 IDE 大纲/文档分组导航。

---@class FdmPreprocess       [FDM] 加载/变换模型: HsBa.loadModel, modelInfo, translate/rotate/scaleModel
---@class FdmSlice            [FDM] 逐层切片: HsBa.layerCount, layerZ, slice
---@class FdmSupportStage     [FDM] 支撑生成: generate_support() / HsBa.fdmSupport
---@class FdmFillStage        [FDM] 填充: generate_fill(polys) / HsBa.fill
---@class FdmPathOutput       [FDM] G-code 输出: HsBa.toGcode, spiralize, writeFile

---@class SlaPreprocess       [SLA] 加载/变换模型: HsBa.loadModel, modelInfo
---@class SlaSlice            [SLA] 逐层切片: HsBa.layerCount, layerZ, slice
---@class SlaFloorStage       [SLA] 地板/Raft: generate_floor(bottom, cfg) / HsBa.floor
---@class SlaSupportStage     [SLA] 支撑生成: generate_support() / HsBa.slaSupport
---@class SlaExportStage      [SLA] 渲染打包: HsBa.saveSlaPackage / export_sla()

---@class SlsPreprocess       [SLS] 加载/变换模型
---@class SlsSlice            [SLS] 逐层切片
---@class SlsExportStage      [SLS] 导出打包: HsBa.saveSlsPackage / export_sls()

---@class SlmPreprocess       [SLM] 加载/变换模型
---@class SlmSlice            [SLM] 逐层切片
---@class SlmExportStage      [SLM] 导出打包: HsBa.saveSlsPackage / export_slm()

---@class LomPreprocess       [LOM] 加载/变换模型（逐板材）
---@class LomSlice            [LOM] 逐板材切片
---@class LomExportStage      [LOM] 导出打包: HsBa.saveSlsPackage / export_lom()

---@class TdpPreprocess       [TDP/3DP] 加载/变换模型
---@class TdpSlice            [TDP/3DP] 逐层喷射轮廓切片
---@class TdpExportStage      [TDP/3DP] 导出打包: HsBa.saveSlsPackage / export_tdp()

---@class WaamPreprocess      [WAAM] 加载/变换模型
---@class WaamSlice           [WAAM] 熔覆轮廓切片
---@class WaamRobotPath       [WAAM] 机器人代码: HsBa.saveWaamPackage / export_waam()

---@class CustomPipeline      [Custom] 完全 Lua 驱动的自定义流水线: run_pipeline()
