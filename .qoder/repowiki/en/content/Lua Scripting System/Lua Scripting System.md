# Lua Scripting System

<cite>
**Referenced Files in This Document**   
- [lua_pipeline.hpp](file://LibHsBaSlicer/Extends/lua_pipeline.hpp)
- [lua_pipeline.cpp](file://LibHsBaSlicer/Extends/lua_pipeline.cpp)
- [LuaSupport.hpp](file://support/LuaSupport.hpp)
- [LuaSupport.cpp](file://support/LuaSupport.cpp)
- [ISupport.hpp](file://support/ISupport.hpp)
- [ISupport.cpp](file://support/ISupport.cpp)
- [parallel.hpp](file://base/parallel.hpp)
- [pipeline_parallel.hpp](file://DllHsBaSlicer/pipeline_parallel.hpp)
- [path_generator.cpp](file://LibHsBaSlicer/Path/path_generator.cpp)
- [gcodepath.cpp](file://paths/gcodepath.cpp)
- [my_fdm_pipeline.lua](file://samples/Custom/scripts/my_fdm_pipeline.lua)
- [my_infill.lua](file://samples/FDM/scripts/my_infill.lua)
- [my_support.lua](file://samples/FDM/scripts/my_support.lua)
- [lua_pipeline_api.md](file://docs/en/lua_pipeline_api.md)
</cite>

## Update Summary
**Changes Made**   
- Removed extensive implementation details and detailed guides as per the update reason
- Focused on configuration parameters and protocol definitions
- Simplified architectural descriptions while preserving essential API information
- Updated parallel processing documentation to reflect current implementation
- Streamlined examples to focus on configuration rather than implementation details

## Table of Contents
1. [Introduction](#introduction)
2. [Configuration Overview](#configuration-overview)
3. [Core APIs](#core-apis)
4. [Pipeline Configuration](#pipeline-configuration)
5. [Stage Hooks](#stage-hooks)
6. [Parallel Processing](#parallel-processing)
7. [Examples](#examples)
8. [Troubleshooting](#troubleshooting)

## Introduction
HsBaSlicer provides a Lua scripting system for customizing slicing workflows. The system supports two main approaches:
- **Custom pipelines**: Full control over the entire workflow through Lua scripts
- **Stage hooks**: Replace specific built-in stages (support, fill, floor, export) while keeping C++ pipeline order fixed

The system emphasizes configuration-driven customization over detailed implementation guidance, focusing on what users need to configure rather than how the internals work.

## Configuration Overview
The Lua scripting system exposes configuration through several key mechanisms:

### Environment Variables
- `HSBA_PIPELINE_THREADS`: Controls parallel processing thread count (default: hardware_concurrency)
- `HSBA_USE_BIT7Z`: Enables 7z compression support
- `HSBA_USE_MYSQL`: Enables MySQL database adapter
- `HSBA_USE_PGSQL`: Enables PostgreSQL database adapter

### Script Entry Points
Scripts define entry functions that C++ calls:
- Custom pipeline: `run_pipeline()` (default, configurable via `pipeline_entry`)
- Support stage: `generate_support()`
- Fill stage: `generate_fill(current_layer)`
- Export stages: `export_sls()`, `export_waam()`, etc.

## Core APIs

### HsBa Operation Table (Custom Pipeline Only)
The global `HsBa` table provides access to all pipeline operations:

| Category | Functions | Purpose |
|----------|-----------|---------|
| Model Management | `loadModel`, `modelInfo`, `translateModel`, `rotateModel`, `scaleModel`, `removeModel`, `modelNames` | Load and manipulate 3D models |
| Slicing | `layerCount`, `layerZ`, `slice`, `sliceUnsafe` | Convert 3D models to 2D layers |
| Geometry | `toInt`, `toDouble`, `fill` | Coordinate conversion and polygon filling |
| Support | `fdmSupport`, `slaSupport` | Generate support structures |
| Floor | `floor` | Generate SLA bottom floor/raft |
| Path Generation | `spiralize`, `toGcode` | Create toolpaths and G-code |
| File Operations | `readFile`, `writeFile` | Basic file I/O |
| Progress | `progress`, `setLayers`, `setOutputPath` | Report progress and metadata |

### Shared Libraries
Available across all environments:
- `PolygonOperations`: Boolean operations, offsets, hulls, area calculations
- `PolygonFill`: Various fill algorithms (line, zigzag, composite)
- `Support`: Support generator factories and utilities
- `PathOptimize`: Path ordering optimization
- `Cipher`: Base64/hex encoding
- `Zipper`/`Bit7zZipper`: Compression and packaging
- Database adapters: SQLite, MySQL, PostgreSQL
- `ParamStore`: Parameter persistence

## Pipeline Configuration

### Custom Pipeline Context
```lua
-- Global variables available in custom pipeline scripts
model_name      -- Input model name
model_path      -- Input model file path  
output_path     -- Default output path
pipeline_config -- JSON configuration string
machine         -- Machine parameters (optional, injected by C++)
```

### Support Configuration
```lua
-- Common support config fields
{
    overhang_angle = 45.0,           -- Angle threshold for overhang detection
    layer_height = 0.2,              -- Layer height in mm
    support_gap = 0.5,               -- Gap between support and model
    support_diameter = 2.0,          -- Support feature size
    support_density = 0.3,            -- Support density
    support_pattern = 0,             -- Pattern type (0=plane, 1=tree, 2=honeycomb)
    tree_branch_angle = 30.0,        -- Tree branch angle
    tree_max_branch_radius = 5.0,    -- Max tree branch radius
    honeycomb_cell_size = 5.0        -- Honeycomb cell size
}
```

### SLA Floor Configuration
```lua
-- SLA floor/raft configuration
{
    raft_offset = 0.0,               -- Raft offset from model
    border_width = 2.0,              -- Border width
    fill_spacing = 1.0,              -- Fill spacing
    fill_angle_deg = 45.0,           -- Fill angle in degrees
    border_count = 1,                -- Number of borders
    use_convex_hull = false,         -- Use convex hull for raft
    concave_hull_points = 32         -- Concave hull resolution
}
```

### G-code Generation Configuration
```lua
-- G-code generation parameters
{
    layerHeight = 0.2,               -- Layer height
    lineWidth = 0.4,                 -- Line width
    printSpeed = 60,                 -- Print speed (mm/s)
    travelSpeed = 150,               -- Travel speed (mm/s)
    extrusionMultiplier = 1.0,       -- Extrusion multiplier
    firmware = "marlin",             -- Firmware type (marlin/reprap/klipper)
    nozzleDiameter = 0.4,            -- Nozzle diameter
    filamentDiameter = 1.75,         -- Filament diameter
    nozzleTemp = 210,                -- Nozzle temperature
    bedTemp = 60,                    -- Bed temperature
    retractLength = 4.0,             -- Retract length
    retractSpeed = 40                -- Retract speed
}
```

## Stage Hooks

### Support Stage Hook
Replace the default support generation algorithm:
- **Globals**: `current_layer`, `prev_layer`, `layer_height`, `config`
- **Return**: Support polygons (same format as input layers)
- **Libraries**: `PolygonOperations`, `Support` module

### Fill Stage Hook
Replace the default fill algorithm:
- **Parameter**: `current_layer` (polygons with perimeter already subtracted)
- **Return**: Fill polygons
- **Libraries**: `PolygonOperations`, `PolygonFill`, `PathOptimize`
- **Note**: No globals injected; use script defaults

### SLA Floor Hook
Customize SLA bottom floor generation:
- **Libraries**: `PolygonOperations`, `PolygonFill`
- **Purpose**: Customize raft/floor generation logic

### Export Stage Hook
Customize export process:
- **Globals**: `config`, `images`, `output_path`
- **Return**: `{ success = true/false, export_path = "..." }`
- **Libraries**: `Zipper`, `Cipher`, `Bit7zZipper`, database adapters, `ParamStore`

## Parallel Processing

### Thread Pool Configuration
The system uses a process-wide thread pool for parallel processing:
- **Environment Variable**: `HSBA_PIPELINE_THREADS` controls worker count
- **Default**: Hardware concurrency (minimum 2)
- **Serial Mode**: Set to 1 for deterministic behavior

### Parallelized Stages
Current parallelization covers:
- **Support generation**: Per-layer independent support calculation
- **Path generation**: Per-layer path combination and optimization  
- **G-code serialization**: Per-layer G-code string generation

### Performance Considerations
- Use `HSBA_PIPELINE_THREADS=1` for regression testing and debugging
- Avoid excessive intermediate polygon creation in scripts
- Batch operations where possible to reduce overhead
- Monitor memory usage with large models

## Examples

### Minimal Custom Pipeline
```lua
function run_pipeline()
    local name = (model_name or "part")
    local path = (model_path or "models/bunny.stl")
    local out = (output_path or "output/part.gcode")
    
    HsBa.loadModel(name, path)
    local info = HsBa.modelInfo(name)
    local layers = HsBa.layerCount(name, 0.2, 0.25)
    HsBa.setLayers(layers)
    
    -- Generate layers and fills
    local outlines, fills = {}, {}
    for i = 0, layers - 1 do
        local z = HsBa.layerZ(i, 0.2, 0.25)
        outlines[i+1] = HsBa.slice(name, info.bbox_min.z + z)
        fills[i+1] = HsBa.fill(outlines[i], {spacing=0.4, angle=45})
    end
    
    -- Generate G-code
    local gcode = HsBa.toGcode({
        outlines = outlines,
        fills = fills,
        supports = {},
        zHeight = {}
    }, {layerHeight=0.2, lineWidth=0.45})
    
    HsBa.writeFile(out, gcode)
    return "Done: " .. layers .. " layers"
end
```

### Custom Support Generator
```lua
function generate_support()
    if #prev_layer == 0 then
        return {}
    end
    
    -- Detect overhang regions
    local overhang_regions = Support.detect_overhang(
        current_layer, prev_layer, layer_height, 
        config.overhang_angle_threshold or 45.0)
    
    if #overhang_regions == 0 then
        return {}
    end
    
    -- Generate supports using configured pattern
    local generator = Support.new_plane()
    local result = Support.generate(generator, current_layer, prev_layer, 
                                   layer_height, Support.default_config())
    
    return result
end
```

## Troubleshooting

### Common Issues
- **Lua state creation fails**: Check memory availability and initialization
- **Script file not found**: Verify file paths and permissions
- **Missing model**: Ensure `loadModel` succeeded before using model
- **Invalid layer height**: Validate inputs to `layerCount` and `layerZ`
- **Export script returns wrong type**: Always return `{success=..., export_path=...}`

### Debugging Tips
- Use `HsBa.progress(percent, stage)` to track execution flow
- Enable logging through C++ logger integration
- Test with `HSBA_PIPELINE_THREADS=1` for deterministic behavior
- Validate polygon data using `PolygonOperations.area()`

### Performance Optimization
- Minimize polygon operations in tight loops
- Use integer coordinates where possible (`HsBa.toInt`)
- Reuse computed values across iterations
- Consider disabling unnecessary features (supports, complex fills)