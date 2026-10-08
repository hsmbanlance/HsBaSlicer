# Custom Algorithm Development Guide

<cite>
**Referenced Files in This Document**
- [README.md](file://README.md)
- [lua_pipeline_api.md](file://docs/en/lua_pipeline_api.md)
- [my_fdm_pipeline.lua](file://samples/Custom/scripts/my_fdm_pipeline.lua)
- [my_infill.lua](file://samples/FDM/scripts/my_infill.lua)
- [my_support.lua](file://samples/FDM/scripts/my_support.lua)
- [my_sla_floor.lua](file://samples/SLA/scripts/my_sla_floor.lua)
- [my_sla_support.lua](file://samples/SLA/scripts/my_sla_support.lua)
- [my_waam_path.lua](file://samples/WAAM/scripts/my_waam_path.lua)
- [polygon_fill.hpp](file://LibHsBaSlicer/Fill/polygon_fill.hpp)
- [fdm_support.hpp](file://LibHsBaSlicer/Support/fdm_support.hpp)
- [LuaSupport.hpp](file://support/LuaSupport.hpp)
- [LuaAdapter.hpp](file://2D/LuaAdapter.hpp)
- [pipeline_types.h](file://pipelinetypes/pipeline_types.h)
- [hsba_slicer.cppm](file://ModuleHsBaSlicer/hsba_slicer.cppm)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Conclusion](#conclusion)
10. [Appendices](#appendices)

## Introduction
This guide explains how to write custom algorithms in Lua for HsBaSlicer, including bespoke slicing algorithms, support structure generators, fill patterns, and export handlers. It covers the two runtime environments (custom pipeline vs built-in stage hooks), interface contracts, parameter handling, callback mechanisms, and step-by-step recipes for FDM infill, SLA supports, and specialized path generation. It also documents best practices for robustness, progress reporting, error handling, and performance optimization.

## Project Structure
HsBaSlicer exposes a modular C++ core with Lua extension points:
- Custom pipelines are fully scripted via an operation table (`HsBa`) and context globals.
- Built-in pipelines allow replacing specific stages (support, fill, floor, export) with user scripts.
- Shared libraries provide geometry operations, fill algorithms, support generators, path optimization, compression, encoding, and database access inside Lua.

```mermaid
graph TB
subgraph "User Scripts"
A["Custom Pipeline<br/>run_pipeline()"]
B["Built-in Stage Hooks<br/>generate_support / generate_fill / export_*"]
end
subgraph "Runtime"
C["Lua Runtime"]
D["Shared Libraries<br/>PolygonOperations / PolygonFill / Support / PathOptimize"]
E["C++ Core<br/>Slice / Fill / Support / Path"]
end
A --> C
B --> C
C --> D
C --> E
```

[No sources needed since this diagram shows conceptual workflow, not actual code structure]

**Section sources**
- [README.md:21-38](file://README.md#L21-L38)
- [lua_pipeline_api.md:19-28](file://docs/en/lua_pipeline_api.md#L19-L28)

## Core Components
- Custom-pipeline environment: The script defines `run_pipeline()` and uses the global `HsBa` operation table plus context globals such as `model_name`, `model_path`, `output_path`, and `pipeline_config`.
- Built-in stage hooks: Replace only one algorithm per stage; each stage injects its own globals and expects a specific entry function.
- Shared libraries: Available in both environments, including polygon operations, fill algorithms, support generators, path optimization, compression, encoding, and database adapters.

Key responsibilities:
- Geometry data types: Points, polygons, layers are represented as Lua tables with fixed field names.
- Control and I/O: Progress reporting, file read/write, model lifecycle management.
- Slicing and path generation: Slice meshes, compute fills, generate G-code or robot programs.

**Section sources**
- [lua_pipeline_api.md:32-45](file://docs/en/lua_pipeline_api.md#L32-L45)
- [lua_pipeline_api.md:48-133](file://docs/en/lua_pipeline_api.md#L48-L133)
- [lua_pipeline_api.md:136-212](file://docs/en/lua_pipeline_api.md#L136-L212)

## Architecture Overview
The system supports two modes:
- Fully scripted custom pipeline: The script orchestrates loading, slicing, support, fill, path generation, and output.
- Built-in pipeline with Lua overrides: The C++ pipeline controls stage order; user scripts replace specific stages.

```mermaid
sequenceDiagram
participant App as "Caller"
participant Lua as "Lua Runtime"
participant Ops as "HsBa Operations"
participant Core as "C++ Core"
App->>Lua : Execute run_pipeline()
Lua->>Ops : loadModel(name, path)
Ops->>Core : Load model into pool
Lua->>Ops : layerCount(...)
Lua->>Ops : slice(name, z)
Ops->>Core : Slice mesh at z
Core-->>Ops : Polygons (float)
Ops-->>Lua : Polygons
Lua->>Ops : fill(polygons, cfg)
Ops->>Core : Fill engine (built-in or Lua custom)
Core-->>Ops : Fill polygons
Ops-->>Lua : Fill polygons
Lua->>Ops : toGcode(layers, cfg)
Ops->>Core : Generate G-code
Core-->>Ops : String
Ops-->>Lua : String
Lua->>Ops : writeFile(path, content)
Lua-->>App : Result string
```

**Diagram sources**
- [my_fdm_pipeline.lua:52-171](file://samples/Custom/scripts/my_fdm_pipeline.lua#L52-L171)
- [lua_pipeline_api.md:48-133](file://docs/en/lua_pipeline_api.md#L48-L133)

## Detailed Component Analysis

### Custom FDM Infill Pattern (Lua)
Purpose: Provide a custom fill pattern by implementing `generate_fill(current_layer)` and integrating it into the pipeline.

Interface contract:
- Entry function: `generate_fill(current_layer)`
- Input: `current_layer` is an array of polygons (already excluding perimeter regions).
- Output: Array of polygons representing the fill paths.
- Environment: No injected globals; use defaults within the script. Shared libraries available include `PolygonOperations` and `PolygonFill`.

Implementation notes:
- Compute bounding box and scan lines at a given angle.
- Create thin rectangles for each line and intersect with the current layer to clip to valid areas.
- Return the resulting polygons.

```mermaid
flowchart TD
Start(["Start generate_fill"]) --> CheckEmpty{"Is current_layer empty?"}
CheckEmpty --> |Yes| ReturnEmpty["Return {}"]
CheckEmpty --> |No| ComputeBB["Compute bounding box"]
ComputeBB --> BuildLines["Build parallel lines at angle"]
BuildLines --> Clip["Intersect lines with current_layer"]
Clip --> ReturnResult["Return clipped polygons"]
```

**Diagram sources**
- [my_infill.lua:20-88](file://samples/FDM/scripts/my_infill.lua#L20-L88)

**Section sources**
- [my_infill.lua:1-89](file://samples/FDM/scripts/my_infill.lua#L1-L89)
- [lua_pipeline_api.md:234-239](file://docs/en/lua_pipeline_api.md#L234-L239)

### Custom FDM Support Generator (Lua)
Purpose: Replace the built-in FDM support algorithm with a Lua script that detects overhangs and generates supports using built-in generators.

Interface contract:
- Entry function: `generate_support()`
- Globals: `current_layer`, `prev_layer`, `layer_height`, `config`.
- Output: Array of polygons per layer (same format as `current_layer`).

Implementation notes:
- Skip first layer if no previous layer exists.
- Use `Support.detect_overhang` to find overhang regions.
- Select generator based on `config.support_pattern` (plane/tree/honeycomb).
- Configure support parameters and call `Support.generate`.
- Optionally refine results (e.g., offset to reduce contact area).

```mermaid
flowchart TD
Start(["Start generate_support"]) --> FirstLayer{"Is prev_layer empty?"}
FirstLayer --> |Yes| ReturnEmpty["Return {}"]
FirstLayer --> |No| DetectOverhang["Detect overhang regions"]
DetectOverhang --> HasRegions{"Any overhang regions?"}
HasRegions --> |No| ReturnEmpty
HasRegions --> |Yes| SelectGen["Select generator by config.support_pattern"]
SelectGen --> ApplyCfg["Apply support_cfg fields"]
ApplyCfg --> Generate["Call Support.generate"]
Generate --> Refine["Optional refinement (offset)"]
Refine --> ReturnResult["Return support polygons"]
```

**Diagram sources**
- [my_support.lua:32-80](file://samples/FDM/scripts/my_support.lua#L32-L80)

**Section sources**
- [my_support.lua:1-83](file://samples/FDM/scripts/my_support.lua#L1-L83)
- [lua_pipeline_api.md:219-232](file://docs/en/lua_pipeline_api.md#L219-L232)

### Custom SLA Floor Generation (Lua)
Purpose: Customize the SLA bottom floor/raft generation logic.

Interface contract:
- Entry function: `generate_floor(bottom_layer, config)`
- Input: `bottom_layer` (polygons), `config` (table with raft_offset, border_width, fill_spacing, fill_angle_deg, border_count, use_convex_hull, concave_hull_points).
- Output: Array of polygons representing the floor.

Implementation notes:
- Optionally compute convex hull of the footprint.
- Offset outward/inward to create raft borders.
- Generate internal fill using `PolygonFill.zigzagFill` or other fill functions.
- Union border and fill to produce final floor.

```mermaid
flowchart TD
Start(["Start generate_floor"]) --> Footprint{"use_convex_hull?"}
Footprint --> |Yes| Hull["Compute convex hull"]
Footprint --> |No| Keep["Use original footprint"]
Hull --> OffsetOuter["Offset outward for raft outer"]
Keep --> OffsetOuter
OffsetOuter --> OffsetInner["Offset inward for raft inner"]
OffsetInner --> Border["Difference(outer, inner)"]
Border --> FillArea["Offset inner slightly to avoid overlap"]
FillArea --> Fill["Zigzag fill with spacing/angle"]
Fill --> Union["Union(border, fill)"]
Union --> ReturnFloor["Return floor polygons"]
```

**Diagram sources**
- [my_sla_floor.lua:35-62](file://samples/SLA/scripts/my_sla_floor.lua#L35-L62)

**Section sources**
- [my_sla_floor.lua:1-64](file://samples/SLA/scripts/my_sla_floor.lua#L1-L64)
- [lua_pipeline_api.md:241-245](file://docs/en/lua_pipeline_api.md#L241-L245)

### Custom SLA Support Generator (Lua)
Purpose: Replace SLA support generation with a Lua script using built-in SLA support generator.

Interface contract:
- Entry function: `generate_support()`
- Globals: `current_layer`, `prev_layer`, `layer_height`, `config`.
- Output: Array of polygons per layer.

Implementation notes:
- Skip first layer if no previous layer.
- Detect overhang regions.
- Use `Support.new_sla()` and configure support parameters.
- Optionally filter small-area supports.

```mermaid
flowchart TD
Start(["Start generate_support"]) --> FirstLayer{"Is prev_layer empty?"}
FirstLayer --> |Yes| ReturnEmpty["Return {}"]
FirstLayer --> |No| DetectOverhang["Detect overhang regions"]
DetectOverhang --> HasRegions{"Any overhang regions?"}
HasRegions --> |No| ReturnEmpty
HasRegions --> |Yes| GenSLA["Create SLA generator"]
GenSLA --> ApplyCfg["Apply support_cfg fields"]
ApplyCfg --> Generate["Call Support.generate"]
Generate --> Filter["Filter by minimum area"]
Filter --> ReturnResult["Return filtered support polygons"]
```

**Diagram sources**
- [my_sla_support.lua:34-72](file://samples/SLA/scripts/my_sla_support.lua#L34-L72)

**Section sources**
- [my_sla_support.lua:1-76](file://samples/SLA/scripts/my_sla_support.lua#L1-L76)
- [lua_pipeline_api.md:219-232](file://docs/en/lua_pipeline_api.md#L219-L232)

### Specialized Path Generation (WAAM Export Script)
Purpose: Generate a custom robot program text for WAAM processes.

Interface contract:
- Entry function: `export_waam()`
- Globals: `header`, `startProgramFunc`, `endProgramFunc`, `points` (array of point records with `end`, `middle`, `velocity`, `type`).
- Output: String containing the complete robot program text.

Implementation notes:
- Iterate through points and emit robot instructions based on type.
- Include header/footer if provided.
- Concatenate buffer and return the full program text.

```mermaid
flowchart TD
Start(["Start export_waam"]) --> InitBuf["Initialize buffer"]
InitBuf --> Header{"Has startProgramFunc?"}
Header --> |Yes| EmitHeader["Emit PROGRAM header"]
Header --> |No| LoopPts["Loop points"]
EmitHeader --> LoopPts
LoopPts --> ForEachPt{"For each point"}
ForEachPt --> TypeCheck{"Type == MoveJ?"}
TypeCheck --> |Yes| EmitMoveJ["Emit MOVEJ instruction"]
TypeCheck --> |No| EmitMoveL["Emit MOVEL instruction with weld ON"]
EmitMoveJ --> NextPt["Next point"]
EmitMoveL --> NextPt
NextPt --> EndHeader{"Has endProgramFunc?"}
EndHeader --> |Yes| EmitFooter["Emit ENDPROGRAM footer"]
EndHeader --> |No| Concat["Concatenate buffer"]
EmitFooter --> Concat
Concat --> ReturnText["Return program text"]
```

**Diagram sources**
- [my_waam_path.lua:17-42](file://samples/WAAM/scripts/my_waam_path.lua#L17-L42)

**Section sources**
- [my_waam_path.lua:1-46](file://samples/WAAM/scripts/my_waam_path.lua#L1-L46)
- [lua_pipeline_api.md:246-259](file://docs/en/lua_pipeline_api.md#L246-L259)

### Custom Full Pipeline (FDM)
Purpose: Orchestrate the entire slicing process from model loading to G-code writing.

Interface contract:
- Entry function: `run_pipeline()`
- Context globals: `model_name`, `model_path`, `output_path`, `pipeline_config`, optional `machine`.
- Operation table: `HsBa` provides model management, slicing, filling, support, path generation, and file I/O.

Implementation notes:
- Determine machine parameters from injected `machine` table or defaults.
- Load model, compute layer count, iterate layers to slice and fill.
- Optionally generate supports.
- Assemble path layers and generate G-code.
- Write output file and report progress.

```mermaid
sequenceDiagram
participant Script as "my_fdm_pipeline.lua"
participant HsBa as "HsBa Operations"
participant Core as "C++ Core"
Script->>Script : Resolve params (machine/default)
Script->>HsBa : progress(1, "load model")
Script->>HsBa : loadModel(name, path)
Script->>HsBa : modelInfo(name)
Script->>HsBa : layerCount(name, lh, flh)
loop Layers
Script->>HsBa : layerZ(i, lh, flh)
Script->>HsBa : slice(name, z)
Script->>HsBa : fill(polys, cfg)
end
Script->>HsBa : fdmSupport(outlines, cfg)
Script->>HsBa : toGcode(path_layers, cfg)
Script->>HsBa : writeFile(output, gcode)
Script->>HsBa : setOutputPath(output)
Script->>HsBa : removeModel(name)
Script->>HsBa : progress(100, "done")
```

**Diagram sources**
- [my_fdm_pipeline.lua:52-171](file://samples/Custom/scripts/my_fdm_pipeline.lua#L52-L171)

**Section sources**
- [my_fdm_pipeline.lua:1-172](file://samples/Custom/scripts/my_fdm_pipeline.lua#L1-L172)
- [lua_pipeline_api.md:48-133](file://docs/en/lua_pipeline_api.md#L48-L133)

## Dependency Analysis
The Lua extension points depend on shared libraries and C++ core components:
- `PolygonOperations` and `PolygonFill` provide geometry and fill capabilities.
- `Support` provides generator factories and helpers.
- `PathOptimize` enables path ordering improvements.
- File and packaging utilities (`Zipper`, `Bit7zZipper`, `Cipher`) are used in export stages.

```mermaid
graph LR
LuaScripts["Lua Scripts"] --> PolyOps["PolygonOperations"]
LuaScripts --> PolyFill["PolygonFill"]
LuaScripts --> SupportMod["Support"]
LuaScripts --> PathOpt["PathOptimize"]
LuaScripts --> Zip["Zipper / Bit7zZipper"]
LuaScripts --> Cipher["Cipher"]
LuaScripts --> Core["C++ Core APIs"]
```

[No sources needed since this diagram shows conceptual dependencies, not direct code mappings]

**Section sources**
- [lua_pipeline_api.md:136-212](file://docs/en/lua_pipeline_api.md#L136-L212)

## Performance Considerations
- Prefer built-in fill algorithms when possible; use Lua custom fills only for specialized patterns.
- Minimize repeated computations (e.g., precompute bounding boxes and angles).
- Batch operations where feasible (e.g., process multiple layers before heavy computation).
- Use integerization internally for boolean/fill operations; convert back to float only when necessary.
- Avoid excessive object creation in tight loops; reuse buffers and tables.
- Report progress periodically to keep UI responsive and enable cancellation.

[No sources needed since this section provides general guidance]

## Troubleshooting Guide
Common issues and resolutions:
- Wrong return type in export scripts: Ensure export functions return `{ success = true/false, export_path = "..." }`.
- Missing globals in built-in stage hooks: Verify the correct entry function and globals for each stage.
- File I/O errors: Handle nil returns from `readFile` and catch errors from `writeFile`.
- Progress not updating: Call `HsBa.progress(percent, stage)` regularly during long-running tasks.
- Model lifecycle: Always call `removeModel(name)` after processing to free resources.

**Section sources**
- [lua_pipeline_api.md:246-260](file://docs/en/lua_pipeline_api.md#L246-L260)
- [my_fdm_pipeline.lua:153-171](file://samples/Custom/scripts/my_fdm_pipeline.lua#L153-L171)

## Conclusion
HsBaSlicer’s Lua extension framework allows flexible customization across slicing stages. By adhering to the documented interfaces, leveraging shared libraries, and following best practices for error handling, progress reporting, and performance, developers can implement robust custom algorithms for FDM infill, SLA supports, and specialized path generation.

[No sources needed since this section summarizes without analyzing specific files]

## Appendices

### API Reference Summary
- Custom pipeline environment: `HsBa` operation table and context globals.
- Built-in stage hooks: Stage-specific globals and entry functions.
- Shared libraries: Geometry, fill, support, path optimization, compression, encoding, database adapters.

**Section sources**
- [lua_pipeline_api.md:19-28](file://docs/en/lua_pipeline_api.md#L19-L28)
- [lua_pipeline_api.md:48-133](file://docs/en/lua_pipeline_api.md#L48-L133)
- [lua_pipeline_api.md:136-212](file://docs/en/lua_pipeline_api.md#L136-L212)

### Code-Level Interfaces and Types
- Polygon fill API wrappers and Lua custom fill integration.
- Support generation API and Lua-based custom support generator.
- Pipeline configuration and result structures for C ABI.
- Module-level integration of fill strategies and path generation.

**Section sources**
- [polygon_fill.hpp:17-53](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L17-L53)
- [fdm_support.hpp:39-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L39-L67)
- [LuaSupport.hpp:21-80](file://support/LuaSupport.hpp#L21-L80)
- [pipeline_types.h:108-140](file://pipelinetypes/pipeline_types.h#L108-L140)
- [hsba_slicer.cppm:667-696](file://ModuleHsBaSlicer/hsba_slicer.cppm#L667-L696)