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
| `slm_pipeline.h` | SLM metal powder-bed full-pipeline interface |
| `lom_pipeline.h` | LOM laminated-object full-pipeline interface |
| `tdp_pipeline.h` | 3DP binder-jetting full-pipeline interface |
| `waam_pipeline.h` | WAAM arc-additive (robot) full-pipeline interface |
| `file_transfer_pipeline.h` | File transfer pipeline interface (sync/async) |
| `custom_pipeline.h` | Custom Lua pipeline interface (sync/async) |
| `pipeline_convert.h` | Proto serialized bytes ↔ C struct conversion |
| `lua_register.h` | Lua extension function registration (2D/3D/File/Event callbacks) |
| `event_source_register.h` | C++ event source registration (Zipper progress / DB events) |
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
| `spiral_mode` | 0 | Spiralize the outer wall into a single continuous rising path (vase mode, 0=false, 1=true); when enabled the outer contour is emitted as one extrusion-continuous helix and per-layer infill/support are skipped |

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

### SLM Pipeline

Selective Laser Melting of metal powder beds (laser / e-beam). The flow mirrors SLS exactly (Preprocess -> Slice -> Lua-script export: zip + database registration, no floor/support), additionally carrying metal-specific parameters (material, energy source, shielding gas) that are folded into the config handed to the export script.

```c
HsBaSlmPipelineConfig_t HsBaCreateDefaultSlmConfig(void);

HsBaSlmPipelineResult_t HsBaRunSlmPipeline(const HsBaSlmPipelineConfig_t* config,
                                           HsBaSlmProgressCallback callback, void* user_data);

void HsBaRunSlmPipelineAsync(const HsBaSlmPipelineConfig_t* config,
                             HsBaSlmProgressCallback callback, void* user_data,
                             HsBaSlmResultCallback result_callback, void* result_user_data);

void HsBaFreeSlmPipelineResult(HsBaSlmPipelineResult_t* result);
```

#### Configuration Fields

| Field | Default | Description |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.06 / 0.08 | Layer / first-layer height (mm) |
| `laser_power` | 200.0 | Laser power (W) |
| `scan_speed` | 1000.0 | Scan speed (mm/s) |
| `hatch_spacing` / `hatch_rotation` | 0.1 / 67.0 | Hatch line spacing (mm) / inter-layer rotation (deg) |
| `bed_temperature` | 100.0 | Powder bed temperature (°C) |
| `material` | `HSBA_SLM_MATERIAL_TITANIUM` | Metal powder: IRON / ALUMINUM / TITANIUM / UNKNOWN |
| `light_source` | `HSBA_SLM_LIGHT_LASER` | Energy source: LASER / EBEAM / UNKNOWN |
| `protect_gas` | `HSBA_METAL_GAS_ARGON` | Shielding gas: ARGON / HELIUM / N2 / CO2 / UNKNOWN |
| `export_lua_script` | NULL | Export Lua script path (**must not be NULL**) |
| `export_lua_func` | NULL | Export function name, `export_slm` when NULL |
| `output_path` | NULL | Output path |

### LOM Pipeline

Laminated Object Manufacturing (sheet-by-sheet bonding + laser cutting). Slices per sheet thickness and hands each layer's contour outlines plus cut/bond parameters to the Lua export script.

```c
HsBaLomPipelineConfig_t HsBaCreateDefaultLomConfig(void);

HsBaLomPipelineResult_t HsBaRunLomPipeline(const HsBaLomPipelineConfig_t* config,
                                           HsBaLomProgressCallback callback, void* user_data);

void HsBaRunLomPipelineAsync(const HsBaLomPipelineConfig_t* config,
                             HsBaLomProgressCallback callback, void* user_data,
                             HsBaLomResultCallback result_callback, void* result_user_data);

void HsBaFreeLomPipelineResult(HsBaLomPipelineResult_t* result);
```

#### Configuration Fields

| Field | Default | Description |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.2 / 0.2 | Sheet / first-sheet thickness (mm) |
| `cut_speed` / `cut_power` / `cut_margin` | 300.0 / 0.8 / 0.5 | Laser cut speed (mm/s) / power [0,1] / contour offset (mm) |
| `bond_temperature` / `bond_pressure` / `bond_time` | 150.0 / 1.0 / 5.0 | Bonding temperature (°C) / pressure (MPa) / per-layer time (s) |
| `seal_contour` | 1 | Seal part edge (0=false, 1=true) |
| `cut_mode` | `HSBA_LOM_CUT_CONTOUR` | Cut mode: CONTOUR / HALFTONE |
| `export_lua_script` | NULL | Export Lua script path (**must not be NULL**) |
| `export_lua_func` | NULL | Export function name, `export_lom` when NULL |
| `output_path` | NULL | Output path |

### 3DP Pipeline

Binder jetting (powder bed + liquid binder). Slices per layer and hands each layer's binder-jet outlines plus head/curing parameters to the Lua export script.

```c
HsBaTdpPipelineConfig_t HsBaCreateDefaultTdpConfig(void);

HsBaTdpPipelineResult_t HsBaRunTdpPipeline(const HsBaTdpPipelineConfig_t* config,
                                           HsBaTdpProgressCallback callback, void* user_data);

void HsBaRunTdpPipelineAsync(const HsBaTdpPipelineConfig_t* config,
                             HsBaTdpProgressCallback callback, void* user_data,
                             HsBaTdpResultCallback result_callback, void* result_user_data);

void HsBaFreeTdpPipelineResult(HsBaTdpPipelineResult_t* result);
```

#### Configuration Fields

| Field | Default | Description |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.1 / 0.12 | Layer / first-layer height (mm) |
| `head_count` | 128 | Print-head nozzle count |
| `drop_spacing` | 0.05 | Binder drop spacing (mm) |
| `binder_saturation` | 0.6 | Binder saturation [0,1] |
| `ink_curing_time` | 1.0 | Per-layer curing time (s) |
| `bed_temperature` | 40.0 | Powder bed temperature (°C) |
| `binder_mode` | `HSBA_TDP_SINGLE` | Mode: FULL_COLOR / SINGLE / SINTERING |
| `spiral_mode` | 0 | Spiralize outer contour into a continuous rising path (0=false, 1=true) |
| `export_lua_script` | NULL | Export Lua script path (**must not be NULL**) |
| `export_lua_func` | NULL | Export function name, `export_tdp` when NULL |
| `output_path` | NULL | Output path |

### WAAM Pipeline

Wire Arc Additive Manufacturing (robot bead-by-bead metal deposition). Fundamentally different from bed-based modes: the output is a **robot language program** (ABB / KUKA / FANUC) rather than a layer-image zip. The `UNKNOWN` robot type requires a Lua path script to customize code generation.

```c
HsBaWaamPipelineConfig_t HsBaCreateDefaultWaamConfig(void);

HsBaWaamPipelineResult_t HsBaRunWaamPipeline(const HsBaWaamPipelineConfig_t* config,
                                             HsBaWaamProgressCallback callback, void* user_data);

void HsBaRunWaamPipelineAsync(const HsBaWaamPipelineConfig_t* config,
                              HsBaWaamProgressCallback callback, void* user_data,
                              HsBaWaamResultCallback result_callback, void* result_user_data);

void HsBaFreeWaamPipelineResult(HsBaWaamPipelineResult_t* result);
```

#### Configuration Fields

| Field | Default | Description |
| --- | --- | --- |
| `layer_height` / `first_layer_height` | 0.8 / 1.0 | Bead/layer height / first-layer height (mm) |
| `bead_width` | 1.2 | Deposited bead width (mm) |
| `travel_speed` / `wire_feed_speed` | 8.0 / 5.0 | Torch travel speed (mm/s) / wire feed speed (m/min) |
| `arc_current` / `arc_voltage` | 180.0 / 22.0 | Welding current (A) / arc voltage (V) |
| `gas_flow_rate` | 15.0 | Shielding gas flow (L/min) |
| `material` | `HSBA_WAAM_MATERIAL_STEEL` | Material: STEEL / ALUMINUM / TITANIUM / COPPER / UNKNOWN |
| `welding_process` | `HSBA_WAAM_WELD_ARC` | Process: ARC / LASER / UNKNOWN |
| `protection` / `protect_gas` | `SHIELD_GAS` / `ARGON` | Protection method (shield-gas / vacuum) / shielding gas |
| `interpass_temperature` | 100.0 | Interpass temperature (°C) |
| `robot_type` | `HSBA_WAAM_ROBOT_ABB` | Robot: ABB / KUKA / FANUC / UNKNOWN (requires a Lua path script) |
| `path_lua_script` / `path_lua_func` | NULL | Optional custom robot-code generator script and function name (built-in when NULL, default `export_waam`) |
| `spiral_mode` | 0 | Deposit the outer wall as one continuous, Z-rising bead (0=false, 1=true) |
| `output_path` | NULL | Output robot-program path |

> The WAAM result struct reports its output through the `output_path` field (the robot-program path), rather than the `export_path` used by the other pipelines.

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

### Custom Lua Pipeline

Unlike FDM/SLA/SLS (whose stage order is fixed in C++ with optional per-stage Lua customization), the Custom pipeline delegates the **entire workflow to a Lua script**: the C++ side only builds the Lua environment, exposes every pipeline building block through the global `HsBa` table and calls the script's entry function. New processes can be added by editing a script, without rebuilding the library.

```c
HsBaCustomPipelineConfig_t HsBaCreateDefaultCustomConfig(void);

HsBaCustomPipelineResult_t HsBaRunCustomPipeline(const HsBaCustomPipelineConfig_t* config,
                                                 HsBaCustomProgressCallback callback, void* user_data);

void HsBaRunCustomPipelineAsync(const HsBaCustomPipelineConfig_t* config,
                                HsBaCustomProgressCallback callback, void* user_data,
                                HsBaCustomResultCallback result_callback, void* result_user_data);

void HsBaFreeCustomPipelineResult(HsBaCustomPipelineResult_t* result);
```

#### Configuration Fields

| Field | Default | Description |
| --- | --- | --- |
| `pipeline_lua_script` | NULL | Path to the pipeline Lua script |
| `pipeline_lua_source` | NULL | Inline Lua source, executed **before** the script file (parameter prelude) |
| `entry_func` | NULL | Entry function name, `run_pipeline` when NULL |
| `config_json` | NULL | Free-form JSON string, readable in Lua as `pipeline_config` |
| `model_name` / `model_path` | NULL | Model name / file path, readable as `model_name` / `model_path` |
| `output_path` | NULL | Default output path, readable as `output_path` |

> At least one of `pipeline_lua_script` / `pipeline_lua_source` must be set. All fields above can also be delivered as Proto bytes, see [Proto Serialization Conversion](#proto-serialization-conversion).

#### Script Environment

Injected globals: `HsBa` (operations table), `model_name`, `model_path`, `output_path`, `pipeline_config`, `pipeline_entry`. The pooled libraries (`PolygonOperations`, `Support`, `PolygonFill`, `PathOptimize`, `Zipper`, `Cipher`, `SQLiteAdapter`, ...) are available as well.

`HsBa` operations (coordinates in mm):

| Group | Operations |
| --- | --- |
| Reporting | `progress(pct[, stage])`, `setLayers(n)`, `setOutputPath(path)` |
| Files | `readFile(path)`, `writeFile(path, content)` |
| Model | `loadModel(n, path)`, `modelInfo(n)`, `translateModel`, `rotateModel`, `scaleModel`, `removeModel`, `modelNames` |
| Slicing | `layerCount(n, lh, flh)`, `layerZ(i, lh, flh)`, `slice(n, z)`, `sliceUnsafe(n, z)`, `toInt`, `toDouble` |
| Process | `fill(polys[, cfg])`, `fdmSupport(layers, cfg)`, `slaSupport(layers, cfg)`, `floor(bottom, cfg)` |
| Output | `toGcode(layers, cfg)`, `saveSlaPackage(tbl)`, `saveSlsPackage(tbl)`, `renderImage(polys, w, h, path)` |

Any truthy return value of the entry function means success (a string return is reported through `result_string`); returning `false`/`nil` or raising a Lua error means failure. `total_layers` and `output_path` are reported by the script via `HsBa.setLayers()` / `HsBa.setOutputPath()`.

#### Calling Via Protobuf

For cross-process / cross-language use there is no need to marshal every string field at the boundary: serialize the request as `custom_pipe_config` wire bytes, rebuild the C config struct on the receiving side with the C interface, run it as usual, and return the result as `custom_pipe_result` bytes.

```c
#include "pipeline_convert.h"

// 1. Receive custom_pipe_config bytes from the peer
HsBaCustomPipelineConfig_t cfg = HsBaCustomConfigDefault();
if (!HsBaCustomConfigFromProtoBytes(buf, size, &cfg)) { /* parse failure */ }

// 2. Identical to the plain call with directly assigned fields
HsBaCustomPipelineResult_t r = HsBaRunCustomPipeline(&cfg, OnProgress, NULL);

// 3. Release the deserialized strings and send the result back
HsBaFreeCustomConfigStrings(&cfg);
void* out_buf = NULL; int out_size = 0;
HsBaCustomResultToProtoBytes(&r, &out_buf, &out_size);  /* send out_buf[0, out_size) */
free(out_buf);
HsBaFreeCustomPipelineResult(&r);
```

The pipeline definition itself (stage order, operation combination) stays inside the Lua script — Proto only carries the script path / inline source plus the model and output inputs, which is why `custom_pipe_config` has far fewer fields than the FDM/SLA/SLS messages. The `pipeline_lua_source` field can ship a whole workflow, enabling file-free deployments.

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

// SLM
int HsBaSlmConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaSlmPipelineConfig_t* config);
int HsBaSlmConfigToProtoBytes(const HsBaSlmPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaSlmResultFromProtoBytes(const void* proto_data, int proto_size, HsBaSlmPipelineResult_t* result);
int HsBaSlmResultToProtoBytes(const HsBaSlmPipelineResult_t* result, void** out_data, int* out_size);

// LOM
int HsBaLomConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaLomPipelineConfig_t* config);
int HsBaLomConfigToProtoBytes(const HsBaLomPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaLomResultFromProtoBytes(const void* proto_data, int proto_size, HsBaLomPipelineResult_t* result);
int HsBaLomResultToProtoBytes(const HsBaLomPipelineResult_t* result, void** out_data, int* out_size);

// 3DP
int HsBaTdpConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaTdpPipelineConfig_t* config);
int HsBaTdpConfigToProtoBytes(const HsBaTdpPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaTdpResultFromProtoBytes(const void* proto_data, int proto_size, HsBaTdpPipelineResult_t* result);
int HsBaTdpResultToProtoBytes(const HsBaTdpPipelineResult_t* result, void** out_data, int* out_size);

// WAAM
int HsBaWaamConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaWaamPipelineConfig_t* config);
int HsBaWaamConfigToProtoBytes(const HsBaWaamPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaWaamResultFromProtoBytes(const void* proto_data, int proto_size, HsBaWaamPipelineResult_t* result);
int HsBaWaamResultToProtoBytes(const HsBaWaamPipelineResult_t* result, void** out_data, int* out_size);

// File Transfer
int HsBaFileTransferConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineConfig_t* config);
int HsBaFileTransferConfigToProtoBytes(const HsBaFileTransferPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaFileTransferResultFromProtoBytes(const void* proto_data, int proto_size, HsBaFileTransferPipelineResult_t* result);
int HsBaFileTransferResultToProtoBytes(const HsBaFileTransferPipelineResult_t* result, void** out_data, int* out_size);

// Custom Lua pipeline
int HsBaCustomConfigFromProtoBytes(const void* proto_data, int proto_size, HsBaCustomPipelineConfig_t* config);
int HsBaCustomConfigToProtoBytes(const HsBaCustomPipelineConfig_t* config, void** out_data, int* out_size);
int HsBaCustomResultFromProtoBytes(const void* proto_data, int proto_size, HsBaCustomPipelineResult_t* result);
int HsBaCustomResultToProtoBytes(const HsBaCustomPipelineResult_t* result, void** out_data, int* out_size);

// Memory cleanup
void HsBaFreeFdmConfigStrings(HsBaFdmPipelineConfig_t* config);
void HsBaFreeSlaConfigStrings(HsBaSlaPipelineConfig_t* config);
void HsBaFreeSlsConfigStrings(HsBaSlsPipelineConfig_t* config);
void HsBaFreeSlmConfigStrings(HsBaSlmPipelineConfig_t* config);
void HsBaFreeLomConfigStrings(HsBaLomPipelineConfig_t* config);
void HsBaFreeTdpConfigStrings(HsBaTdpPipelineConfig_t* config);
void HsBaFreeWaamConfigStrings(HsBaWaamPipelineConfig_t* config);
void HsBaFreeFileTransferConfigStrings(HsBaFileTransferPipelineConfig_t* config);
void HsBaFreeCustomConfigStrings(HsBaCustomPipelineConfig_t* config);
```

> Proto message definitions are in the `proto/` directory (`fdm_pipeline.proto`, `sla_pipeline.proto`, `sls_pipeline.proto`, `slm_pipeline.proto`, `lom_pipeline.proto`, `tdp_pipeline.proto`, `waam_pipeline.proto`, `file_transfer_pipeline.proto`, `custom_pipeline.proto`), with multi-language output support for C++/C#/Java/Python/PHP.
>
> The strings produced by `HsBaCustomResultFromProtoBytes` are `malloc`'d as well; release them with `HsBaFreeCustomPipelineResult()` (Custom has no separate ResultStrings helper).
>
> **Note for C++ callers**: `DllHsBaSlicer` already links a copy of `HsBaSlicerProto`. Do not link the generated `.pb.cc` into the same process as well, otherwise protobuf aborts at startup with a duplicate descriptor registration (`File already exists in database`). Pure C++ integrations should use the C structs directly, or go through the `LibHsBaSlicer` / `ModuleHsBaSlicer` layers; cross-language callers (C# / Python / Java, each with its own protobuf runtime) are unaffected, and example 4 of `samples/Custom/` shows the bytes-only approach by hand-encoding the wire format.

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

### C++ Event Source Registration

Register C-style event callbacks for monitoring Zipper compression progress and database operation events (non-Lua, pure C callbacks):

```c
// Zipper event callback (progress percentage + stage description)
void HsBaAddZipperEventCallback(const char* event_name, void (*func)(double, const char*));

// Database event callback (key + value)
void HsBaAddDBEventCallback(const char* event_name, void (*func)(const char*, const char*));
```

Example:

```c
#include "initialize.h"
#include "event_source_register.h"

static void on_zip_progress(double percent, const char* stage) {
    // handle compression progress
}

static void on_db_event(const char* key, const char* value) {
    // handle database events
}

int main(void) {
    initialize();
    HsBaAddZipperEventCallback("zipper.on_progress", on_zip_progress);
    HsBaAddDBEventCallback("db.on_query", on_db_event);
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
2. `gcode_content` / `export_path` (`output_path` for WAAM) / `error_message` inside result structs are allocated by the library and **must** be released with the matching `HsBaFree*PipelineResult()`;
3. Version strings must be freed with `HsBaFreeVersionString()`;
4. Model handles (`void*` returned by `HsBaLoadModel` / `HsBaGetModel` / `HsBaBoolean*` / `HsBaThickSolidModel`) must be released with `HsBaReleaseModelHandle()`;
5. `pipeline_types.h` also provides DLL-independent inline initializers `HsBaFdmConfigDefault()` / `HsBaSlaConfigDefault()` / `HsBaSlsConfigDefault()` / `HsBaSlmConfigDefault()` / `HsBaLomConfigDefault()` / `HsBaTdpConfigDefault()` / `HsBaWaamConfigDefault()` / `HsBaFileTransferConfigDefault()`, handy for header-only scenarios (e.g. mirroring structs for P/Invoke);
6. Proto deserialization (`*FromProtoBytes`) allocates string fields with `malloc`—release them with the matching `HsBaFree*ConfigStrings()` (for Custom results use `HsBaFreeCustomPipelineResult()`); `*ToProtoBytes` output buffers (`out_data`) must be `free`'d by the caller.

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
- `samples/SLM/` — SLM metal powder-bed pipeline with Lua export example (basic / custom metal params / async)
- `samples/LOM/` — LOM laminated-object pipeline with Lua export example
- `samples/TDP/` — 3DP binder-jetting pipeline with Lua export example
- `samples/WAAM/` — WAAM arc-additive robot path export example
- `samples/Custom/` — Fully Lua-script-defined pipeline example (FDM / SLA / inline script / async / Protobuf bytes)
- `android/` — Android JNI sample project
- `ios/HsBaSlicerExample/` — iOS Swift bridging sample
