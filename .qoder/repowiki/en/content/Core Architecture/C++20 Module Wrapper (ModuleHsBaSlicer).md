# C++20 Module Wrapper (ModuleHsBaSlicer)

<cite>
**Referenced Files in This Document**
- [CMakeLists.txt](file://ModuleHsBaSlicer/CMakeLists.txt)
- [hsba_slicer.cppm](file://ModuleHsBaSlicer/hsba_slicer.cppm)
- [module_anchor.cpp](file://ModuleHsBaSlicer/module_anchor.cpp)
- [CMakeLists.txt](file://LibHsBaSlicer/CMakeLists.txt)
- [export.h](file://LibHsBaSlicer/export.h)
- [model_preprocess.hpp](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp)
- [mesh_slice.hpp](file://LibHsBaSlicer/Slice/mesh_slice.hpp)
- [fdm_support.hpp](file://LibHsBaSlicer/Support/fdm_support.hpp)
- [polygon_fill.hpp](file://LibHsBaSlicer/Fill/polygon_fill.hpp)
- [path_generator.hpp](file://LibHsBaSlicer/Path/path_generator.hpp)
- [sla_floor.hpp](file://LibHsBaSlicer/Floor/sla_floor.hpp)
- [sls_export.hpp](file://LibHsBaSlicer/Path/sls_export.hpp)
- [pipeline_types.h](file://pipelinetypes/pipeline_types.h)
</cite>

## Update Summary
**Changes Made**
- Updated Performance Considerations section to document inline function optimizations
- Enhanced Core Components section with inline function details
- Added specific performance impact analysis for frequently-called methods
- Updated architecture diagrams to reflect inline function benefits

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

## Introduction
This document describes the C++20 module wrapper named ModuleHsBaSlicer, which provides a modern class-based API over LibHsBaSlicer's free functions. The module exposes a cohesive set of classes and utilities for FDM, SLA, and SLS workflows, along with Lua-driven customization points. It is designed to be imported via `import hsba.slicer;` and linked as a static library, while internally forwarding calls to LibHsBaSlicer.

Key goals:
- Provide RAII model management and exception-based error handling.
- Offer high-level pipeline classes that encapsulate slicing, support generation, filling, path generation, floor creation, rendering, and packaging.
- Maintain compatibility with existing LibHsBaSlicer APIs and configuration types.
- **Optimize runtime performance through strategic inline function declarations for frequently-called methods.**

## Project Structure
The module resides under ModuleHsBaSlicer and consists of:
- A single-file module interface unit containing both declarations and definitions to avoid MSVC implicit-import issues.
- A small anchor source to ensure the static library archive is produced by the archiver.
- CMake configuration that declares the module FILE_SET and links against LibHsBaSlicer and required dependencies.

```mermaid
graph TB
subgraph "ModuleHsBaSlicer"
M_CMAKE["CMakeLists.txt"]
M_IMPL["hsba_slicer.cppm<br/>(Inline Optimized)"]
M_ANCHOR["module_anchor.cpp"]
end
subgraph "LibHsBaSlicer"
L_CMAKE["CMakeLists.txt"]
L_EXPORT["export.h"]
L_PREPROC["Preprocess/model_preprocess.hpp"]
L_SLICE["Slice/mesh_slice.hpp"]
L_SUPPORT["Support/fdm_support.hpp"]
L_FILL["Fill/polygon_fill.hpp"]
L_PATH["Path/path_generator.hpp"]
L_FLOOR["Floor/sla_floor.hpp"]
L_SLS["Path/sls_export.hpp"]
end
M_CMAKE --> M_IMPL
M_CMAKE --> M_ANCHOR
M_IMPL --> L_PREPROC
M_IMPL --> L_SLICE
M_IMPL --> L_SUPPORT
M_IMPL --> L_FILL
M_IMPL --> L_PATH
M_IMPL --> L_FLOOR
M_IMPL --> L_SLS
M_CMAKE --> L_CMAKE
M_CMAKE --> L_EXPORT
```

**Diagram sources**
- [CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [hsba_slicer.cppm:1-642](file://ModuleHsBaSlicer/hsba_slicer.cppm#L1-L642)
- [module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-L13)
- [CMakeLists.txt:1-78](file://LibHsBaSlicer/CMakeLists.txt#L1-L78)
- [export.h:1-15](file://LibHsBaSlicer/export.h#L1-L15)
- [model_preprocess.hpp:1-88](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L1-L88)
- [mesh_slice.hpp:1-41](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L1-L41)
- [fdm_support.hpp:1-68](file://LibHsBaSlicer/Support/fdm_support.hpp#L1-L68)
- [polygon_fill.hpp:1-54](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L1-L54)
- [path_generator.hpp:1-62](file://LibHsBaSlicer/Path/path_generator.hpp#L1-L62)
- [sla_floor.hpp:1-183](file://LibHsBaSlicer/Floor/sla_floor.hpp#L1-L183)
- [sls_export.hpp:1-52](file://LibHsBaSlicer/Path/sls_export.hpp#L1-L52)

**Section sources**
- [CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [hsba_slicer.cppm:1-642](file://ModuleHsBaSlicer/hsba_slicer.cppm#L1-L642)
- [module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-L13)

## Core Components
The module exports a cohesive API surface under namespace HsBa::Slicer:

- Exception type:
  - SlicerError: Base exception for slicer errors.

- Type aliases and re-exports:
  - Clipper2 polygon types: Point2, Polygon, Polygons, Point2D, PolygonD, PolygonsD.
  - Pipeline config/result enums and structs from pipeline_types.h.
  - Support configuration types from Support namespace.
  - Default config factories: defaultFdmConfig(), defaultSlaConfig(), defaultSlsConfig().

- Model (RAII):
  - Model: Loads a model into an internal pool on construction, manages lifetime, exposes transforms, slicing, and raw access.
  - **Performance Optimization**: Move constructor, move assignment, info(), translate(), rotate(), scale(), slice(), sliceD(), raw(), and name() are declared inline for optimal performance.

- Pipelines:
  - FdmPipeline: Full FDM workflow (slice -> support -> fill -> path), plus stepwise helpers.
    - **Performance Optimization**: sliceAll(), generateSupports(), fill(), and generatePath() are declared inline.
  - SlaPipeline: Full SLA workflow (slice -> support -> floor -> render -> package).
    - **Performance Optimization**: run(), generateFloor(), renderLayer(), and savePackage() are declared inline.
  - SlsPipeline: SLS export driven by Lua scripts.
    - **Performance Optimization**: run() method is declared inline.

- Lua customization:
  - luaCustomFill, luaCustomFloor, luaCustomSupport - all declared inline for performance.

- Utilities:
  - versionJson(), versionXml() - declared inline for performance.
  - toDouble(), toInt() - declared inline for performance.

These components wrap LibHsBaSlicer free functions and provide a consistent, exception-based, object-oriented interface with optimized inline implementations for frequently-called operations.

**Section sources**
- [hsba_slicer.cppm:60-276](file://ModuleHsBaSlicer/hsba_slicer.cppm#L60-L276)
- [pipeline_types.h:1-400](file://pipelinetypes/pipeline_types.h#L1-L400)

## Architecture Overview
At runtime, consumers import the module and call methods on the exported classes. Internally, these methods forward to LibHsBaSlicer functions such as LoadModel, Slice, GenerateAllFdmSupport, FillWithBorder, GenerateGCodePath, GenerateFloorRaft, RenderPolygonsToImage, SaveSlaPackage, and SaveSlsPackageLua.

The inline function optimization ensures that frequently-called methods like slicing operations, accessor functions, and simple transformations are inlined at compile-time, reducing function call overhead and improving overall performance.

```mermaid
sequenceDiagram
    participant App as "Consumer App"
    participant Mod as "ModuleHsBaSlicer<br/>Inline Optimized"
    participant Lib as "LibHsBaSlicer"
    
    App->>Mod: "import hsba.slicer;"
    App->>Mod: "Model m(name, file)"
    Note over Mod: "Inline constructor & destructor"
    Mod->>Lib: "LoadModel(name, file)"
    App->>Mod: "FdmPipeline.run(m)"
    Note over Mod: "Inline run() method"
    Mod->>Lib: "GetModelInfo(name)"
    loop "For each layer"
        Mod->>Lib: "Slice(model, z)"
        Note over Mod: "Inline slice() method"
    end
    Mod->>Lib: "GenerateAllFdmSupport(layers_d, cfg)"
    Mod->>Lib: "FillWithBorder(contour, spacing, walls, mode, angle)"
    Mod->>Lib: "GenerateGCodePath(layer_data, path_cfg)"
    Mod-->>App: "FdmResult { gcode, total_layers }"
```

**Diagram sources**
- [hsba_slicer.cppm:297-465](file://ModuleHsBaSlicer/hsba_slicer.cppm#L297-L465)
- [model_preprocess.hpp:35-83](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L35-L83)
- [mesh_slice.hpp:18-24](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L18-L24)
- [fdm_support.hpp:32-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L32-L63)
- [polygon_fill.hpp:32-49](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L32-L49)
- [path_generator.hpp:45-46](file://LibHsBaSlicer/Path/path_generator.hpp#L45-L46)

## Detailed Component Analysis

### Class Model
Responsibilities:
- RAII ownership of a model name and shared pointer to IModel.
- Construction loads the model into the global pool; destruction removes it.
- Exposes transforms (translate, rotate, scale), info retrieval, slicing at integer or double precision, and direct access to the underlying IModel.

Design notes:
- Move semantics are supported; copy is disabled.
- Errors during load throw SlicerError.
- **Performance Enhancement**: All core methods including move constructor, move assignment, info(), translate(), rotate(), scale(), slice(), sliceD(), raw(), and name() are declared inline to eliminate function call overhead for frequently-accessed operations.

```mermaid
classDiagram
class Model {
+Model(name, file)
+~Model()
+info() ModelInfo [inline]
+translate(t) void [inline]
+rotate(r) void [inline]
+scale(s) void [inline]
+scale(v) void [inline]
+slice(height) Polygons [inline]
+sliceD(height) PolygonsD [inline]
+raw() const IModel& [inline]
+name() const std : : string& [inline]
-name_ : std : : string
-ptr_ : std : : shared_ptr<IModel>
}
```

**Diagram sources**
- [hsba_slicer.cppm:114-152](file://ModuleHsBaSlicer/hsba_slicer.cppm#L114-L152)
- [hsba_slicer.cppm:297-345](file://ModuleHsBaSlicer/hsba_slicer.cppm#L297-L345)
- [model_preprocess.hpp:35-83](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L35-L83)

**Section sources**
- [hsba_slicer.cppm:114-152](file://ModuleHsBaSlicer/hsba_slicer.cppm#L114-L152)
- [hsba_slicer.cppm:297-345](file://ModuleHsBaSlicer/hsba_slicer.cppm#L297-L345)
- [model_preprocess.hpp:35-83](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L35-L83)

### FDM Pipeline
Responsibilities:
- Encapsulates full FDM flow: slice all layers, generate supports, fill contours, assemble LayerPathData, generate G-code paths, and optionally save output.
- Provides stepwise helpers for reuse or inspection.

Key behaviors:
- Uses first_layer_height for layer zero and subsequent layer heights based on cfg_.layer_height.
- Supports Lua-based support and infill customization when configured.
- Converts between integer and double polygons where needed.
- **Performance Enhancement**: Core methods sliceAll(), generateSupports(), fill(), and generatePath() are declared inline to optimize frequently-called operations.

```mermaid
flowchart TD
Start(["FdmPipeline::run"]) --> SliceAll["Slice all layers [inline]"]
SliceAll --> ToDouble["Convert to PolygonsD"]
ToDouble --> Supports{"Enable support?"}
Supports --> |Yes| GenSupport["GenerateAllFdmSupport or Lua [inline]"]
Supports --> |No| SkipSupport["Skip supports"]
GenSupport --> Assemble["Assemble LayerPathData per layer"]
SkipSupport --> Assemble
Assemble --> Fill["Fill contours (built-in or Lua) [inline]"]
Fill --> PathGen["GenerateGCodePath [inline]"]
PathGen --> SaveOpt{"Output path set?"}
SaveOpt --> |Yes| Save["Save G-code"]
SaveOpt --> |No| ReturnRes["Return FdmResult"]
Save --> ReturnRes
```

**Diagram sources**
- [hsba_slicer.cppm:419-465](file://ModuleHsBaSlicer/hsba_slicer.cppm#L419-L465)
- [fdm_support.hpp:32-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L32-L63)
- [polygon_fill.hpp:32-49](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L32-L49)
- [path_generator.hpp:45-46](file://LibHsBaSlicer/Path/path_generator.hpp#L45-L46)

**Section sources**
- [hsba_slicer.cppm:166-186](file://ModuleHsBaSlicer/hsba_slicer.cppm#L166-L186)
- [hsba_slicer.cppm:351-465](file://ModuleHsBaSlicer/hsba_slicer.cppm#L351-L465)
- [fdm_support.hpp:32-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L32-L63)
- [polygon_fill.hpp:32-49](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L32-L49)
- [path_generator.hpp:45-46](file://LibHsBaSlicer/Path/path_generator.hpp#L45-L46)

### SLA Pipeline
Responsibilities:
- Full SLA flow: slice layers, optional support, floor/raft generation, image rendering, and packaging into a zip.
- Supports Lua-based support, floor, and export customization.

Key behaviors:
- Computes number of layers from bounding box height and layer height.
- Generates floor from bottom layer using built-in or Lua logic.
- Renders images and saves package via SaveSlaPackage or SaveSlaPackageLua.
- **Performance Enhancement**: Core methods run(), generateFloor(), renderLayer(), and savePackage() are declared inline for optimal performance.

```mermaid
sequenceDiagram
participant App as "Consumer App"
participant Mod as "SlaPipeline [Inline Optimized]"
participant Lib as "LibHsBaSlicer"
App->>Mod : "run(model, output_zip) [inline]"
Mod->>Lib : "GetModelInfo(name)"
loop Layers
Mod->>Lib : "Slice(model, z)"
end
alt Enable support
Mod->>Lib : "GenerateAllSlaSupport or Lua"
end
Mod->>Lib : "GenerateFloorRaft or Lua [inline]"
Mod->>Lib : "RenderPolygonsToImage [inline]"
Mod->>Lib : "SaveSlaPackage or SaveSlaPackageLua [inline]"
Mod-->>App : "SlaResult { saved, total_layers }"
```

**Diagram sources**
- [hsba_slicer.cppm:507-570](file://ModuleHsBaSlicer/hsba_slicer.cppm#L507-L570)
- [sla_floor.hpp:132-178](file://LibHsBaSlicer/Floor/sla_floor.hpp#L132-L178)
- [fdm_support.hpp:45-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L45-L63)

**Section sources**
- [hsba_slicer.cppm:200-219](file://ModuleHsBaSlicer/hsba_slicer.cppm#L200-L219)
- [hsba_slicer.cppm:471-570](file://ModuleHsBaSlicer/hsba_slicer.cppm#L471-L570)
- [sla_floor.hpp:132-178](file://LibHsBaSlicer/Floor/sla_floor.hpp#L132-L178)

### SLS Pipeline
Responsibilities:
- SLS export is entirely Lua-driven. The pipeline slices layers and passes them to SaveSlsPackageLua with provided script and function name.

Key behaviors:
- Requires export_lua_script to be set; otherwise throws SlicerError.
- Builds SlsPackage with outlines and Z heights.
- **Performance Enhancement**: The run() method is declared inline to optimize the primary entry point.

```mermaid
flowchart TD
Start(["SlsPipeline::run [inline]"]) --> CheckScript{"export_lua_script set?"}
CheckScript --> |No| ThrowErr["Throw SlicerError"]
CheckScript --> |Yes| SliceLayers["Slice layers and build SlsPackage"]
SliceLayers --> ExportLua["SaveSlsPackageLua(pkg, output, script, func)"]
ExportLua --> End(["Return bool"])
```

**Diagram sources**
- [hsba_slicer.cppm:578-601](file://ModuleHsBaSlicer/hsba_slicer.cppm#L578-L601)
- [sls_export.hpp:45-47](file://LibHsBaSlicer/Path/sls_export.hpp#L45-L47)

**Section sources**
- [hsba_slicer.cppm:226-237](file://ModuleHsBaSlicer/hsba_slicer.cppm#L226-L237)
- [hsba_slicer.cppm:576-601](file://ModuleHsBaSlicer/hsba_slicer.cppm#L576-L601)
- [sls_export.hpp:45-47](file://LibHsBaSlicer/Path/sls_export.hpp#L45-L47)

### Lua Customization Functions
Responsibilities:
- Provide convenient wrappers around LibHsBaSlicer Lua integration for fill, floor, and support generation.

Usage:
- luaCustomFill: custom fill pattern via Lua script file.
- luaCustomFloor: custom floor via Lua script file.
- luaCustomSupport: custom support via inline Lua script.
- **Performance Enhancement**: All three functions are declared inline for optimal performance when called frequently.

**Section sources**
- [hsba_slicer.cppm:244-256](file://ModuleHsBaSlicer/hsba_slicer.cppm#L244-L256)
- [hsba_slicer.cppm:607-625](file://ModuleHsBaSlicer/hsba_slicer.cppm#L607-L625)
- [polygon_fill.hpp:47-49](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L47-L49)
- [sla_floor.hpp:94-114](file://LibHsBaSlicer/Floor/sla_floor.hpp#L94-L114)
- [fdm_support.hpp:60-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L60-L63)

## Dependency Analysis
ModuleHsBaSlicer depends on:
- LibHsBaSlicer (static or shared depending on build settings).
- HsBaPipelineTypes for C-compatible config/result types.
- Eigen3 for geometry operations.
- Clipper2 for polygon math.
- Lua libraries for scripting integration.

Build-time considerations:
- Single-file module avoids MSVC implicit-import issues.
- Static library ensures consumers link directly and avoid DLL linkage quirks.
- Compiler flags and definitions are propagated to consumers to maintain ABI consistency across CGAL/Eigen boundaries.
- **Inline optimization strategy**: Frequently-called methods are marked inline to reduce function call overhead while maintaining clean separation between interface and implementation.

```mermaid
graph LR
Consumer["Consumer App"] --> Module["ModuleHsBaSlicer (STATIC)<br/>Inline Optimized"]
Module --> Lib["LibHsBaSlicer"]
Module --> Types["HsBaPipelineTypes"]
Module --> Eigen["Eigen3::Eigen"]
Module --> Clipper["Clipper2"]
Module --> Lua["Lua Libraries"]
```

**Diagram sources**
- [CMakeLists.txt:12-29](file://ModuleHsBaSlicer/CMakeLists.txt#L12-L29)
- [CMakeLists.txt:1-78](file://LibHsBaSlicer/CMakeLists.txt#L1-L78)

**Section sources**
- [CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [CMakeLists.txt:1-78](file://LibHsBaSlicer/CMakeLists.txt#L1-L78)

## Performance Considerations
**Updated** Added comprehensive inline function optimization analysis

### Inline Function Optimization Strategy
The module implements strategic inline function declarations for 27 frequently-called methods across the core classes:

#### Model Class Optimizations
- **Move Operations**: `Model(Model&&)` and `operator=(Model&&)` - eliminated move overhead
- **Accessors**: `info()`, `raw()`, `name()` - direct access without function call overhead
- **Transformations**: `translate()`, `rotate()`, `scale()` - frequent geometric operations
- **Slicing**: `slice()`, `sliceD()` - core slicing operations called per layer

#### Pipeline Optimizations
- **FdmPipeline**: `sliceAll()`, `generateSupports()`, `fill()`, `generatePath()` - core processing methods
- **SlaPipeline**: `run()`, `generateFloor()`, `renderLayer()`, `savePackage()` - main workflow methods  
- **SlsPipeline**: `run()` - primary export method

#### Utility Optimizations
- **Lua Functions**: `luaCustomFill()`, `luaCustomFloor()`, `luaCustomSupport()` - customization entry points
- **Version Info**: `versionJson()`, `versionXml()` - simple accessor functions
- **Type Conversion**: `toDouble()`, `toInt()` - frequently-used conversion utilities

### Performance Impact Analysis
- **Reduced Function Call Overhead**: Inline functions eliminate call/return overhead for frequently-accessed methods
- **Compiler Optimization Opportunities**: Inlined code enables better compiler optimizations like constant propagation and dead code elimination
- **Memory Access Patterns**: Direct access to member variables through inline functions improves cache locality
- **Critical Path Optimization**: Slicing operations, which are called once per layer, benefit significantly from inlining

### Best Practices
- Prefer using sliceD only when downstream algorithms require double precision; otherwise use slice to avoid conversion overhead.
- Reuse FdmPipeline/SlaPipeline instances across models to minimize repeated configuration setup.
- Disable unnecessary steps (e.g., support) when not needed to reduce computation time.
- Use appropriate image formats for SLA outputs: PNG for lossless quality, JPG for smaller files, SVG for vector scalability.
- **Leverage inline optimizations**: The module's inline design means performance-critical paths are already optimized at compile-time.

## Troubleshooting Guide
Common issues and resolutions:
- MSVC C2572 / C5050 errors when importing BMI:
  - Ensure consumer targets propagate the same compile options and definitions as ModuleHsBaSlicer (fp:strict, fp:except-, _SCL_SECURE_NO_WARNINGS).
- Missing .lib for static module:
  - The anchor TU guarantees the archiver runs; verify CMake FILE_SET usage and target_sources configuration.
- SLS export failure due to missing script:
  - Ensure export_lua_script is set before calling SlsPipeline::run.
- Model loading failures:
  - Verify file path and format support; check that RemoveModel is called automatically via RAII.
- **Performance Issues**: If experiencing unexpected performance problems, verify that the module is being compiled with optimization enabled (-O2 or higher) to allow proper inline expansion.

**Section sources**
- [CMakeLists.txt:36-45](file://ModuleHsBaSlicer/CMakeLists.txt#L36-L45)
- [module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-L13)
- [hsba_slicer.cppm:578-582](file://ModuleHsBaSlicer/hsba_slicer.cppm#L578-L582)
- [hsba_slicer.cppm:297-309](file://ModuleHsBaSlicer/hsba_slicer.cppm#L297-L309)

## Conclusion
ModuleHsBaSlicer delivers a modern, exception-safe, and ergonomic C++20 API over LibHsBaSlicer with significant performance optimizations through strategic inline function declarations. By exporting classes like Model, FdmPipeline, SlaPipeline, and SlsPipeline with 27 frequently-called methods optimized as inline functions, it abstracts away low-level free functions while preserving flexibility through Lua customization and maximizing runtime performance. The single-file module design, careful CMake configuration, and inline optimization strategy ensure reliable consumption across platforms and toolchains while providing excellent performance characteristics for production workloads.