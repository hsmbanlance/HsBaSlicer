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
- [file_transfer.hpp](file://LibHsBaSlicer/Transfer/file_transfer.hpp)
- [EventSourceFunction.hpp](file://LibHsBaSlicer/Extends/EventSourceFunction.hpp)
- [LuaAddFunction.hpp](file://LibHsBaSlicer/Extends/LuaAddFunction.hpp)
- [lua_pipeline.hpp](file://LibHsBaSlicer/Extends/lua_pipeline.hpp)
- [lua_pipeline.cpp](file://LibHsBaSlicer/Extends/lua_pipeline.cpp)
- [custom_pipeline.h](file://DllHsBaSlicer/custom_pipeline.h)
- [custom_pipeline.cpp](file://DllHsBaSlicer/custom_pipeline.cpp)
- [pipeline_types.h](file://pipelinetypes/pipeline_types.h)
</cite>

## Update Summary
**Changes Made**
- Added comprehensive CustomLuaPipeline class with full Lua-driven workflow orchestration
- Enhanced module exports with new Lua pipeline functionality and context management
- Expanded event callback system with Zipper and Database event handlers
- Updated type aliases and configuration structures to support custom pipeline operations
- Added progress reporting mechanisms for custom Lua pipelines
- Integrated asynchronous execution capabilities using C++20 coroutines

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
This document describes the C++20 module wrapper named ModuleHsBaSlicer, which provides a modern class-based API over LibHsBaSlicer's free functions. The module exposes a cohesive set of classes and utilities for FDM, SLA, SLS workflows, along with **new custom Lua pipeline capabilities** and **enhanced file transfer functionality**. It is designed to be imported via `import hsba.slicer;` and linked as a static library, while internally forwarding calls to LibHsBaSlicer.

Key goals:
- Provide RAII model management and exception-based error handling.
- Offer high-level pipeline classes that encapsulate slicing, support generation, filling, path generation, floor creation, rendering, packaging, **file transfer operations**, and **fully customizable Lua-driven workflows**.
- Maintain compatibility with existing LibHsBaSlicer APIs and configuration types.
- **Optimize runtime performance through strategic inline function declarations for frequently-called methods.**
- **Enable comprehensive event-driven programming through robust callback systems.**
- **Provide flexible customization points through Lua scripting for complex multi-stage workflows.**

## Project Structure
The module resides under ModuleHsBaSlicer and consists of:
- A single-file module interface unit containing both declarations and definitions to avoid MSVC implicit-import issues.
- A small anchor source to ensure the static library archive is produced by the archiver.
- CMake configuration that declares the module FILE_SET and links against LibHsBaSlicer and required dependencies.

```mermaid
graph TB
subgraph "ModuleHsBaSlicer"
M_CMAKE["CMakeLists.txt"]
M_IMPL["hsba_slicer.cppm<br/>Inline Optimized"]
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
L_TRANSFER["Transfer/file_transfer.hpp"]
L_EVENT["Extends/EventSourceFunction.hpp"]
L_LUA["Extends/LuaAddFunction.hpp"]
L_LUAPIPE["Extends/lua_pipeline.hpp"]
end
subgraph "DllHsBaSlicer"
D_CUSTOM["custom_pipeline.h/.cpp"]
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
M_IMPL --> L_TRANSFER
M_IMPL --> L_EVENT
M_IMPL --> L_LUA
M_IMPL --> L_LUAPIPE
M_CMAKE --> L_CMAKE
M_CMAKE --> L_EXPORT
D_CUSTOM --> L_LUAPIPE
```

**Diagram sources**
- [CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [hsba_slicer.cppm:1-867](file://ModuleHsBaSlicer/hsba_slicer.cppm#L1-L867)
- [module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-L13)
- [CMakeLists.txt:1-78](file://LibHsBaSlicer/CMakeLists.txt#L1-L78)
- [export.h:1-15](file://LibHsBaSlicer/export.h#L1-L15)
- [lua_pipeline.hpp:1-84](file://LibHsBaSlicer/Extends/lua_pipeline.hpp#L1-L84)
- [custom_pipeline.h:1-66](file://DllHsBaSlicer/custom_pipeline.h#L1-L66)

**Section sources**
- [CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [hsba_slicer.cppm:1-867](file://ModuleHsBaSlicer/hsba_slicer.cppm#L1-L867)
- [module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-L13)

## Core Components
The module exports a cohesive API surface under namespace HsBa::Slicer:

- Exception type:
  - SlicerError: Base exception for slicer errors.

- Type aliases and re-exports:
  - Clipper2 polygon types: Point2, Polygon, Polygons, Point2D, PolygonD, PolygonsD.
  - Pipeline config/result enums and structs from pipeline_types.h.
  - Support configuration types from Support namespace.
  - Default config factories: defaultFdmConfig(), defaultSlaConfig(), defaultSlsConfig(), **defaultFileTransferConfig()**, **defaultCustomConfig()**.
  - **Event callback function types: ZipperEventCallbackFunc, DBEventCallbackFunc**.
  - **File transfer progress callback type: FileTransferProgressFunc**.

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
  - **FileTransferPipeline**: Complete file transfer workflow with validation, connection pooling, and progress reporting.
    - **Performance Optimization**: run() methods are declared inline for optimal performance.
  - **CustomLuaPipeline**: Fully Lua-driven workflow where the entire process is orchestrated by Lua scripts.
    - **Performance Enhancement**: Provides flexible customization through Lua environment with all pipeline building blocks exposed.

- **Event System**:
  - addEventCallback(): Register event callbacks by name (e.g., "zipper.on_add", "db.on_query").
  - addZipperEventCallback(): Register C++ event callbacks for zipper operations.
  - addDBEventCallback(): Register C++ event callbacks for database operations.

- **Lua Customization**:
  - luaCustomFill, luaCustomFloor, luaCustomSupport - all declared inline for performance.
  - add2DFunction(), add3DFunction(), addFileFunction() - register external Lua functions for different pipeline stages.

- Utilities:
  - versionJson(), versionXml() - declared inline for performance.
  - toDouble(), toInt() - declared inline for performance.

These components wrap LibHsBaSlicer free functions and provide a consistent, exception-based, object-oriented interface with optimized inline implementations for frequently-called operations.

**Section sources**
- [hsba_slicer.cppm:60-383](file://ModuleHsBaSlicer/hsba_slicer.cppm#L60-L383)
- [pipeline_types.h:1-491](file://pipelinetypes/pipeline_types.h#L1-L491)

## Architecture Overview
At runtime, consumers import the module and call methods on the exported classes. Internally, these methods forward to LibHsBaSlicer functions such as LoadModel, Slice, GenerateAllFdmSupport, FillWithBorder, GenerateGCodePath, GenerateFloorRaft, RenderPolygonsToImage, SaveSlaPackage, SaveSlsPackageLua, **TransferFiles**, **RunLuaPipeline**, and various event callback functions.

The inline function optimization ensures that frequently-called methods like slicing operations, accessor functions, simple transformations, file transfer operations, and custom pipeline executions are inlined at compile-time, reducing function call overhead and improving overall performance.

```mermaid
sequenceDiagram
participant App as "Consumer App"
participant Mod as "ModuleHsBaSlicer<br/>Inline Optimized"
participant Lib as "LibHsBaSlicer"
participant Lua as "Lua Environment"
App->>Mod : "import hsba.slicer;"
App->>Mod : "Model m(name, file)"
Note over Mod : "Inline constructor & destructor"
Mod->>Lib : "LoadModel(name, file)"
App->>Mod : "CustomLuaPipeline.run()"
Note over Mod : "Inline run() method"
Mod->>Lib : "SetupLuaPipelineEnvironment()"
Lib->>Lua : "Create Lua state with HsBa table"
Lua->>Lua : "Execute user script"
Lua->>Lib : "Call pipeline building blocks"
Lib-->>Mod : "Return results"
Mod-->>App : "CustomLuaResult { success, layers, output }"
```

**Diagram sources**
- [hsba_slicer.cppm:771-804](file://ModuleHsBaSlicer/hsba_slicer.cppm#L771-L804)
- [lua_pipeline.hpp:25-84](file://LibHsBaSlicer/Extends/lua_pipeline.hpp#L25-L84)
- [lua_pipeline.cpp:728-855](file://LibHsBaSlicer/Extends/lua_pipeline.cpp#L728-L855)
- [custom_pipeline.cpp:165-191](file://DllHsBaSlicer/custom_pipeline.cpp#L165-L191)

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
- [hsba_slicer.cppm:130-173](file://ModuleHsBaSlicer/hsba_slicer.cppm#L130-L173)
- [hsba_slicer.cppm:406-454](file://ModuleHsBaSlicer/hsba_slicer.cppm#L406-L454)
- [model_preprocess.hpp:35-83](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L35-L83)

**Section sources**
- [hsba_slicer.cppm:130-173](file://ModuleHsBaSlicer/hsba_slicer.cppm#L130-L173)
- [hsba_slicer.cppm:406-454](file://ModuleHsBaSlicer/hsba_slicer.cppm#L406-L454)
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
- [hsba_slicer.cppm:543-590](file://ModuleHsBaSlicer/hsba_slicer.cppm#L543-L590)
- [fdm_support.hpp:32-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L32-L63)
- [polygon_fill.hpp:32-49](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L32-L49)
- [path_generator.hpp:45-46](file://LibHsBaSlicer/Path/path_generator.hpp#L45-L46)

**Section sources**
- [hsba_slicer.cppm:186-207](file://ModuleHsBaSlicer/hsba_slicer.cppm#L186-L207)
- [hsba_slicer.cppm:460-590](file://ModuleHsBaSlicer/hsba_slicer.cppm#L460-L590)
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
- [hsba_slicer.cppm:632-695](file://ModuleHsBaSlicer/hsba_slicer.cppm#L632-L695)
- [sla_floor.hpp:132-178](file://LibHsBaSlicer/Floor/sla_floor.hpp#L132-L178)
- [fdm_support.hpp:45-63](file://LibHsBaSlicer/Support/fdm_support.hpp#L45-L63)

**Section sources**
- [hsba_slicer.cppm:220-240](file://ModuleHsBaSlicer/hsba_slicer.cppm#L220-L240)
- [hsba_slicer.cppm:596-695](file://ModuleHsBaSlicer/hsba_slicer.cppm#L596-L695)
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
- [hsba_slicer.cppm:703-726](file://ModuleHsBaSlicer/hsba_slicer.cppm#L703-L726)
- [sls_export.hpp:45-47](file://LibHsBaSlicer/Path/sls_export.hpp#L45-L47)

**Section sources**
- [hsba_slicer.cppm:246-258](file://ModuleHsBaSlicer/hsba_slicer.cppm#L246-L258)
- [hsba_slicer.cppm:701-726](file://ModuleHsBaSlicer/hsba_slicer.cppm#L701-L726)
- [sls_export.hpp:45-47](file://LibHsBaSlicer/Path/sls_export.hpp#L45-L47)

### File Transfer Pipeline
Responsibilities:
- Complete file transfer workflow with validation, connection pooling, and progress reporting.
- Supports both synchronous and asynchronous execution modes.
- Provides detailed progress tracking and error handling.

Key behaviors:
- Validates file existence before transfer attempts.
- Establishes connection pools for efficient file transfers.
- Reports progress through callback functions with percentage and stage information.
- Handles both successful and failed transfer scenarios with detailed error messages.
- **Performance Enhancement**: Both run() methods are declared inline for optimal performance.

```mermaid
flowchart TD
Start(["FileTransferPipeline::run [inline]"]) --> Validate["Validate file paths"]
Validate --> Connect["Establish connection pool"]
Connect --> TransferLoop{"Transfer files"}
TransferLoop --> |Each file| Progress["Report progress"]
Progress --> Send["Send file to remote"]
Send --> Next{"More files?"}
Next --> |Yes| TransferLoop
Next --> |No| Result["Build result"]
Result --> Success{"Success?"}
Success --> |Yes| Return["Return FileTransferOutcome"]
Success --> |No| Error["Throw SlicerError"]
```

**Diagram sources**
- [hsba_slicer.cppm:734-765](file://ModuleHsBaSlicer/hsba_slicer.cppm#L734-L765)
- [file_transfer.hpp:45-56](file://LibHsBaSlicer/Transfer/file_transfer.hpp#L45-L56)

**Section sources**
- [hsba_slicer.cppm:272-288](file://ModuleHsBaSlicer/hsba_slicer.cppm#L272-L288)
- [hsba_slicer.cppm:732-765](file://ModuleHsBaSlicer/hsba_slicer.cppm#L732-L765)
- [file_transfer.hpp:19-56](file://LibHsBaSlicer/Transfer/file_transfer.hpp#L19-L56)

### Custom Lua Pipeline
Responsibilities:
- **Fully Lua-driven workflow orchestration** where the entire process is controlled by Lua scripts.
- Provides a complete Lua environment with all pipeline building blocks exposed through the global `HsBa` table.
- Supports both synchronous and asynchronous execution with progress reporting.
- Enables complex multi-stage workflows without recompiling C++ code.

Key features:
- **LuaPipelineContext**: Configuration structure for script execution including inline source, script file, entry function, and context variables.
- **LuaPipelineOutput**: Result structure capturing success status, layer count, output path, and return values from Lua scripts.
- **HsBa Table Operations**: Comprehensive Lua API exposing model management, slicing, support generation, filling, floor creation, path generation, and packaging operations.
- **Progress Reporting**: Built-in progress callback system allowing Lua scripts to report execution status.
- **Flexible Configuration**: Support for JSON configuration, model parameters, and output settings.

```mermaid
flowchart TD
Start(["CustomLuaPipeline::run"]) --> CheckConfig{"Script or source set?"}
CheckConfig --> |No| ThrowErr["Throw SlicerError"]
CheckConfig --> |Yes| SetupEnv["SetupLuaPipelineEnvironment()"]
SetupEnv --> CreateLua["Create Lua state with HsBa table"]
CreateLua --> ExecuteScript["Execute Lua script"]
ExecuteScript --> CallEntry["Call entry function (run_pipeline)"]
CallEntry --> ProcessResults["Process Lua results"]
ProcessResults --> BuildResult["Build CustomLuaResult"]
BuildResult --> Return["Return result with layers, output, string"]
```

**Diagram sources**
- [hsba_slicer.cppm:771-804](file://ModuleHsBaSlicer/hsba_slicer.cppm#L771-L804)
- [lua_pipeline.cpp:728-855](file://LibHsBaSlicer/Extends/lua_pipeline.cpp#L728-L855)

**Section sources**
- [hsba_slicer.cppm:294-322](file://ModuleHsBaSlicer/hsba_slicer.cppm#L294-L322)
- [hsba_slicer.cppm:771-804](file://ModuleHsBaSlicer/hsba_slicer.cppm#L771-L804)
- [lua_pipeline.hpp:25-84](file://LibHsBaSlicer/Extends/lua_pipeline.hpp#L25-L84)
- [lua_pipeline.cpp:728-855](file://LibHsBaSlicer/Extends/lua_pipeline.cpp#L728-L855)

### Event System
Responsibilities:
- Provides comprehensive event-driven programming capabilities for various system operations.
- Supports multiple event types including zipper operations and database queries.
- Enables flexible callback registration and management.

Key features:
- addEventCallback(): Register custom event handlers by name.
- addZipperEventCallback(): Handle zipper-related events with progress and status updates.
- addDBEventCallback(): Manage database operation events with query and result information.
- **Performance Enhancement**: All event registration functions are declared inline for optimal performance.

```mermaid
classDiagram
class EventSystem {
+addEventCallback(event_name, func) void [inline]
+addZipperEventCallback(func) void [inline]
+addDBEventCallback(func) void [inline]
+ZipperEventCallbackFunc
+DBEventCallbackFunc
}
class ZipperEvents {
+on_progress(percent, stage)
+on_add(file_path)
+on_remove(file_path)
}
class DatabaseEvents {
+on_query(query_string)
+on_result(result_data)
+on_error(error_message)
}
EventSystem --> ZipperEvents
EventSystem --> DatabaseEvents
```

**Diagram sources**
- [hsba_slicer.cppm:356-363](file://ModuleHsBaSlicer/hsba_slicer.cppm#L356-L363)
- [EventSourceFunction.hpp:14-26](file://LibHsBaSlicer/Extends/EventSourceFunction.hpp#L14-L26)

**Section sources**
- [hsba_slicer.cppm:356-363](file://ModuleHsBaSlicer/hsba_slicer.cppm#L356-L363)
- [EventSourceFunction.hpp:14-26](file://LibHsBaSlicer/Extends/EventSourceFunction.hpp#L14-L26)

### Lua Customization Functions
Responsibilities:
- Provide convenient wrappers around LibHsBaSlicer Lua integration for fill, floor, and support generation.
- Enable advanced customization through external Lua scripts and functions.

Usage:
- luaCustomFill: custom fill pattern via Lua script file.
- luaCustomFloor: custom floor via Lua script file.
- luaCustomSupport: custom support via inline Lua script.
- add2DFunction(), add3DFunction(), addFileFunction(): Register external Lua functions for different pipeline stages.
- **Performance Enhancement**: All functions are declared inline for optimal performance when called frequently.

**Section sources**
- [hsba_slicer.cppm:810-826](file://ModuleHsBaSlicer/hsba_slicer.cppm#L810-L826)
- [hsba_slicer.cppm:328-354](file://ModuleHsBaSlicer/hsba_slicer.cppm#L328-L354)
- [LuaAddFunction.hpp:19-24](file://LibHsBaSlicer/Extends/LuaAddFunction.hpp#L19-L24)

## Dependency Analysis
ModuleHsBaSlicer depends on:
- LibHsBaSlicer (static or shared depending on build settings).
- HsBaPipelineTypes for C-compatible config/result types.
- Eigen3 for geometry operations.
- Clipper2 for polygon math.
- Lua libraries for scripting integration.
- **File transfer libraries for network operations**.
- **Event system libraries for callback management**.
- **Custom pipeline libraries for Lua-driven workflows**.

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
Module --> Network["Network Libraries"]
Module --> Events["Event System"]
Module --> CustomPipe["Custom Pipeline Engine"]
```

**Diagram sources**
- [CMakeLists.txt:12-29](file://ModuleHsBaSlicer/CMakeLists.txt#L12-L29)
- [CMakeLists.txt:1-78](file://LibHsBaSlicer/CMakeLists.txt#L1-L78)
- [hsba_slicer.cppm:38-61](file://ModuleHsBaSlicer/hsba_slicer.cppm#L38-L61)

**Section sources**
- [CMakeLists.txt:1-46](file://ModuleHsBaSlicer/CMakeLists.txt#L1-L46)
- [CMakeLists.txt:1-78](file://LibHsBaSlicer/CMakeLists.txt#L1-L78)
- [hsba_slicer.cppm:38-61](file://ModuleHsBaSlicer/hsba_slicer.cppm#L38-L61)

## Performance Considerations
**Updated** Added comprehensive inline function optimization analysis and new custom pipeline performance optimizations

### Inline Function Optimization Strategy
The module implements strategic inline function declarations for 31 frequently-called methods across the core classes:

#### Model Class Optimizations
- **Move Operations**: `Model(Model&&)` and `operator=(Model&&)` - eliminated move overhead
- **Accessors**: `info()`, `raw()`, `name()` - direct access without function call overhead
- **Transformations**: `translate()`, `rotate()`, `scale()` - frequent geometric operations
- **Slicing**: `slice()`, `sliceD()` - core slicing operations called per layer

#### Pipeline Optimizations
- **FdmPipeline**: `sliceAll()`, `generateSupports()`, `fill()`, `generatePath()` - core processing methods
- **SlaPipeline**: `run()`, `generateFloor()`, `renderLayer()`, `savePackage()` - main workflow methods  
- **SlsPipeline**: `run()` - primary export method
- **FileTransferPipeline**: `run()` methods - file transfer operations optimized for performance
- **CustomLuaPipeline**: `run()` - custom pipeline execution optimized for performance

#### Event System Optimizations
- **Event Registration**: `addEventCallback()`, `addZipperEventCallback()`, `addDBEventCallback()` - event setup operations
- **Lua Integration**: `add2DFunction()`, `add3DFunction()`, `addFileFunction()` - function registration operations

#### Utility Optimizations
- **Lua Functions**: `luaCustomFill()`, `luaCustomFloor()`, `luaCustomSupport()` - customization entry points
- **Version Info**: `versionJson()`, `versionXml()` - simple accessor functions
- **Type Conversion**: `toDouble()`, `toInt()` - frequently-used conversion utilities

### Performance Impact Analysis
- **Reduced Function Call Overhead**: Inline functions eliminate call/return overhead for frequently-accessed methods
- **Compiler Optimization Opportunities**: Inlined code enables better compiler optimizations like constant propagation and dead code elimination
- **Memory Access Patterns**: Direct access to member variables through inline functions improves cache locality
- **Critical Path Optimization**: Slicing operations, file transfer operations, event registrations, and custom pipeline executions benefit significantly from inlining
- **Network Operation Optimization**: File transfer operations are optimized for high-throughput scenarios
- **Lua Pipeline Optimization**: Custom pipeline execution is optimized for minimal overhead in script invocation

### Best Practices
- Prefer using sliceD only when downstream algorithms require double precision; otherwise use slice to avoid conversion overhead.
- Reuse FdmPipeline/SlaPipeline instances across models to minimize repeated configuration setup.
- Disable unnecessary steps (e.g., support) when not needed to reduce computation time.
- Use appropriate image formats for SLA outputs: PNG for lossless quality, JPG for smaller files, SVG for vector scalability.
- **Leverage inline optimizations**: The module's inline design means performance-critical paths are already optimized at compile-time.
- **Optimize file transfer operations**: Configure appropriate pool sizes and batch file transfers for maximum throughput.
- **Use event callbacks judiciously**: Register only necessary event handlers to minimize overhead.
- **Optimize custom Lua pipelines**: Design efficient Lua scripts and minimize data transfer between C++ and Lua environments.

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
- **File transfer failures**:
  - Verify host/port configuration and network connectivity.
  - Check file path validity and permissions.
  - Monitor progress callbacks for detailed error information.
- **Custom Lua pipeline failures**:
  - Ensure pipeline_lua_script or pipeline_lua_source is set before running CustomLuaPipeline.
  - Verify that the Lua entry function exists and returns appropriate values.
  - Check Lua script syntax and available functions in the HsBa table.
- **Event callback issues**:
  - Ensure proper callback registration before operations begin.
  - Verify callback function signatures match expected types.
  - Check for memory management issues in callback implementations.
- **Performance Issues**: If experiencing unexpected performance problems, verify that the module is being compiled with optimization enabled (-O2 or higher) to allow proper inline expansion.

**Section sources**
- [CMakeLists.txt:36-45](file://ModuleHsBaSlicer/CMakeLists.txt#L36-L45)
- [module_anchor.cpp:1-13](file://ModuleHsBaSlicer/module_anchor.cpp#L1-L13)
- [hsba_slicer.cppm:705-706](file://ModuleHsBaSlicer/hsba_slicer.cppm#L705-L706)
- [hsba_slicer.cppm:406-418](file://ModuleHsBaSlicer/hsba_slicer.cppm#L406-L418)
- [hsba_slicer.cppm:756-758](file://ModuleHsBaSlicer/hsba_slicer.cppm#L756-L758)
- [hsba_slicer.cppm:780-781](file://ModuleHsBaSlicer/hsba_slicer.cppm#L780-L781)

## Conclusion
ModuleHsBaSlicer delivers a modern, exception-safe, and ergonomic C++20 API over LibHsBaSlicer with significant performance optimizations through strategic inline function declarations. By exporting classes like Model, FdmPipeline, SlaPipeline, SlsPipeline, **FileTransferPipeline**, and **CustomLuaPipeline** with 31 frequently-called methods optimized as inline functions, it abstracts away low-level free functions while preserving flexibility through Lua customization and maximizing runtime performance. 

The module now includes comprehensive **event-driven programming capabilities**, **robust file transfer functionality**, and **fully customizable Lua-driven workflows** that delegate entire processes to Lua scripts. The **CustomLuaPipeline** class represents a major enhancement, providing complete workflow orchestration through Lua while maintaining the performance benefits of inline optimizations. The single-file module design, careful CMake configuration, and inline optimization strategy ensure reliable consumption across platforms and toolchains while providing excellent performance characteristics for production workloads requiring complex, customizable slicing workflows.