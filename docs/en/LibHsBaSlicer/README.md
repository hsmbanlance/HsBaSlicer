# LibHsBaSlicer Module

LibHsBaSlicer is the core C++ static library of HsBaSlicer, providing five major slicing interfaces: Preprocess, Slice, Support, Fill, and Path Generation.

## Submodule List

- [Preprocess (Model Preprocessing)](./model_preprocess.md) - Model loading, transformation, and information queries
- [Slice (Mesh Slicing)](./mesh_slice.md) - Z-axis plane slicing for generating layer contours
- [Support (FDM Support Generation)](./fdm_support.md) - FDM support cross-section generation
- [Fill (Polygon Fill)](./polygon_fill.md) - Polygon infill with various patterns
- [Path (Path Generation)](./path_generator.md) - G-code path generation from layer data
- **Extends (Lua Extension Registration)** - External Lua function registration pools and event callbacks

## Architecture

```
LibHsBaSlicer
├── Preprocess/    Model loading & transformation
├── Slice/         Mesh slicing at specified heights
├── Support/       FDM support generation
├── Fill/          Polygon infill patterns
├── Path/          G-code path generation
└── Extends/       External Lua function registration (2D/3D/File/Event callbacks)
```

## Usage

To use LibHsBaSlicer, include the corresponding header files:

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"  // Lua extension registration
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

## Typical Workflow

1. **Preprocess**: Load model via `LoadModel()`, apply transforms
2. **Slice**: Generate layer contours via `Slice()` at each layer height
3. **Support**: Generate support structures via `GenerateAllFdmSupport()`
4. **Fill**: Fill layer polygons via `FillPolygon()` or `FillWithBorder()`
5. **Path**: Generate G-code paths via `GenerateGCodePathV2()` with multi-firmware output

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
