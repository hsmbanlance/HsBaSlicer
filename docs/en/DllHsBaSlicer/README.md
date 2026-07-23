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
| `fdm_pipeline.h` | FDM full-pipeline interface |
| `sla_pipeline.h` | SLA full-pipeline interface |
| `sls_pipeline.h` | SLS full-pipeline interface |
| `version_info.h` | Version information (JSON / XML strings) |
| `pipelinetypes/pipeline_types.h` | All config/result structs, enums, callback types and inline default initializers (**no DLL dependency**, can be included standalone) |

## API Overview

### Initialization

```c
void initialize(void);   // call once after process startup
```

### FDM Pipeline

Preprocess -> Slice -> Support -> Fill -> Path Generation, outputs G-code.

```c
HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void);

HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                           HsBaProgressCallback callback, void* user_data);

void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config,
                             HsBaProgressCallback callback, void* user_data,
                             HsBaResultCallback result_callback, void* result_user_data);

void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result);
```

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

### Version Information

```c
char* HsBaGetVersionJson(void);   // free with HsBaFreeVersionString
char* HsBaGetVersionXml(void);
void  HsBaFreeVersionString(char* str);
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
4. `pipeline_types.h` also provides DLL-independent inline initializers `HsBaFdmConfigDefault()` / `HsBaSlaConfigDefault()` / `HsBaSlsConfigDefault()`, handy for header-only scenarios (e.g. mirroring structs for P/Invoke).

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
