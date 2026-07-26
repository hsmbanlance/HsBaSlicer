# DllHsBaSlicer Module

DllHsBaSlicer is the **upper-level C export shared library** of HsBaSlicer. It wraps the C++ slicing core of [LibHsBaSlicer](../LibHsBaSlicer/) behind a pure C ABI so that any host language — **Qt / wxWidgets / Unity / Unreal Engine / C# / Java / Python / Swift** — can call the slicer cross-platform.

Internally the pipeline scheduling is built on C++20 coroutines, and both **synchronous** and **asynchronous** call styles are provided.

## Design Position

```
Host application (Qt / wxWidgets / Unity / UE / Android / iOS ...)
        |  pure C ABI (extern "C")
        v
DllHsBaSlicer  <- this module: C export layer (dll / so / dylib / static lib)
        |  calls only LibHsBaSlicer exported functions
        v
LibHsBaSlicer  <- C++ static lib: Preprocess / Slice / Support / Fill / Path Generation
```

Constraint: DllHsBaSlicer may only call LibHsBaSlicer's exported interfaces and never touches lower-level modules directly, keeping the ABI boundary clean.

## Build Artifacts

| Platform | Artifact | Notes |
| --- | --- | --- |
| Windows | `DllHsBaSlicer.dll` + `.lib` import library | Output to `bin/<CONFIG>/` |
| Linux | `libDllHsBaSlicer.so` | Shared library |
| macOS | `libDllHsBaSlicer.dylib` | Shared library |
| Android | `libDllHsBaSlicer.so` | Shared library, called via JNI |
| iOS | `libDllHsBaSlicer.a` | **Static library** (iOS forbids loading arbitrary dylibs) |

All public headers are exported to `include/HsBaSlicer/` in the install tree.

## Headers

| Header | Content |
| --- | --- |
| `dllexport.h` | `HSBA_SLICER_API` export macro |
| `initialize.h` | `initialize()` global initialization (must be called first) |
| `model_preprocess.h` | Model preprocessing interface (load/transform/query/boolean/shell) |
| `fdm_pipeline.h` | FDM full-pipeline interface |
| `sla_pipeline.h` | SLA full-pipeline interface |
| `sls_pipeline.h` | SLS full-pipeline interface |
| `file_transfer_pipeline.h` | File transfer pipeline interface (sync/async) |
| `pipeline_convert.h` | Proto serialized bytes ↔ C struct conversion |
| `lua_register.h` | Lua extension function registration (2D/3D/File/Event callbacks) |
| `version_info.h` | Version information (JSON / XML strings) |
| `pipelinetypes/pipeline_types.h` | All config/result structs, enums, callback types and inline default initializers (**no DLL dependency**, can be included standalone) |

## API Overview

### Initialization

```c
void initialize(void);   // call once after process startup
```

### Model Preprocessing

Standalone model management interface for operating on models before or outside pipeline execution. Models are referenced via opaque handles (`void*`) with internal reference counting for lifetime management.

#### Basic Operations

```c
void* HsBaLoadModel(const char* name, const char* file_path);  // Load model (IGL: STL/OBJ/PLY/OFF; OCCT: STEP/IGES/VRML/BREP)
void* HsBaGetModel(const char* name);                          // Get a previously loaded model
void  HsBaRemoveModel(const char* name);                       // Remove model from pool
int   HsBaContainsModel(const char* name);                     // Check if model exists
int   HsBaModelCount(void);                                    // Number of models in pool
int   HsBaCleanupModels(void);                                 // Clean up unreferenced models
```

#### Transform Operations

```c
int HsBaTranslateModel(const char* name, float tx, float ty, float tz);       // Translation
int HsBaRotateModel(const char* name, float qx, float qy, float qz, float qw); // Rotation (quaternion)
int HsBaScaleModelUniform(const char* name, float scale);                      // Uniform scale
int HsBaScaleModel(const char* name, float sx, float sy, float sz);            // Non-uniform scale
```

#### Query Operations

```c
int HsBaGetModelInfo(const char* name, float out_bbox_min[3], float out_bbox_max[3], float* out_volume);
```

#### Advanced Operations (require CGAL/OCCT)

```c
void* HsBaThickSolidModel(const char* source_name, const char* result_name, float thickness);  // Shell (requires OCCT BRep model)
void* HsBaBooleanUnion(const char* left_name, const char* right_name, const char* result_name);        // Boolean union
void* HsBaBooleanIntersection(const char* left_name, const char* right_name, const char* result_name); // Boolean intersection
void* HsBaBooleanDifference(const char* left_name, const char* right_name, const char* result_name);   // Boolean difference
void* HsBaBooleanXor(const char* left_name, const char* right_name, const char* result_name);          // Boolean XOR
```

#### Handle Management

```c
void HsBaReleaseModelHandle(void* handle);  // Release handle reference (model stays in pool)
```

> **Kernel routing strategy**: Boolean operations prefer OCCT (BRep-BRep); falls back to IGL/CGAL for mesh models. ThickSolid only supports OCCT BRep models. Advanced operations are compile-time gated by the `USE_CGAL` macro and return NULL when unavailable.

#### Model Preprocessing Example

```c
#include "initialize.h"
#include "model_preprocess.h"

int main(void)
{
    initialize();

    // Load mesh model (IGL)
    void* h1 = HsBaLoadModel("part_a", "models/part_a.stl");
    void* h2 = HsBaLoadModel("part_b", "models/part_b.stl");

    // Transform
    HsBaTranslateModel("part_b", 10.0f, 0.0f, 0.0f);

    // Boolean union (IGL/CGAL fallback for mesh)
    void* merged = HsBaBooleanUnion("part_a", "part_b", "merged");

    // Query
    float bmin[3], bmax[3], vol;
    HsBaGetModelInfo("merged", bmin, bmax, &vol);

    // Release handles
    HsBaReleaseModelHandle(h1);
    HsBaReleaseModelHandle(h2);
    HsBaReleaseModelHandle(merged);

    // Cleanup when done
    HsBaCleanupModels();
    return 0;
}
```

### FDM Pipeline

Preprocess -> Slice -> Support -> Fill -> Path Generation, outputs standard 3D printer G-code (supports Marlin / RepRap / Klipper firmware formats).

```c
HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                           HsBaProgressCallback callback, void* user_data);

void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config,
                             HsBaProgressCallback callback, void* user_data,
                             HsBaResultCallback result_callback, void* result_user_data);

void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);
```

#### GCode Firmware Selection

Use the `gcode_firmware` field to specify the target firmware for standards-compliant GCode output:

| Enum Value | Firmware | Features |
| --- | --- | --- |
| `HSBA_GCODE_MARLIN` | Marlin (default) | M104/M109 temp wait, G92 E0, M82/M83 |
| `HSBA_GCODE_REPRAP` | RepRap/RRF | Additional M106 fan control |
| `HSBA_GCODE_KLIPPER` | Klipper | SET_PRESSURE_ADVANCE, M220/M221, SET_FAN_SPEED |

#### Printer Configuration Fields (New)

| Field | Default | Description |
| --- | --- | --- |
| `gcode_firmware` | `HSBA_GCODE_MARLIN` | Target firmware type |
| `nozzle_diameter` | 0.4 | Nozzle diameter (mm) |
| `filament_diameter` | 1.75 | Filament diameter (mm) |
| `nozzle_temp` | 200.0 | Nozzle temperature (°C) |
| `bed_temp` | 60.0 | Bed temperature (°C) |
| `retract_length` | 1.0 | Retraction length (mm) |
| `retract_speed` | 40.0 | Retraction speed (mm/s) |
| `first_layer_speed` | 20.0 | First layer speed (mm/s) |

### SLA Pipeline

Preprocess -> Slice -> Floor/Raft -> Support -> Export (zip of layer images), outputs a PNG/JPG/SVG layer-image archive.

```c
HsBaSlaPipelineConfig_t HsBaCreateDefaultSlaConfig(void);

HsBaSlaPipelineResult_t HsBaRunSlaPipeline(const HsBaSlaPipelineConfig_t* config,
                                           HsBaSlaProgressCallback callback, void* user_data);

void HsBaRunSlaPipelineAsync(const HsBaSlaPipelineConfig_t* config,
                             HsBaSlaProgressCallback callback, void* user_data,
                             HsBaSlaResultCallback result_callback, void* result_user_data);

void HsBaFreeSlaPipelineResult(HsBaSlaPipelineResult_t* result);
```

### SLS Pipeline

Preprocess -> Slice -> Lua-script export (no standard output format; entirely determined by the export script).

```c
HsBaSlsPipelineConfig_t HsBaCreateDefaultSlsConfig(void);

HsBaSlsPipelineResult_t HsBaRunSlsPipeline(const HsBaSlsPipelineConfig_t* config,
                                           HsBaSlsProgressCallback callback, void* user_data);

void HsBaRunSlsPipelineAsync(const HsBaSlsPipelineConfig_t* config,
                             HsBaSlsProgressCallback callback, void* user_data,
                             HsBaSlsResultCallback result_callback, void* result_user_data);

void HsBaFreeSlsPipelineResult(HsBaSlsPipelineResult_t* result);
```

> The SLS `export_lua_script` field **must not be NULL**.

### File Transfer Pipeline

Validate -> Establish connection pool -> Transfer files sequentially to a remote executor service.

```c
HsBaFileTransferPipelineConfig_t HsBaCreateDefaultFileTransferConfig(void);

HsBaFileTransferPipelineResult_t HsBaRunFileTransferPipeline(
    const HsBaFileTransferPipelineConfig_t* config,
    HsBaFileTransferProgressCallback callback, void* user_data);

void HsBaRunFileTransferPipelineAsync(
    const HsBaFileTransferPipelineConfig_t* config,
    HsBaFileTransferProgressCallback callback, void* user_data,
    HsBaFileTransferResultCallback result_callback, void* result_user_data);

void HsBaFreeFileTransferPipelineResult(HsBaFileTransferPipelineResult_t* result);
```

#### Configuration Fields

| Field | Default | Description |
| --- | --- | --- |
| `host` | NULL | Remote host address |
| `port` | NULL | Remote service port |
| `pool_size` | 4 | Connection pool size [1, 16] |
| `file_paths` | NULL | Array of file paths to transfer |
| `file_count` | 0 | Number of files |

### Proto Serialization Conversion

Bidirectional conversion between C structs and Protobuf serialized bytes, suitable for cross-process / cross-language communication. All output buffers are allocated with `malloc`; the caller is responsible for `free`.

```c
// FDM
int HsBaFdmConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFdmPipelineConfig_t* config);
int HsBaFdmConfigToProtoBytes(const HsBaFdmPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaFdmResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFdmPipelineResult_t* result);
int HsBaFdmResultToProtoBytes(const HsBaFdmPipelineResult_t* result, void** out_data, int* out_size);

// SLA
int HsBaSlaConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlaPipelineConfig_t* config);
int HsBaSlaConfigToProtoBytes(const HsBaSlaPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaSlaResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlaPipelineResult_t* result);
int HsBaSlaResultToProtoBytes(const HsBaSlaPipelineResult_t* result, void** out_data, int* out_size);

// SLS
int HsBaSlsConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlsPipelineConfig_t* config);
int HsBaSlsConfigToProtoBytes(const HsBaSlsPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaSlsResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlsPipelineResult_t* result);
int HsBaSlsResultToProtoBytes(const HsBaSlsPipelineResult_t* result, void** out_data, int* out_size);

// File Transfer
int HsBaFileTransferConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineConfig_t* config);
int HsBaFileTransferConfigToProtoBytes(const HsBaFileTransferPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaFileTransferResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineResult_t* result);
int HsBaFileTransferResultToProtoBytes(const HsBaFileTransferPipelineResult_t* result, void** out_data, int* out_size);

// Memory cleanup
void HsBaFreeFdmConfigStrings(HsBaFdmPipelineConfig_t* config);
void HsBaFreeSlaConfigStrings(HsBaSlaPipelineConfig_t* config);
void HsBaFreeSlsConfigStrings(HsBaSlsPipelineConfig_t* config);
void HsBaFreeFileTransferConfigStrings(HsBaFileTransferPipelineConfig_t* config);
```

> Proto message definitions are in the `proto/` directory (`fdm_pipeline.proto`, `sla_pipeline.proto`, `sls_pipeline.proto`, `file_transfer_pipeline.proto`), with multi-language output support for C++/C#/Java/Python/PHP.

### Version Information

```c
char* HsBaGetVersionJson(void);   // free with HsBaFreeVersionString
char* HsBaGetVersionXml(void);
void  HsBaFreeVersionString(char* str);
```

### Lua Extension Function Registration

Register external Lua functions before running pipelines; they are automatically injected when each stage creates its Lua environment:

```c
typedef void (*HsBaLuaRegFn)(lua_State*);

void HsBaAdd2DFunction(HsBaLuaRegFn func);       // 2D (Support, Fill, SLA Output)
void HsBaAdd3DFunction(HsBaLuaRegFn func);       // 3D (Slice, Support)
void HsBaAddFileFunction(HsBaLuaRegFn func);     // File (SLS Output, SLA Output)
void HsBaAddEventCallback(const char* event_name, HsBaLuaRegFn func);  // Event callback
```

Example:

```c
#include "initialize.h"
#include "lua_register.h"
#include <lua.hpp>

static int my_custom_func(lua_State* L) {
    // custom implementation
    return 0;
}

static void register_my_functions(lua_State* L) {
    lua_register(L, "my_custom_func", my_custom_func);
}

int main(void) {
    initialize();
    HsBaAdd3DFunction(register_my_functions);  // Register for slice/support stages
    // ... run pipeline
    return 0;
}
```

## Callback & Threading Model

```c
typedef void (*HsBaProgressCallback)(int percent, const char* stage, void* user_data);
typedef void (*HsBaResultCallback)(HsBaFdmPipelineResult_t result, void* user_data);
```

- Progress and result callbacks fire on the **library's internal worker thread**, never on the caller's UI thread;
- Hosts (Qt/wxWidgets/Unity/UE) must marshal back to their UI/game thread before updating widgets;
- `stage` is a UTF-8 string valid only during the callback — copy it if you need to keep it;
- The `config` pointer passed to async functions is read only during the call; it may be freed or reused after the call returns.

## Memory Management Rules

1. `HsBaCreateDefault*Config()` returns a **value-type** struct that needs no freeing; the caller guarantees the lifetime of memory referenced by its string fields;
2. `gcode_content` / `export_path` / `error_message` inside result structs are allocated by the library and **must** be released with the matching `HsBaFree*PipelineResult()`;
3. Version strings must be freed with `HsBaFreeVersionString()`;
4. Model handles (`void*` returned by `HsBaLoadModel` / `HsBaGetModel` / `HsBaBoolean*` / `HsBaThickSolidModel`) must be released with `HsBaReleaseModelHandle()`;
5. `pipeline_types.h` also provides DLL-independent inline initializers `HsBaFdmConfigDefault()` / `HsBaSlaConfigDefault()` / `HsBaSlsConfigDefault()` / `HsBaFileTransferConfigDefault()`, handy for header-only scenarios (e.g. mirroring structs for P/Invoke);
6. Proto deserialization (`*FromProtoBytes`) allocates string fields with `malloc`—release them with the matching `HsBaFree*ConfigStrings()`; `*ToProtoBytes` output buffers (`out_data`) must be `free`'d by the caller.

## Minimal Example (C/C++)

```c
#include "initialize.h"
#include "fdm_pipeline.h"

static void OnProgress(int percent, const char* stage, void* ud) { /* ... */ }

int main(void)
{
    initialize();

    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name  = "stanford_bunny";
    cfg.model_path  = "models/stanford_bunny.stl";
    cfg.output_path = "output/bunny.gcode";
    cfg.gcode_firmware = HSBA_GCODE_MARLIN;  // or: HSBA_GCODE_REPRAP, HSBA_GCODE_KLIPPER
    cfg.nozzle_temp = 210.0f;
    cfg.bed_temp    = 60.0f;

    HsBaFdmPipelineResult_t r = HsBaRunFdmPipeline(&cfg, OnProgress, NULL);
    if (r.success) { /* use r.gcode_content ... */ }
    HsBaFreePipelineResult(&r);
    return 0;
}
```

## Integration Guides

- **[Qt / wxWidgets Desktop Integration](./qt_wxwidgets_integration.md)** — CMake linking, worker threads, progress bars, signal-slot / CallAfter marshalling
- **[Unity / Unreal Engine Integration](./game_engine_integration.md)** — C# P/Invoke, UE ThirdParty module, Blueprint wrappers, per-platform packaging

## Related Samples

- `samples/FDM/` — FDM sync/async, Lua custom support & infill full examples
- `samples/SLA/` — SLA pipeline with Lua custom floor/support/export examples
- `samples/SLS/` — SLS pipeline with Lua export example
- `android/` — Android JNI sample project
- `ios/HsBaSlicerExample/` — iOS Swift bridging sample
