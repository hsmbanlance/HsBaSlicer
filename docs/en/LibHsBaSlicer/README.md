# LibHsBaSlicer Module

LibHsBaSlicer is the core C++ library of HsBaSlicer, providing the full slicing pipeline: Preprocess, Slice, Support, Fill, Path Generation, SLA Floor/Render/Package, SLS Export, File Transfer, and Lua Extensions.

## Submodule List

- [Preprocess (Model Preprocessing)](./model_preprocess.md) - Model loading, transformation, information queries, boolean operations and shelling
- [Slice (Mesh Slicing)](./mesh_slice.md) - Z-axis plane slicing for generating layer contours
- [Support (Support Generation)](./fdm_support.md) - FDM/SLA/Lua support cross-section generation
- [Fill (Polygon Fill)](./polygon_fill.md) - Polygon infill with various patterns
- [Path (Path Generation)](./path_generator.md) - G-code path generation from layer data
- **Floor (SLA Floor/Render/Package)** - SLA raft generation, layer image rendering and zip packaging
- **Path/SLS Export** - SLS Lua-script-driven export (no standard format)
- **Transfer (File Transfer)** - Remote executor file transfer (pooled TCP connections)
- **Extends (Extension Registration)** - External Lua function pools, event callbacks and C++ event sources

## Architecture

```
LibHsBaSlicer
├── Preprocess/    Model loading, transformation, boolean operations & shelling
├── Slice/         Mesh slicing at specified heights
├── Support/       FDM/SLA/Lua support generation
├── Fill/          Polygon infill patterns
├── Path/          G-code path generation + SLS Lua export
├── Floor/         SLA floor/raft generation, layer rendering, zip packaging
├── Transfer/      Remote file transfer (pooled connections)
└── Extends/       External Lua function registration + C++ event sources (Zipper/DB)
```

## Usage

To use LibHsBaSlicer, include the corresponding header files:

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"            // SLS Lua export
#include "LibHsBaSlicer/Floor/sla_floor.hpp"            // SLA floor/render/package
#include "LibHsBaSlicer/Transfer/file_transfer.hpp"     // File transfer
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"     // Lua extension registration
#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp" // C++ event sources
#include "LibHsBaSlicer/version_info.hpp"               // Version info
```

Link against `LibHsBaSlicer` and its dependencies.

For detailed CMake integration steps, see the [C++ Usage Guide (CMake Integration)](../cpp_cmake_usage.md).

## Lua Extension Function Registration

Use `Extends/LuaAddFunction.hpp` to register external Lua functions that are automatically injected when pipeline stages create their Lua environments:

```cpp
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
using namespace HsBa::Slicer;

// Register custom 2D functions (available in Support, Fill, SLA Output stages)
Add2DFunctions([](lua_State* L) {
    lua_register(L, "my_2d_func", my_2d_func_impl);
});

// Register custom 3D functions (available in Slice, Support stages)
Add3DFunctions([](lua_State* L) {
    lua_register(L, "my_3d_func", my_3d_func_impl);
});

// Register custom File functions (available in SLS Output, SLA Output stages)
AddFileFunctions([](lua_State* L) {
    lua_register(L, "my_file_func", my_file_func_impl);
});

// Register event callbacks (e.g. Zipper events)
AddEventCallback("zipper.on_add", [](lua_State* L) {
    lua_register(L, "on_zip_add", on_zip_add_impl);
});
```

### Function Types Available Per Stage

| Pipeline Stage | Available Function Types |
| --- | --- |
| Slice | 3D |
| Support | 2D + 3D |
| Fill | 2D |
| SLS Output | File |
| SLA Output | 2D + File |

## C++ Event Source Registration

Use `Extends/EventSourceFunction.hpp` to register native C++ event callbacks (non-Lua) for Zipper progress and database event notifications:

```cpp
#include "LibHsBaSlicer/Extends/EventSourceFunction.hpp"
using namespace HsBa::Slicer;

// Register Zipper event callback (progress percentage + stage description)
AddZipperEventCallback([](double percent, std::string_view stage) {
    // handle compression progress
});

// Register database event callback (key + value)
AddDBEventCallback([](std::string_view key, std::string_view value) {
    // handle database events
});
```

## File Transfer

Use `Transfer/file_transfer.hpp` for remote file transfer with pooled TCP connections:

```cpp
#include "LibHsBaSlicer/Transfer/file_transfer.hpp"
using namespace HsBa::Slicer;

FileTransferConfig config;
config.host = "192.168.1.100";
config.port = "9000";
config.pool_size = 4;
config.files = {"part_a.stl", "part_b.stl"};

FileTransferResult result = TransferFiles(config, [](int percent, std::string_view stage) {
    // progress callback
});

if (result.success) {
    // result.files_transferred == result.total_files
}
```

## SLA Floor / Render / Package

Use `Floor/sla_floor.hpp` for SLA raft generation, layer image rendering, and zip package export:

```cpp
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
using namespace HsBa::Slicer;

// Configure floor parameters
SlaFloorConfig floor_cfg;
floor_cfg.raft_offset = 2.0;
floor_cfg.border_width = 1.0;
floor_cfg.fill_spacing = 0.5;
floor_cfg.use_convex_hull = false;

// Generate complete floor (border + fill)
Polygons floor = GenerateFloorRaft(bottom_layer, floor_cfg);

// Render polygons to layer image
RenderPolygonsToImage(layer_polys, 1920, 1080, "output/layer_0.png");

// Package as zip
SlaPackage pkg;
pkg.layer_outlines = layers;
pkg.image_width = 1920;
pkg.image_height = 1080;
SaveSlaPackage(pkg, "output/result.zip");

// Or use Lua custom export
SaveSlaPackageLua(pkg, "output/result.zip", lua_script, "export_sla");
```

### Main Functions

| Function | Description |
| --- | --- |
| `GenerateFloorContact()` | Compute build-plate contact area |
| `GenerateFloorRaft()` | Generate complete raft (border + fill) |
| `GenerateFloorBorder()` | Generate border ring only |
| `GenerateFloorFill()` | Generate internal fill only |
| `LuaCustomFloorByFile()` | Custom floor via Lua script file |
| `LuaCustomFloorByString()` | Custom floor via inline Lua script |
| `RenderPolygonsToImage()` | Render polygons to image (PNG/JPG/SVG) |
| `SaveSlaPackage()` | Package as zip archive |
| `SaveSlaPackageLua()` | Package with Lua custom export logic |

## SLS Lua Export

Use `Path/sls_export.hpp` for SLS export. SLS has no standard output format; output is entirely determined by the Lua script:

```cpp
#include "LibHsBaSlicer/Path/sls_export.hpp"
using namespace HsBa::Slicer;

SlsPackage pkg;
pkg.layer_outlines = layers;      // Per-layer outlines
pkg.layer_z_heights = z_heights;  // Per-layer Z heights

// Lua script receives config/images/output_path globals
SaveSlsPackageLua(pkg, "output/result.zip", "scripts/export_sls.lua", "export_sls");
```

## Version Information

```cpp
#include "LibHsBaSlicer/version_info.hpp"
using namespace HsBa::Slicer;

std::string json = GetVersionJson();  // JSON format version info
std::string xml = GetVersionXml();    // XML format version info
```

## Typical Workflow

1. **Preprocess**: Load model via `LoadModel()`, apply transforms, optionally perform boolean operations/shelling
2. **Slice**: Generate layer contours via `Slice()` at each layer height
3. **Support**: Generate support structures via `GenerateAllFdmSupport()` / `GenerateAllSlaSupport()`
4. **Fill**: Fill layer polygons via `FillPolygon()` or `FillWithBorder()`
5. **Path**: Generate G-code paths via `GenerateGCodePathV2()` with multi-firmware output
6. **SLA Export**: Use `GenerateFloorRaft()` + `RenderPolygonsToImage()` + `SaveSlaPackage()` for SLA output
7. **SLS Export**: Use `SaveSlsPackageLua()` for Lua-script-determined output format

## Advanced Model Preprocessing (CGAL/OCCT)

When compiled with the `USE_CGAL` macro enabled, the preprocessing module provides the following advanced geometric operations:

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
using namespace HsBa::Slicer;

// Load models
LoadModel("body", "models/body.step");    // BRep via OCCT
LoadModel("cavity", "models/cavity.stl"); // Mesh via IGL

// Insert externally constructed model into pool
auto custom_model = std::make_shared<FullTopoModel>(/*...*/);
InsertModel("custom", custom_model);

// ThickSolid (shell) - requires OCCT BRep source
ThickSolidModel("body", "shelled", 2.0f);

// ThickSolid with face exclusion (open faces specified as vertex loops)
std::vector<std::vector<Eigen::Vector3f>> open_faces = {{{0,0,0},{1,0,0},{1,1,0}}};
ThickSolidModel("body", "shelled_open", open_faces, 2.0f);

// Boolean operations - OCCT for BRep-BRep, IGL/CGAL fallback for mesh
BooleanUnion("body", "cavity", "merged");
BooleanDifference("body", "cavity", "cut");
BooleanIntersection("body", "cavity", "common");
BooleanXor("body", "cavity", "xor_result");

// Pool management
ContainsModel("merged");   // true
ModelCount();              // number of models
GetModelNames();           // all names
CleanupModels();           // remove unreferenced models
```

### Kernel Routing Strategy

| Operation | BRep Model (OCCT) | Mesh Model (IGL/CGAL) |
| --- | --- | --- |
| Boolean operations | OCCT preferred | IGL/CGAL fallback |
| ThickSolid (shell) | Supported | Not supported (throws exception) |
| Loading | STEP/IGES/VRML/BREP | STL/OBJ/PLY/OFF |

## GCode Multi-Firmware Output (V2)

`GenerateGCodePathV2()` returns a `GCodePath` object (inheriting from `LayersPath`) that generates standard 3D printer GCode for the target firmware:

```cpp
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "paths/gcodepath.hpp"
using namespace HsBa::Slicer;

// Configure printer parameters
GCodePrinterConfig printer_cfg;
printer_cfg.nozzle_temp = 210.0f;
printer_cfg.bed_temp = 60.0f;
printer_cfg.filament_diameter = 1.75f;
printer_cfg.retract_length = 1.2f;

// Generate GCode path
auto gcode_path = GenerateGCodePathV2(layer_data, path_cfg, printer_cfg);

// Output by firmware format
std::string marlin_gcode = gcode_path->ToGCode(GCodeFirmware::Marlin);
std::string klipper_gcode = gcode_path->ToGCode(GCodeFirmware::Klipper);

// Save to file
gcode_path->SaveGCode("output/model.gcode", GCodeFirmware::Marlin);
```

### Supported Firmware Formats

| Firmware | Enum Value | Features |
| --- | --- | --- |
| Marlin | `GCodeFirmware::Marlin` | M104/M109 temp wait, G92 E0 reset, M82/M83 extrusion mode |
| RepRap/RRF | `GCodeFirmware::RepRap` | Additional M106 fan control, M82/M83 explicit switch |
| Klipper | `GCodeFirmware::Klipper` | SET_PRESSURE_ADVANCE, M220/M221 speed/flow percentage, SET_FAN_SPEED |

### GCodePrinterConfig Fields

| Field | Default | Description |
| --- | --- | --- |
| `nozzle_diameter` | 0.4 | Nozzle diameter (mm) |
| `filament_diameter` | 1.75 | Filament diameter (mm) |
| `nozzle_temp` | 200.0 | Nozzle temperature (°C) |
| `bed_temp` | 60.0 | Bed temperature (°C) |
| `retract_length` | 1.0 | Retraction length (mm) |
| `retract_speed` | 40.0 | Retraction speed (mm/s) |
| `first_layer_speed` | 20.0 | First layer speed (mm/s) |
| `relative_extrusion` | false | Relative extrusion mode (M83) |
| `enable_retraction` | true | Enable retraction |

> The original `GenerateGCodePath()` remains available (returns `PointsPath`) for backward compatibility.

## Namespace

All APIs are in the `HsBa::Slicer` namespace.

## Samples

- `samples/LibHsBaSlicer/main_header.cpp` — Full FDM slicing workflow using traditional #include
- `samples/LibHsBaSlicer/main_module.cpp` — C++20 module version (`import hsba.slicer;`, class-based API)

## C++20 Module Version (ModuleHsBaSlicer)

If your compiler supports C++20 modules, use the `ModuleHsBaSlicer` wrapper for a class-based API:

```cpp
#include <iostream>          // MSVC: #include must precede import
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"  // GMF types not exported
#include "pipelinetypes/pipeline_types.h"

import hsba.slicer;
using namespace HsBa::Slicer;

Model model("bunny", "model.stl");   // RAII
HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
cfg.output_path = "out.gcode";
FdmPipeline pipeline(cfg);
FdmResult result = pipeline.run(model);  // Full pipeline
```

**Important**:
- On MSVC, `#include` must appear before `import` (otherwise C2572 redefinition errors)
- Project types in the GMF (`ModelInfo`, etc.) are NOT exported by the module; consumers must `#include` them directly
- Config structs are shared from `pipelinetypes/pipeline_types.h`—do NOT redefine them

See [C++ Usage Guide](../cpp_cmake_usage.md#using-modulehsbaslicer-c20-module) for details.
