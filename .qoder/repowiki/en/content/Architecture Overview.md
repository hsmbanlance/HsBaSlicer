# Architecture Overview

<cite>
**Referenced Files in This Document**
- [pipeline_parallel.hpp](file://DllHsBaSlicer/pipeline_parallel.hpp)
- [parallel.hpp](file://base/parallel.hpp)
- [thread_pool.hpp](file://base/thread_pool.hpp)
- [ISupport.hpp](file://support/ISupport.hpp)
- [ISupport.cpp](file://support/ISupport.cpp)
- [path_generator.cpp](file://LibHsBaSlicer/Path/path_generator.cpp)
- [gcodepath.cpp](file://paths/gcodepath.cpp)
- [fdm_pipeline.h](file://DllHsBaSlicer/fdm_pipeline.h)
- [sla_pipeline.h](file://DllHsBaSlicer/sla_pipeline.h)
- [sls_pipeline.h](file://DllHsBaSlicer/sls_pipeline.h)
- [lom_pipeline.h](file://DllHsBaSlicer/lom_pipeline.h)
- [tdp_pipeline.h](file://DllHsBaSlicer/tdp_pipeline.h)
- [waam_pipeline.h](file://DllHsBaSlicer/waam_pipeline.h)
- [export.h](file://LibHsBaSlicer/export.h)
- [model_preprocess.hpp](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp)
- [mesh_slice.hpp](file://LibHsBaSlicer/Slice/mesh_slice.hpp)
- [polygon_fill.hpp](file://LibHsBaSlicer/Fill/polygon_fill.hpp)
- [path_generator.hpp](file://LibHsBaSlicer/Path/path_generator.hpp)
- [sla_floor.hpp](file://LibHsBaSlicer/Floor/sla_floor.hpp)
- [fdm_support.hpp](file://LibHsBaSlicer/Support/fdm_support.hpp)
- [lua_pipeline.cpp](file://LibHsBaSlicer/Extends/lua_pipeline.cpp)
- [hsba_slicer.cppm](file://ModuleHsBaSlicer/hsba_slicer.cppm)
</cite>

## Update Summary
**Changes Made**
- Updated parallel processing infrastructure to reflect the new process-wide ThreadPool singleton architecture
- Added documentation for the atomic cursor-based dynamic load balancing system
- Updated support generation parallelization with IsLayerIndependent() capability flag
- Enhanced path generation parallelization documentation
- Updated G-code serialization parallelization details
- Revised cross-cutting concerns section to reflect the new parallel runtime

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
HsBaSlicer is a modular, cross-platform C++ framework for 3D-printing slicers. It exposes layered slicing capabilities through a static C++ library and a shared dynamic library with a stable C ABI. The system supports multiple manufacturing technologies: FDM, SLA, SLS, LOM, TDP (3DP), and WAAM. Its design separates foundational utilities, mesh processing, algorithmic slicing logic, technology-specific pipeline orchestration, and Lua-driven extension points.

The repository provides:
- A Foundation Layer (`base/`) with types, concurrency, memory management, and utility primitives.
- A Processing Layer (`LibHsBaSlicer/`) exposing reusable slicing APIs for preprocessing, slicing, support generation, polygon filling, path generation, and SLA floor/export.
- An Algorithm Layer (`2D/`, `meshmodel/`, `support/`) implementing geometry, hull, fill, and support algorithms.
- An Interface Layer (`DllHsBaSlicer/`) exporting per-technology C ABI pipelines.
- An Extension Layer (Lua scripts) enabling custom support, fill, floor, export, and fully custom pipelines.

**Section sources**
- [README.md:21-39](file://README.md#L21-L39)

## Project Structure
The top-level build configuration defines compiler requirements, optional copyleft kernel integration, dependency discovery, target installation layout, and sample/test inclusion. It also documents the installed header layout and CMake package export structure.

```mermaid
graph TB
Root["Root Build<br/>CMakeLists.txt"] --> Base["Foundation Layer<br/>base/"]
Root --> Utils["Utilities & Bindings<br/>utils/, fileoperator/"]
Root --> Mesh["Mesh Kernel<br/>meshmodel/"]
Root --> Preprocess["Model Preprocessing<br/>preprocess/"]
Root --> Support["Support Algorithms<br/>support/"]
Root --> Paths["Path Output Types<br/>paths/"]
Root --> Lib["Processing Layer<br/>LibHsBaSlicer/"]
Root --> Dll["Interface Layer<br/>DllHsBaSlicer/"]
Root --> Module["C++20 Module Wrapper<br/>ModuleHsBaSlicer/"]
Root --> Samples["Samples & Tests<br/>samples/, tests/"]
```

**Diagram sources**
- [CMakeLists.txt:410-454](file://CMakeLists.txt#L410-L454)
- [README.md:126-172](file://README.md#L126-L172)

**Section sources**
- [CMakeLists.txt:410-454](file://CMakeLists.txt#L410-L454)
- [README.md:100-172](file://README.md#L100-L172)

## Core Components
The core components are organized by responsibility:

| Layer | Responsibility | Key Headers / Files |
|---|---|---|
| Foundation Layer | Types, concepts, coroutines, thread pool, memory pools, error handling, units, reflection helpers | `base/thread_pool.hpp`, `base/memory_pool.hpp` |
| Processing Layer | Public C++ slicing API: model loading, slicing, support, fill, path generation, SLA floor/export | `LibHsBaSlicer/Preprocess/model_preprocess.hpp`, `LibHsBaSlicer/Slice/mesh_slice.hpp`, `LibHsBaSlicer/Support/fdm_support.hpp`, `LibHsBaSlicer/Fill/polygon_fill.hpp`, `LibHsBaSlicer/Path/path_generator.hpp`, `LibHsBaSlicer/Floor/sla_floor.hpp` |
| Algorithm Layer | 2D polygons, hulls, image-to-polygons, fill engine; mesh models; support algorithms | `2D/FloatPolygons.hpp`, `2D/PolygonFill.hpp`, `meshmodel/FullTopoModel.hpp`, `support/SupportConfig.hpp` |
| Interface Layer | Per-technology C ABI pipelines: FDM, SLA, SLS, LOM, TDP, WAAM, plus custom Lua pipeline | `DllHsBaSlicer/*_pipeline.h`, `pipelinetypes/pipeline_types.h` |
| Extension Layer | Lua bindings and scripting hooks for support, fill, floor, export, and full custom pipelines | `LibHsBaSlicer/Extends/lua_pipeline.cpp`, `types/hsba_pipeline.d.lua` |

**Section sources**
- [export.h:1-22](file://LibHsBaSlicer/export.h#L1-L22)
- [model_preprocess.hpp:22-120](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L22-L120)
- [mesh_slice.hpp:22-103](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L22-L103)
- [fdm_support.hpp:19-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L19-L67)
- [polygon_fill.hpp:17-53](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L17-L53)
- [path_generator.hpp:20-91](file://LibHsBaSlicer/Path/path_generator.hpp#L20-L91)
- [sla_floor.hpp:21-195](file://LibHsBaSlicer/Floor/sla_floor.hpp#L21-L195)

## Architecture Overview
HsBaSlicer uses a layered architecture where each layer has a clear contract and limited coupling to lower layers.

```mermaid
graph TB
subgraph "Extension Layer"
LuaScripts["Lua Scripts<br/>Custom support/fill/floor/export/custom pipeline"]
end
subgraph "Interface Layer"
FDM["FDM Pipeline<br/>fdm_pipeline.h"]
SLA["SLA Pipeline<br/>sla_pipeline.h"]
SLS["SLS Pipeline<br/>sls_pipeline.h"]
LOM["LOM Pipeline<br/>lom_pipeline.h"]
TDP["TDP Pipeline<br/>tdp_pipeline.h"]
WAAM["WAAM Pipeline<br/>waam_pipeline.h"]
Custom["Custom Lua Pipeline<br/>custom_pipeline.h"]
end
subgraph "Processing Layer"
Preprocess["Model Preprocess<br/>model_preprocess.hpp"]
Slice["Mesh Slice<br/>mesh_slice.hpp"]
Support["Support Generation<br/>fdm_support.hpp"]
Fill["Polygon Fill<br/>polygon_fill.hpp"]
PathGen["Path Generator<br/>path_generator.hpp"]
Floor["SLA Floor/Export<br/>sla_floor.hpp"]
end
subgraph "Algorithm Layer"
TwoD["2D Geometry & Fill<br/>2D/"]
MeshModel["Mesh Models<br/>meshmodel/"]
SupportAlgo["Support Algorithms<br/>support/"]
end
subgraph "Foundation Layer"
Base["Base Primitives<br/>base/"]
ParallelRuntime["Parallel Runtime<br/>parallel.hpp + thread_pool.hpp"]
end
LuaScripts --> FDM
LuaScripts --> SLA
LuaScripts --> SLS
LuaScripts --> LOM
LuaScripts --> TDP
LuaScripts --> WAAM
LuaScripts --> Custom
FDM --> Preprocess
FDM --> Slice
FDM --> Support
FDM --> Fill
FDM --> PathGen
SLA --> Preprocess
SLA --> Slice
SLA --> Floor
SLA --> Support
SLS --> Preprocess
SLS --> Slice
SLS --> LuaScripts
LOM --> Preprocess
LOM --> Slice
LOM --> LuaScripts
TDP --> Preprocess
TDP --> Slice
TDP --> LuaScripts
WAAM --> Preprocess
WAAM --> Slice
WAAM --> PathGen
Preprocess --> MeshModel
Slice --> MeshModel
Support --> SupportAlgo
Fill --> TwoD
PathGen --> TwoD
Floor --> TwoD
MeshModel --> Base
SupportAlgo --> Base
TwoD --> Base
Support --> ParallelRuntime
PathGen --> ParallelRuntime
```

**Diagram sources**
- [fdm_pipeline.h:23-35](file://DllHsBaSlicer/fdm_pipeline.h#L23-L35)
- [sla_pipeline.h:23-34](file://DllHsBaSlicer/sla_pipeline.h#L23-L34)
- [sls_pipeline.h:23-34](file://DllHsBaSlicer/sls_pipeline.h#L23-L34)
- [lom_pipeline.h:23-36](file://DllHsBaSlicer/lom_pipeline.h#L23-L36)
- [tdp_pipeline.h:23-36](file://DllHsBaSlicer/tdp_pipeline.h#L23-L36)
- [waam_pipeline.h:23-37](file://DllHsBaSlicer/waam_pipeline.h#L23-L37)
- [model_preprocess.hpp:32-120](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L32-L120)
- [mesh_slice.hpp:22-103](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L22-L103)
- [fdm_support.hpp:19-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L19-L67)
- [polygon_fill.hpp:17-53](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L17-L53)
- [path_generator.hpp:20-91](file://LibHsBaSlicer/Path/path_generator.hpp#L20-L91)
- [sla_floor.hpp:21-195](file://LibHsBaSlicer/Floor/sla_floor.hpp#L21-L195)
- [parallel.hpp:1-263](file://base/parallel.hpp#L1-L263)

### System Context Data Flow
The typical data flow from model input to exported output is:

```mermaid
flowchart TD
Input["Model Input<br/>STL/OBJ/STEP/IGES"] --> Load["Load Model<br/>model_preprocess.hpp"]
Load --> Transform["Transform & Query<br/>Translate/Rotate/Scale/BBox/Volume"]
Transform --> Topology["Build Slice Topology<br/>mesh_slice.hpp"]
Topology --> SliceLayers["Slice Layers<br/>mesh_slice.hpp"]
SliceLayers --> DecideProcess{"Technology?"}
DecideProcess --> |FDM| FDMFlow["FDM Flow"]
DecideProcess --> |SLA| SLAFlow["SLA Flow"]
DecideProcess --> |SLS| SLSFlow["SLS Flow"]
DecideProcess --> |LOM| LOMFlow["LOM Flow"]
DecideProcess --> |TDP| TDPFlow["TDP Flow"]
DecideProcess --> |WAAM| WAAMFlow["WAAM Flow"]
FDMFlow --> FDMSupport["FDM Support<br/>fdm_support.hpp"]
FDMSupport --> FDMFill["Polygon Fill<br/>polygon_fill.hpp"]
FDMFill --> FDMPath["G-code Path<br/>path_generator.hpp"]
FDMPath --> FDMExport["G-code Export"]
SLAFlow --> SLAFloor["SLA Floor/Raft<br/>sla_floor.hpp"]
SLAFloor --> SLASupport["SLA Support<br/>fdm_support.hpp"]
SLASupport --> SLAImage["Render Images<br/>sla_floor.hpp"]
SLAImage --> SLAPackage["Zip Package<br/>sla_floor.hpp"]
SLSFlow --> SLSExport["Lua Export Script"]
LOMFlow --> LOMExport["Lua Export Script"]
TDPFlow --> TDPExport["Lua Export Script"]
WAAMFlow --> WAAMPath["Robot Path Export<br/>path_generator.hpp"]
```

**Diagram sources**
- [model_preprocess.hpp:32-120](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L32-L120)
- [mesh_slice.hpp:22-103](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L22-L103)
- [fdm_support.hpp:19-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L19-L67)
- [polygon_fill.hpp:17-53](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L17-L53)
- [path_generator.hpp:20-91](file://LibHsBaSlicer/Path/path_generator.hpp#L20-L91)
- [sla_floor.hpp:21-195](file://LibHsBaSlicer/Floor/sla_floor.hpp#L21-L195)

## Detailed Component Analysis

### Technology Pipelines and Configuration Contracts
Each technology pipeline exposes:
- A default configuration constructor.
- A synchronous run function.
- An asynchronous run function.
- A result-free function.

The configuration and result structures are defined centrally in a C-compatible header so that downstream modules can use them without depending on the shared library.

```mermaid
classDiagram
class HsBaFdmPipelineConfig_t {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+double fill_spacing
+HsBaFillMode_t fill_mode
+double fill_angle
+int wall_count
+int top_layer_count
+int bottom_layer_count
+double infill_density
+int enable_support
+float overhang_angle
+float support_gap
+float support_diameter
+float support_density
+HsBaSupportPattern_t support_pattern
+int interface_layers
+float interface_density
+float line_width
+float print_speed
+float travel_speed
+float extrusion_multiplier
+int spiral_mode
+HsBaGCodeFirmware_t gcode_firmware
+float nozzle_diameter
+float filament_diameter
+float nozzle_temp
+float bed_temp
+float retract_length
+float retract_speed
+float first_layer_speed
+string support_lua_script
+string support_lua_func
+string infill_lua_script
+string infill_lua_func
+string output_path
}
class HsBaFdmPipelineResult_t {
+int success
+int total_layers
+string gcode_content
+string error_message
+double elapsed_seconds
}
class HsBaSlaPipelineConfig_t {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+float bottom_exposure_time
+float normal_exposure_time
+float bottom_lift_distance
+float lift_distance
+float lift_speed
+float retract_speed
+float floor_raft_offset
+float floor_border_width
+float floor_fill_spacing
+float floor_fill_angle
+int floor_border_count
+int floor_use_convex_hull
+int enable_support
+float overhang_angle
+float support_gap
+float support_diameter
+float support_density
+HsBaSlaSupportPattern_t support_pattern
+string support_lua_script
+string support_lua_func
+string floor_lua_script
+string floor_lua_func
+string export_lua_script
+string export_lua_func
+string output_path
+HsBaSlaImageType_t image_type
+int image_width
+int image_height
}
class HsBaSlsPipelineConfig_t {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+float laser_power
+float scan_speed
+float hatch_spacing
+float hatch_rotation
+float bed_temperature
+string export_lua_script
+string export_lua_func
+string output_path
}
class HsBaLomPipelineConfig_t {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+float cut_speed
+float cut_margin
+float cut_power
+float bond_temperature
+float bond_pressure
+float bond_time
+int seal_contour
+HsBaLomCutMode_t cut_mode
+string export_lua_script
+string export_lua_func
+string output_path
}
class HsBaTdpPipelineConfig_t {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+int head_count
+float drop_spacing
+float binder_saturation
+float ink_curing_time
+float bed_temperature
+HsBaTdpBinderMode_t binder_mode
+string export_lua_script
+string export_lua_func
+string output_path
+int spiral_mode
}
class HsBaWaamPipelineConfig_t {
+string model_name
+string model_path
+float layer_height
+float first_layer_height
+float bead_width
+float travel_speed
+float wire_feed_speed
+float arc_current
+float arc_voltage
+float gas_flow_rate
+HsBaWaamMaterial_t material
+HsBaWaamWeldProcess_t welding_process
+HsBaWaamProtection_t protection
+HsBaMetalProtectGas_t protect_gas
+float interpass_temperature
+HsBaWaamRobotType_t robot_type
+string path_lua_script
+string path_lua_func
+string output_path
+int spiral_mode
}
HsBaFdmPipelineConfig_t --> HsBaFdmPipelineResult_t : "produces"
```

**Diagram sources**
- [pipeline_types.h:23-127](file://pipelinetypes/pipeline_types.h#L23-L127)
- [pipeline_types.h:146-232](file://pipelinetypes/pipeline_types.h#L146-L232)
- [pipeline_types.h:251-297](file://pipelinetypes/pipeline_types.h#L251-L297)
- [pipeline_types.h:417-448](file://pipelinetypes/pipeline_types.h#L417-L448)
- [pipeline_types.h:477-508](file://pipelinetypes/pipeline_types.h#L477-L508)
- [pipeline_types.h:573-612](file://pipelinetypes/pipeline_types.h#L573-L612)

#### FDM Pipeline
The FDM pipeline orchestrates preprocessing, slicing, support generation, polygon filling, and G-code path generation. It supports firmware-targeted G-code output and optional spiral vase mode.

```mermaid
sequenceDiagram
participant Client as "Client"
participant FDM as "HsBaRunFdmPipeline"
participant Pre as "Model Preprocess"
participant Slice as "Mesh Slice"
participant Sup as "FDM Support"
participant Fill as "Polygon Fill"
participant Path as "Path Generator"
Client->>FDM : "Run FDM pipeline"
FDM->>Pre : "Load model"
Pre-->>FDM : "IModel"
FDM->>Slice : "Slice layers"
Slice-->>FDM : "Layer outlines"
FDM->>Sup : "Generate supports"
Sup-->>FDM : "Support polygons"
FDM->>Fill : "Generate fills"
Fill-->>FDM : "Fill polygons"
FDM->>Path : "Generate G-code paths"
Path-->>FDM : "G-code content"
FDM-->>Client : "FDM result"
```

**Diagram sources**
- [fdm_pipeline.h:23-35](file://DllHsBaSlicer/fdm_pipeline.h#L23-L35)
- [model_preprocess.hpp:32-120](file://LibHsBaSlicer/Preprocess/model_preprocess.hpp#L32-L120)
- [mesh_slice.hpp:22-103](file://LibHsBaSlicer/Slice/mesh_slice.hpp#L22-L103)
- [fdm_support.hpp:19-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L19-L67)
- [polygon_fill.hpp:17-53](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L17-L53)
- [path_generator.hpp:20-91](file://LibHsBaSlicer/Path/path_generator.hpp#L20-L91)

#### SLA Pipeline
The SLA pipeline produces layer images, optional raft/floor, optional sacrificial or cone supports, and exports a zip package.

```mermaid
sequenceDiagram
participant Client as "Client"
participant SLA as "HsBaRunSlaPipeline"
participant Pre as "Model Preprocess"
participant Slice as "Mesh Slice"
participant Floor as "SLA Floor"
participant Sup as "SLA Support"
participant Export as "SLA Package Export"
Client->>SLA : "Run SLA pipeline"
SLA->>Pre : "Load model"
Pre-->>SLA : "IModel"
SLA->>Slice : "Slice layers"
Slice-->>SLA : "Layer outlines"
SLA->>Floor : "Generate floor/raft"
Floor-->>SLA : "Floor polygons"
SLA->>Sup : "Generate supports"
Sup-->>SLA : "Support polygons"
SLA->>Export : "Render images and save zip"
Export-->>SLA : "Export path"
SLA-->>Client : "SLA result"
```

**Diagram sources**
- [sla_pipeline.h:23-34](file://DllHsBaSlicer/sla_pipeline.h#L23-L34)
- [sla_floor.hpp:21-195](file://LibHsBaSlicer/Floor/sla_floor.hpp#L21-L195)
- [fdm_support.hpp:39-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L39-L67)

#### SLS, LOM, TDP Pipelines
These powder-bed or sheet-based processes slice the model and delegate final export to Lua scripts. They do not require traditional raft/support outputs because the process medium provides support or bonding behavior.

```mermaid
flowchart TD
Start["Start Process-Specific Pipeline"] --> Load["Load Model"]
Load --> Slice["Slice Layers"]
Slice --> LuaExport["Call Lua Export Script"]
LuaExport --> Result["Return Export Path"]
```

**Diagram sources**
- [sls_pipeline.h:23-34](file://DllHsBaSlicer/sls_pipeline.h#L23-L34)
- [lom_pipeline.h:23-36](file://DllHsBaSlicer/lom_pipeline.h#L23-L36)
- [tdp_pipeline.h:23-36](file://DllHsBaSlicer/tdp_pipeline.h#L23-L36)
- [pipeline_types.h:251-297](file://pipelinetypes/pipeline_types.h#L251-L297)
- [pipeline_types.h:417-448](file://pipelinetypes/pipeline_types.h#L417-L448)
- [pipeline_types.h:477-508](file://pipelinetypes/pipeline_types.h#L477-L508)

#### WAAM Pipeline
WAAM generates robot language programs rather than layer zips. It maps sliced contours to deposition paths and optionally supports continuous spiral deposition.

```mermaid
sequenceDiagram
participant Client as "Client"
participant WAAM as "HsBaRunWaamPipeline"
participant Pre as "Model Preprocess"
participant Slice as "Mesh Slice"
participant Path as "Robot Path Export"
Client->>WAAM : "Run WAAM pipeline"
WAAM->>Pre : "Load model"
Pre-->>WAAM : "IModel"
WAAM->>Slice : "Slice layers"
Slice-->>WAAM : "Layer outlines"
WAAM->>Path : "Generate robot program"
Path-->>WAAM : "Output path"
WAAM-->>Client : "WAAM result"
```

**Diagram sources**
- [waam_pipeline.h:23-37](file://DllHsBaSlicer/waam_pipeline.h#L23-L37)
- [path_generator.hpp:64-80](file://LibHsBaSlicer/Path/path_generator.hpp#L64-L80)

### Multi-Technology Support Architecture
The multi-technology support architecture is driven by:
- Centralized C-compatible configuration and result types.
- Per-technology pipeline headers defining default config, sync run, async run, and result free functions.
- Shared processing-layer APIs reused across technologies.
- Lua extension points for custom support, fill, floor, export, and fully custom pipelines.

```mermaid
graph LR
Types["Central Types<br/>pipeline_types.h"] --> FDM["FDM Pipeline"]
Types --> SLA["SLA Pipeline"]
Types --> SLS["SLS Pipeline"]
Types --> LOM["LOM Pipeline"]
Types --> TDP["TDP Pipeline"]
Types --> WAAM["WAAM Pipeline"]
FDM --> LibAPI["LibHsBaSlicer APIs"]
SLA --> LibAPI
SLS --> LibAPI
LOM --> LibAPI
TDP --> LibAPI
WAAM --> LibAPI
LibAPI --> Lua["Lua Extensions"]
```

**Diagram sources**
- [pipeline_types.h:23-127](file://pipelinetypes/pipeline_types.h#L23-L127)
- [pipeline_types.h:146-232](file://pipelinetypes/pipeline_types.h#L146-L232)
- [pipeline_types.h:251-297](file://pipelinetypes/pipeline_types.h#L251-L297)
- [pipeline_types.h:417-448](file://pipelinetypes/pipeline_types.h#L417-L448)
- [pipeline_types.h:477-508](file://pipelinetypes/pipeline_types.h#L477-L508)
- [pipeline_types.h:573-612](file://pipelinetypes/pipeline_types.h#L573-L612)

### Lua Scripting Integration
Lua scripting is integrated at multiple levels:
- Custom support generation via `GenerateAllLuaSupport`.
- Custom fill patterns via `LuaCustomFillByFile`.
- Custom SLA floor generation via `LuaCustomFloorByFile` / `LuaCustomFloorByString`.
- Custom export for SLS, LOM, TDP, and fully custom pipelines.
- Runtime exposure of slicing, support, fill, floor, path, and export helpers through the Lua pipeline binding.

```mermaid
flowchart TD
Entry["Lua Entry Function"] --> Support["Custom Support<br/>GenerateAllLuaSupport"]
Entry --> Fill["Custom Fill<br/>LuaCustomFillByFile"]
Entry --> Floor["Custom Floor<br/>LuaCustomFloorByFile/String"]
Entry --> Export["Custom Export<br/>SLS/LOM/TDP/Custom"]
Entry --> Path["Custom Path<br/>Spiralize/WAAM Path"]
Support --> Result["Polygons"]
Fill --> Result
Floor --> Result
Export --> File["Export File/Zip"]
Path --> Robot["Robot Program/G-code"]
```

**Diagram sources**
- [fdm_support.hpp:52-67](file://LibHsBaSlicer/Support/fdm_support.hpp#L52-L67)
- [polygon_fill.hpp:39-53](file://LibHsBaSlicer/Fill/polygon_fill.hpp#L39-L53)
- [sla_floor.hpp:80-116](file://LibHsBaSlicer/Floor/sla_floor.hpp#L80-L116)
- [lua_pipeline.cpp:537-577](file://LibHsBaSlicer/Extends/lua_pipeline.cpp#L537-L577)
- [hsba_slicer.cppm:731-750](file://ModuleHsBaSlicer/hsba_slicer.cppm#L731-L750)

### Cross-Cutting Concerns

#### Parallel Processing Infrastructure

**Updated** The parallel processing system has been significantly enhanced with a process-wide ThreadPool singleton and atomic cursor-based dynamic load balancing.

The foundation layer now provides:
- **Process-wide ThreadPool Singleton**: `PipelinePool()` returns a static ThreadPool instance that lives until process exit, eliminating per-call thread pool creation overhead.
- **Atomic Cursor Scheduling**: Workers use `std::atomic<int> cursor.fetch_add(1)` for dynamic work distribution, providing better load balancing than fixed block sizes.
- **Progress Callbacks**: Progress callbacks fire on the calling thread every 32 completed layers to minimize synchronization overhead.
- **Environment Configuration**: `HSBA_PIPELINE_THREADS` environment variable controls worker count, with fallback to `hardware_concurrency()`.

```mermaid
classDiagram
class PipelinePool {
+static ThreadPool& PipelinePool()
+static size_t ResolvePipelineThreads()
+static size_t PipelineThreadCount()
}
class ParallelForLayers {
+void ParallelForLayers(total, work, on_progress)
+template R ParallelMapLayers(total, map)
}
class RunAtomicIndexed {
+std : : atomic~cursor~
+std : : atomic~done~
+WorkerLoop()
}
PipelinePool --> ThreadPool : "manages"
ParallelForLayers --> RunAtomicIndexed : "uses"
RunAtomicIndexed --> PipelinePool : "accesses"
```

**Diagram sources**
- [parallel.hpp:30-75](file://base/parallel.hpp#L30-L75)
- [parallel.hpp:77-176](file://base/parallel.hpp#L77-L176)
- [parallel.hpp:178-219](file://base/parallel.hpp#L178-L219)

#### Support Generation Parallelization

**Updated** Support generation now leverages the parallel infrastructure with capability detection.

Key enhancements:
- **IsLayerIndependent() Capability Flag**: Base class method allows implementations to declare whether they can be parallelized safely.
- **Automatic Parallelization**: `ISupport::GenerateAll` automatically uses `ParallelMapLayers` when `IsLayerIndependent()` returns true.
- **Serial Fallback**: Implementations requiring sequential access (like tree supports) override `IsLayerIndependent()` to return false.

```mermaid
flowchart TD
Start["GenerateAll(layers, config)"] --> CheckIndependence{"IsLayerIndependent()?"}
CheckIndependence --> |true| Parallel["ParallelMapLayers<PolygonsD>"]
CheckIndependence --> |false| Serial["Serial loop"]
Parallel --> Worker["Worker: Generate(layers[i], prev, height, config)"]
Worker --> Results["Collect results"]
Serial --> Sequential["Sequential iteration"]
Sequential --> Results
Results --> Return["Return PolygonsD vector"]
```

**Diagram sources**
- [ISupport.hpp:46-54](file://support/ISupport.hpp#L46-L54)
- [ISupport.cpp:14-46](file://support/ISupport.cpp#L14-L46)

#### Path Generation Parallelization

**Updated** Path generation functions now parallelize the computationally intensive per-layer operations while maintaining thread safety.

Enhanced functions:
- **GenerateGCodePath**: Uses `ParallelMapLayers<LayerPts>` to generate outline/fill/support point lists concurrently, then serially inserts into the shared PointsPath.
- **GenerateGCodePathV2**: Parallelizes combined polygon assembly using `ParallelMapLayers<LayerBundle>`, preserving GCodePath thread safety.
- **GenerateGCodePathSpiral**: Remains serial as it operates on a single continuous helix.

**Section sources**
- [path_generator.cpp:66-112](file://LibHsBaSlicer/Path/path_generator.cpp#L66-L112)
- [path_generator.cpp:114-149](file://LibHsBaSlicer/Path/path_generator.cpp#L114-L149)

#### G-code Serialization Parallelization

**Updated** G-code string concatenation is now parallelized for large models.

The `ToGCode` method now:
- Uses `ParallelMapLayers<std::string>` to generate layer bodies concurrently
- Stitches header + wall + bodies + footer on the caller thread
- Preserves exact ordering semantics with `HSBA_PIPELINE_THREADS=1`

**Section sources**
- [gcodepath.cpp:333-368](file://paths/gcodepath.cpp#L333-L368)

#### Memory Management
The foundation layer includes fixed-size memory pool allocators for predictable allocation behavior and reduced heap fragmentation during intensive geometric computations.

```mermaid
classDiagram
class MemoryPoolState~PoolSize~ {
+char[] pool
+bool[] used
+size_t used_count
}
class MemoryPool~T, PoolSize~ {
+allocate(n) pointer
+deallocate(p, n) void
+max_size() size_type
+construct(p, args) void
+destroy(p) void
+get_state() shared_ptr
}
class StaticMemoryPool~T, PoolSize~ {
+allocate(n) pointer
+deallocate(p, n) void
+max_size() size_type
+construct(p, args) void
+destroy(p) void
+UsedCount() size_t
}
MemoryPool --> MemoryPoolState : "owns state"
```

**Diagram sources**
- [memory_pool.hpp:28-208](file://base/memory_pool.hpp#L28-L208)
- [memory_pool.hpp:215-372](file://base/memory_pool.hpp#L215-L372)

## Dependency Analysis
The project's third-party dependencies are declared in the vcpkg manifest and discovered by the root CMake configuration. Copyleft kernels CGAL and OpenCascade are conditionally enabled, affecting both linking and license terms.

```mermaid
graph TB
Project["HsBaSlicer Project"] --> Boost["Boost<br/>log, locale, dll, property-tree, etc."]
Project --> Clipper2["Clipper2"]
Project --> Eigen3["Eigen3"]
Project --> RapidJSON["RapidJSON"]
Project --> Protobuf["Protobuf"]
Project --> OpenSSL["OpenSSL"]
Project --> TinyXML2["tinyxml2"]
Project --> YAML["yaml-cpp"]
Project --> MagicEnum["magic_enum"]
Project --> Lua["Lua"]
Project --> Miniz["miniz"]
Project --> Bit7z["bit7z"]
Project --> OpenCV["OpenCV"]
Project --> Sqlpp11["sqlpp11"]
Project --> IGL["libigl"]
Project --> CGAL["CGAL (optional copyleft)"]
Project --> OCCT["OpenCASCADE (optional copyleft)"]
Project --> OpenVDB["OpenVDB"]
```

**Diagram sources**
- [vcpkg.json:10-87](file://vcpkg.json#L10-L87)
- [vcpkg.json:89-110](file://vcpkg.json#L89-L110)
- [CMakeLists.txt:210-349](file://CMakeLists.txt#L210-L349)
- [CMakeLists.txt:284-318](file://CMakeLists.txt#L284-L318)

### Version Compatibility Considerations
- CMake minimum version: 3.28.
- C++ standard: C++20 required; C standard varies by compiler capability, up to C23 when supported.
- Optional features guarded by compile-time checks: concepts, ranges, source_location, non-type template parameters, template-template matching, coroutines, explicit this, C23 constexpr.
- Copyleft kernel feature controlled by `HSBA_COPL` and vcpkg manifest feature `copyleft`.
- Platform-specific toggles for desktop/mobile/game console, DLL loader availability, bit7z usage, and test execution.

**Section sources**
- [CMakeLists.txt:4-38](file://CMakeLists.txt#L4-L38)
- [CMakeLists.txt:48-104](file://CMakeLists.txt#L48-L104)
- [CMakeLists.txt:170-204](file://CMakeLists.txt#L170-L204)
- [CMakeLists.txt:228-262](file://CMakeLists.txt#L228-L262)
- [CMakeLists.txt:284-318](file://CMakeLists.txt#L284-L318)

## Performance Considerations
- Use prebuilt slicing topology for multi-layer workflows to avoid rebuilding topology per layer.
- Prefer closed-contour slicing for material-fed processes; unsafe slicing may be considered only where open contours are meaningful.
- Enable parallel task submission through the thread pool for independent layer operations where safe.
- Tune memory pool sizes and model pool capacity based on available copyleft kernel support.
- Avoid boolean operations in Debug builds due to performance and memory constraints.
- **New**: Set `HSBA_PIPELINE_THREADS` environment variable before process startup to control parallelism level.
- **New**: Use `IsLayerIndependent()` capability flag to safely parallelize support generation.

[No sources needed since this section provides general guidance]

## Troubleshooting Guide
Common issues and their architectural context:

| Issue | Likely Cause | Recommended Action |
|---|---|---|
| Missing copyleft kernels | CGAL or OpenCascade not found or `copyleft` feature disabled | Enable vcpkg `copyleft` feature or install CGAL/OpenCascade; verify `HSBA_COPL` detection |
| Boolean operations slow or high memory | Debug build disables or limits boolean operations | Use Release build or reduce model complexity; consider disabling boolean operations |
| Lua script errors | Invalid script path, missing function name, or incompatible Lua API | Validate script path, function name, and Lua table shapes against documented fields |
| Thread pool exceptions | Submitting tasks after pool shutdown or invalid thread count | Ensure pool lifecycle matches pipeline lifetime; validate thread count |
| Memory allocation failures | Fixed memory pool exhausted | Increase pool size or reduce per-operation allocations |
| Poor parallel performance | `HSBA_PIPELINE_THREADS` not set or set too low | Set environment variable before process startup; monitor with `PipelineThreadCount()` |
| Support generation not parallelizing | `IsLayerIndependent()` returns false | Verify support implementation doesn't require sequential access |
| G-code serialization bottleneck | Large models with many layers | Leverage built-in parallelization; consider reducing layer count |

**Section sources**
- [CMakeLists.txt:284-318](file://CMakeLists.txt#L284-L318)
- [thread_pool.hpp:37-90](file://base/thread_pool.hpp#L37-L90)
- [memory_pool.hpp:111-141](file://base/memory_pool.hpp#L111-L141)
- [memory_pool.hpp:266-296](file://base/memory_pool.hpp#L266-L296)
- [parallel.hpp:37-54](file://base/parallel.hpp#L37-L54)
- [ISupport.hpp:46-54](file://support/ISupport.hpp#L46-L54)

## Conclusion
HsBaSlicer's architecture cleanly separates concerns across foundation, processing, algorithm, interface, and extension layers. The central type definitions and per-technology pipeline contracts provide a stable surface for external integrations. Lua scripting offers flexible customization for support, fill, floor, export, and fully custom pipelines. 

The recent enhancements to the parallel processing infrastructure significantly improve performance through process-wide thread pool management, atomic cursor-based dynamic load balancing, and capability-aware parallelization. These changes maintain backward compatibility while providing substantial performance gains for large models and complex geometries.

The dependency graph reflects a pragmatic balance between modern C++ tooling, established geometry libraries, and platform-specific considerations. For production use, prefer Release builds, leverage prebuilt slicing topology, configure copyleft kernels according to licensing requirements, and tune `HSBA_PIPELINE_THREADS` for optimal parallel performance.

[No sources needed since this section summarizes without analyzing specific files]