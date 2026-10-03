# Pipeline Lua API Reference

This document collects the **Lua APIs available inside the various slicing pipelines**, written for users who author Lua scripts. It differs from the "Lua extension function registration" sections — those describe how to *inject* new functions from the C++ side, whereas this one describes the functions, global variables, and calling conventions already *usable inside a script*.

> Source of truth: `LibHsBaSlicer/Extends/lua_pipeline.cpp` (the `HsBa` operation table and the custom-pipeline environment), `2D/LuaAdapter.cpp`, `2D/PolygonFill.cpp`, `support/LuaAdapter.cpp`, `LibHsBaSlicer/Path/path_optimizer.cpp`, `cipher/LuaAdapter.cpp`, `fileoperator/LuaAdapter.cpp`.

## Table of Contents

- [Two Lua runtime environments](#two-lua-runtime-environments)
- [Common geometry data types](#common-geometry-data-types)
- [A. Custom-pipeline environment (the `HsBa` operation table)](#a-custom-pipeline-environment-the-hsba-operation-table)
- [B. Shared libraries (available in every environment)](#b-shared-libraries-available-in-every-environment)
- [C. Built-in pipeline stage hooks](#c-built-in-pipeline-stage-hooks)
- [Pipeline availability matrix](#pipeline-availability-matrix)
- [Full example](#full-example)

---

## Two Lua runtime environments

Lua scripts in HsBaSlicer fall into two categories by purpose:

| Environment | Purpose | Trigger | Characteristic API |
|-------------|---------|---------|--------------------|
| **Custom Pipeline** | The script itself decides the full stage order (load → slice → support → fill → path → write) | C++ calls `run_pipeline` (or the name given by the `pipeline_entry` global) | The global `HsBa` operation table + context globals |
| **Built-in stage hook** | Replace only the algorithm of one stage (support / fill / floor / export) of a built-in pipeline; the stage order stays fixed in C++ | The corresponding built-in pipeline loads the script and calls the agreed function | A few stage-specific globals + shared libraries |

Both environments register the [shared libraries](#b-shared-libraries-available-in-every-environment) (`PolygonOperations`, `Support`, `PolygonFill`, `PathOptimize`, `Cipher`, `Zipper`, database adapters, etc.). The only difference is that the `HsBa` operation table exists **exclusively in the custom-pipeline environment**.

---

## Common geometry data types

All APIs use a uniform Lua-table representation for geometry:

| Type | Lua representation | Notes |
|------|--------------------|-------|
| Point | `{ x = 1.0, y = 2.0 }` | Field names fixed as `x` / `y` (mm, double) |
| Polygon | `{ {x=..,y=..}, {x=..,y=..}, ... }` | Ordered array of points, a closed contour |
| Polygons | `{ polygon1, polygon2, ... }` | Array of polygons; holes exist as separate polygons |
| Layers | `{ polygons_layer0, polygons_layer1, ... }` | Per-layer array of polygon sets |
| 3D point | `{ x = .., y = .., z = .. }` | Used by the path returned from `spiralize` |

> **Integerization note**: boolean/fill operations run at integer precision internally. `HsBa.toInt` / `HsBa.toDouble` convert between integer polygons and floating-point polygons; `PolygonOperations` and `PolygonFill` accept and return integerized data, while `HsBa.slice` / `HsBa.fill` return floating-point polygon tables.

---

## A. Custom-pipeline environment (the `HsBa` operation table)

The custom pipeline is the only environment that owns the global `HsBa` table. The script must define an **entry function** that C++ calls and uses to decide success:

- The entry function name defaults to `run_pipeline`, overridable via the `pipeline_entry` global injected by C++.
- Returning a **non-empty string** → success, the string is reported to C++ as the result; returning `true` → success without result text; returning `false`/`nil` or raising a Lua error → failure.

### A.1 Context globals injected by C++

| Global | Type | Meaning |
|--------|------|---------|
| `pipeline_config` | string | JSON config string passed from C++ (passed through verbatim) |
| `output_path` | string | Default output path from C++ (may be empty) |
| `model_name` | string | Model name from C++ (may be empty) |
| `model_path` | string | Model file path from C++ (may be empty) |
| `pipeline_entry` | string | Entry function name (default `run_pipeline`) |

> C++ can also preset a prelude script via `pipeline_lua_source` (e.g. defining a `machine` table) that runs before the main script file is loaded.

### A.2 Control & file IO

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.progress(percent[, stage])` | — | Report progress percentage and stage name to C++ |
| `HsBa.setLayers(n)` | — | Report total layer count (mirrored into the C++ result `total_layers`) |
| `HsBa.setOutputPath(path)` | — | Report the actual output path (mirrored into the C++ result `output_path`) |
| `HsBa.readFile(path)` | string \| nil | Read file content, returns `nil` on failure |
| `HsBa.writeFile(path, content)` | true | Overwrite file in binary mode; raises a Lua error if it cannot be opened |

### A.3 Model management

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.loadModel(name, path)` | true | Load a model into the pool (reuses an existing same-name model); raises an error on failure |
| `HsBa.modelInfo(name)` | table | `{ bbox_min={x,y,z}, bbox_max={x,y,z}, volume }` |
| `HsBa.translateModel(name, {x,y,z})` | — | Translate the model |
| `HsBa.rotateModel(name, {x,y,z,w})` | — | Rotate the model by a quaternion |
| `HsBa.scaleModel(name, s)` or `(name, {x,y,z})` | — | Scale the model (uniform or per-axis) |
| `HsBa.removeModel(name)` | — | Remove from the model pool |
| `HsBa.modelNames()` | table | Array of all model names in the pool |

### A.4 Slicing

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.layerCount(name, layerHeight, firstLayerHeight)` | int | Compute total layer count from model height and layer heights |
| `HsBa.layerZ(index0based, layerHeight, firstLayerHeight)` | number | Z height of the (0-based) layer `index` |
| `HsBa.slice(name, z)` | polygons | Slice at height `z`, returning safe closed contours (float polygons) |
| `HsBa.sliceUnsafe(name, z)` | polygons | Normalized unsafe slice, may drop open contours |

### A.5 Coordinate conversion & fill

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.toInt(polygons)` | polygons | Float polygons → integer polygons |
| `HsBa.toDouble(intPolygons)` | polygons | Integer polygons → float polygons |
| `HsBa.fill(polygons[, cfg])` | polygons | Fill. `cfg = { spacing=0.4, mode="zigzag"\|"line"\|"simpleZigzag", angle=45.0, borderCount=0 }`; when `borderCount>0` it generates perimeters + fill |

### A.6 Support & floor

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.fdmSupport(layers, cfg)` | layers | Per-layer FDM support. See `cfg` fields below |
| `HsBa.slaSupport(layers, cfg)` | layers | Per-layer SLA support |
| `HsBa.floor(bottomPolygons, cfg)` | polygons | Generate the SLA bottom floor / raft |

Common `cfg` fields for `fdmSupport` / `slaSupport`: `overhang_angle`(45), `layer_height`(0.2), `support_gap`(0.5), `support_diameter`(2.0), `support_density`(0.3), `support_pattern`(int), `tree_branch_angle`(30), `tree_max_branch_radius`(5), `honeycomb_cell_size`(5). FDM additionally supports `interface_layers`, `interface_density`; SLA additionally supports `tip_diameter`, `raft_thickness`.

`floor` `cfg` fields: `raft_offset`, `border_width`, `fill_spacing`, `fill_angle_deg`, `border_count`, `use_convex_hull`, `concave_hull_points`.

### A.7 Path & G-code

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.spiralize(sections[, cfg])` | path | Turn per-layer closed contours into a single continuous 3D spiral path (array of {x,y,z}). `cfg = { layerHeight=0.4, startZ=0, zHeights={...} }` (`zHeights` takes priority). Intended for continuous-extrusion processes (FDM/WAAM/3DP) |
| `HsBa.toGcode(layers, cfg)` | string | Generate G-code. Each `layers` element is `{ outlines=, fills=, supports=, zHeight= }`; `cfg = { layerHeight, lineWidth, printSpeed, travelSpeed, extrusionMultiplier, firmware="marlin"\|"reprap"\|"klipper", nozzleDiameter, filamentDiameter, nozzleTemp, bedTemp, retractLength, retractSpeed }` |

### A.8 Packaging & export

| Call | Returns | Description |
|------|---------|-------------|
| `HsBa.saveSlsPackage(tbl)` | bool | Package an SLS export. `tbl = { outlines=layers, zHeights={...}, config="json", output="x.zip", script="export.lua", func="export_sls" }` (`output`, `script` required; `func` defaults to `export_sls`) |
| `HsBa.saveWaamPackage(tbl)` | bool[, err] | Package a WAAM robot program. `tbl = { outlines=layers, zHeights={...}, weld={current,voltage,wireFeedSpeed,gasFlowRate,travelSpeed,process}, robotType=0, beadWidth=1.2, config="json", output="x.txt", script=, func="export_waam" }` (`output` required) |
| `HsBa.saveSlaPackage(tbl)` | bool | Package SLA images. `tbl = { outlines=layers, supports=layers, floor=polygons, config="json", output="x.zip", imageWidth, imageHeight, imageExtension=".png" }` (`output` required) |
| `HsBa.renderImage(polygons, width, height, path)` | bool | Render polygons to an image file |

---

## B. Shared libraries (available in every environment)

The following global libraries are callable from **both the custom pipeline and the built-in stage scripts**.

### B.1 `PolygonOperations` (2D boolean & construction)

| Function | Description |
|----------|-------------|
| `booleanOperation(a, b, op)` | Generic boolean op, `op` ∈ union/intersection/difference/xor |
| `union(a, b)` / `intersection(a, b)` / `difference(a, b)` / `xor(a, b)` | Union / intersection / difference / xor |
| `offsetOperation(polys, delta)` | Inward/outward offset (positive `delta` expands) |
| `convexHullOperation(polys)` / `concaveHullOperation(polys[, ...])` | Convex hull / concave hull |
| `area(polygon)` | Compute area |
| `makeRectangle(xmin, ymin, xmax, ymax)` | Create a rectangle |
| `makeCircle(cx, cy, r)` | Create a circle |
| `makeEllipse(...)` | Create an ellipse |
| `makeRegularPolygon(...)` | Create a regular polygon |
| `textToPolygons(...)` | Convert text to polygon outlines |

> `dumpPolygon` / `dumpPolygons` are available only when `HSBA_POLYGON_DUMP` is compiled in.

### B.2 `PolygonFill` (fill algorithms)

`offsetFill` / `lineFill` / `simpleZigzagFill` / `zigzagFill` / `compositeOffsetFill` / `hybridFill` / `offsetOnly` — accept integer polygon sets plus spacing/angle parameters and return fill-path polygons.

### B.3 `PathOptimize` (path ordering optimization)

| Call | Description |
|------|-------------|
| `PathOptimize.new()` | Create an optimizer object; methods: `addRegion(id, paths)`, `addPolygons(id, polys)`, `addRoute(...)`, `optimizeOrder()`, `buildPaths()`, `buildPolygons()` |
| `PathOptimize.optimizeRegions(regions)` | One-shot optimization in fill-result mode; returns the complete fill path |
| `PathOptimize.optimizePolygons(regions)` | One-shot optimization in polygon mode (before fill); returns the reordered polygon set |

### B.4 `Support` (support generators)

| Call | Description |
|------|-------------|
| `Support.new_plane()` / `new_tree()` / `new_honeycomb()` / `new_sla()` | Create a generator of the given type |
| `Support.new_lua(path, func)` / `new_lua_file(...)` | Lua-script-based custom support generator |
| `Support.generate(obj, current_layer, prev_layer, layer_height, cfg)` | Generate support with the given generator, returns polygons |
| `Support.detect_overhang(current, prev, height, angle)` | Detect overhang regions |
| `Support.default_config()` | Return the default support config table |

### B.5 `Cipher` (encoding)

`Cipher.base64_encode(s)` / `base64_decode(s)` / `hex_encode(s)` / `hex_decode(s)`.

### B.6 `Zipper` / `Bit7zZipper` (compression & packaging)

| Call | Description |
|------|-------------|
| `Zipper.new()` | Create a zip package object |
| `z:AddFile(name, path)` | Add an on-disk file |
| `z:AddByteFile(name, data)` | Add in-memory byte content |
| `z:Save(path)` | Write out the archive |
| `Bit7zZipper.new(format, dll_path)` | 7z compression, `format` ∈ `Zip`/`SevenZip`/`XZ`/`BZIP2`/`GZIP`/`TAR` (requires `HSBA_USE_BIT7Z`) |

### B.7 Database adapters (`SQLiteAdapter` / `MySQLAdapter` / `PostgreSQLAdapter`)

All three share the same methods (MySQL/PostgreSQL require their compile macros):

```lua
local db = SQLiteAdapter.new()
db:Connect("path_or_dsn")
db:CreateTable("...")          -- or db:Execute("SQL...")
db:Insert("table", {col = val})
db:Update("table", {col = val}, "where...")
local rows = db:Query("SELECT ...")   -- returns an array of rows
db:Delete("table", "where...")
```

Methods: `Connect`, `Execute`, `Query`, `Insert`, `Update`, `Delete`, `CreateTable`.

### B.8 `ParamStore` (parameter persistence)

Reflection-based pipeline-config storage. `ParamStore.new(...)` creates an instance; instance methods: `EnsureSchema`, `Save`, `Load`, `List`, `Update`, `Delete` (PascalCase; on success they return `(true, table)`, on failure `(false, err)`).

---

## C. Built-in pipeline stage hooks

Built-in pipelines load user scripts at specific stages to override the default algorithm. **Each stage injects different globals and agrees on a different entry function.**

### C.1 Support stage (custom FDM/SLA support)

Provided by the `LuaSupport` environment, injecting:

| Global | Type | Meaning |
|--------|------|---------|
| `current_layer` | polygons | Current-layer polygons |
| `prev_layer` | polygons | Previous-layer polygons (empty on the first layer) |
| `layer_height` | number | Layer height (mm) |
| `config` | table | Support config (fields as in [A.6](#a6-support--floor)) |

- **Agreed entry function**: `generate_support()`, returning the support cross-section polygon table (same format as `current_layer`).
- Available libraries: `PolygonOperations`, `Support`, plus the 2D/3D external function pool injected by C++.
- The result can also be returned via the `support_polys` global.

### C.2 Fill stage

- **Agreed entry function**: `generate_fill(current_layer)` — the current-layer polygons (with perimeter regions already subtracted) are passed **as an argument**; return the fill polygons.
- This stage **injects no globals** (no layer index / config); use in-script defaults.
- The script chunk should only define the function, **not self-invoke at the end**.
- Available libraries: `PolygonOperations`, `PolygonFill`, `PathOptimize`.

### C.3 Floor stage (SLA)

- Available libraries: `PolygonOperations`, `PolygonFill`.
- Used to customize the SLA bottom floor / raft generation logic.

### C.4 Export stage (SLS / SLA / SLM / LOM / 3DP / WAAM)

Export scripts run at the packaging stage, injecting:

| Global | Type | Meaning |
|--------|------|---------|
| `config` | table | `{ path="config.json", configStr="<JSON content>" }` |
| `images` | array | Each element `{ path="layers/N.json", data="<polygon JSON>" }` |
| `output_path` | string | Output file path |

- **Agreed entry function**: `export_sls()` for SLS (default function name, overridable via the `func` field), `export_waam()` for WAAM, and per-process conventions for the rest.
- Return value: table `{ success = true/false, export_path = "..." }`.
- Available libraries: `Zipper`, `Cipher`, `Bit7zZipper` (optional), `SQLiteAdapter`/`MySQLAdapter`/`PostgreSQLAdapter`, `ParamStore`.

> **Common pitfall**: returning a wrong value type from an export script (e.g. an unexpected structure) causes the zip file to be overwritten or written empty. Always return `{ success=..., export_path=... }`, and keep object method-name casing consistent with the C++ bindings.

---

## Pipeline availability matrix

| API | Custom pipeline | Built-in stage scripts |
|-----|:---------------:|:----------------------:|
| `HsBa` operation table | ✅ (full) | ❌ |
| Context globals (`pipeline_config`, etc.) | ✅ | ❌ |
| `PolygonOperations` | ✅ | ✅ (support/fill/floor/export, per stage) |
| `Support` | ✅ | ✅ (support stage) |
| `PolygonFill` | ✅ | ✅ (fill/floor stage) |
| `PathOptimize` | ✅ | ✅ (fill stage) |
| `Cipher` | ✅ | ✅ (export stage) |
| `Zipper` / `Bit7zZipper` | ✅ | ✅ (export stage) |
| `SQLiteAdapter` / `MySQLAdapter` / `PostgreSQLAdapter` | ✅ | ✅ (export stage) |
| `ParamStore` | ✅ | ✅ (export stage) |

> `MySQLAdapter`, `PostgreSQLAdapter`, and `Bit7zZipper` are registered only when `HSBA_USE_MYSQL`, `HSBA_USE_PGSQL`, and `HSBA_USE_BIT7Z` are enabled respectively.

---

## Full example

A minimal runnable custom-pipeline script:

```lua
-- my_pipeline.lua —— a fully Lua-driven pipeline
function run_pipeline()
    local name = (model_name and #model_name > 0) and model_name or "part"
    local path = (model_path and #model_path > 0) and model_path or "models/bunny.stl"
    local out  = (output_path and #output_path > 0) and output_path or "output/part.gcode"

    HsBa.progress(1, "load model")
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
    HsBa.progress(100, "done")

    return string.format("Done: %d layers -> %s", layers, out)
end
```

For per-process stage-script examples, see the `.lua` files under `samples/Custom/scripts/`, `samples/FDM/scripts/`, `samples/SLA/scripts/`, `samples/SLS/scripts/`, `samples/WAAM/scripts/`, and related directories in the repository.
