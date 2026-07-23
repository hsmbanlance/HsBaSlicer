# LibHsBaSlicer Module

LibHsBaSlicer is the core C++ static library of HsBaSlicer, providing five major slicing interfaces: Preprocess, Slice, Support, Fill, and Path Generation.

## Submodule List

- [Preprocess (Model Preprocessing)](./model_preprocess.md) - Model loading, transformation, and information queries
- [Slice (Mesh Slicing)](./mesh_slice.md) - Z-axis plane slicing for generating layer contours
- [Support (FDM Support Generation)](./fdm_support.md) - FDM support cross-section generation
- [Fill (Polygon Fill)](./polygon_fill.md) - Polygon infill with various patterns
- [Path (Path Generation)](./path_generator.md) - G-code path generation from layer data

## Architecture

```
LibHsBaSlicer
├── Preprocess/    Model loading & transformation
├── Slice/         Mesh slicing at specified heights
├── Support/       FDM support generation
├── Fill/          Polygon infill patterns
└── Path/          G-code path generation
```

## Usage

To use LibHsBaSlicer, include the corresponding header files:

```cpp
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
```

Link against `LibHsBaSlicer` and its dependencies.

For detailed CMake integration steps, see the [C++ Usage Guide (CMake Integration)](../cpp_cmake_usage.md).

## Typical Workflow

1. **Preprocess**: Load model via `LoadModel()`, apply transforms
2. **Slice**: Generate layer contours via `Slice()` at each layer height
3. **Support**: Generate support structures via `GenerateAllFdmSupport()`
4. **Fill**: Fill layer polygons via `FillPolygon()` or `FillWithBorder()`
5. **Path**: Generate G-code paths via `GenerateGCodePath()`

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
