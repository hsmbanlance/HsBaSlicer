# Integration Guide

<cite>
**Referenced Files in This Document**   
- [README.md](file://README.md)
- [CMakeLists.txt](file://CMakeLists.txt)
- [vcpkg.json](file://vcpkg.json)
- [LICENSE.txt](file://LICENSE.txt)
- [pipeline_types.h](file://pipelinetypes/pipeline_types.h)
- [dllexport.h](file://DllHsBaSlicer/dllexport.h)
- [initialize.h](file://DllHsBaSlicer/initialize.h)
- [progress_stage.h](file://DllHsBaSlicer/progress_stage.h)
- [geometry_export.h](file://DllHsBaSlicer/geometry_export.h)
- [job_control.h](file://DllHsBaSlicer/job_control.h)
- [gui_support_api.md](file://docs/en/DllHsBaSlicer/gui_support_api.md)
- [game_engine_integration.md](file://docs/en/DllHsBaSlicer/game_engine_integration.md)
- [qt_wxwidgets_integration.md](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md)
- [android README.md](file://android/README.md)
- [ViewController.swift](file://ios/HsBaSlicerExample/ViewController.swift)
- [HsBaSlicerBridge.h](file://ios/HsBaSlicerExample/HsBaSlicerBridge.h)
- [model.js](file://webui/public/model.js)
- [view.js](file://webui/public/view.js)
- [webui README.md](file://webui/README.md)
</cite>

## Update Summary
**Changes Made**   
- Added comprehensive GUI Support API documentation covering structured progress stages, layer geometry export, and job cancellation
- Updated Unity and Unreal Engine integration sections with new tail field requirements for struct compatibility
- Enhanced Qt/wxWidgets integration with job handle patterns and cancellation support
- Added Web UI MVC architecture section documenting the complete frontend separation
- Updated performance considerations with memory management guidance for geometry snapshots
- Expanded troubleshooting guide with GUI-specific issues

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [GUI Support API](#gui-support-api)
5. [Architecture Overview](#architecture-overview)
6. [Detailed Component Analysis](#detailed-component-analysis)
7. [Dependency Analysis](#dependency-analysis)
8. [Performance Considerations](#performance-considerations)
9. [Troubleshooting Guide](#troubleshooting-guide)
10. [Security, Licensing, and Distribution](#security-licensing-and-distribution)
11. [Conclusion](#conclusion)

## Introduction
This integration guide explains how to embed HsBaSlicer into different platforms and frameworks: Unity, Unreal Engine, Qt, wxWidgets, Android, and iOS. It focuses on the C ABI exposed by `DllHsBaSlicer`, the standalone type definitions in `pipelinetypes/pipeline_types.h`, and the platform-specific build and deployment patterns documented in this repository.

HsBaSlicer is a modular, cross-platform C++ slicing framework. The recommended integration surface for external applications is the shared library `DllHsBaSlicer`, which exposes C-compatible pipeline functions for FDM, SLA, SLS, SLM, LOM, TDP, WAAM, file transfer, and custom Lua pipelines. For pure C++ consumers, `LibHsBaSlicer` provides higher-level slicing capabilities.

**Updated** The latest version includes comprehensive GUI support APIs providing structured progress stages, per-layer geometry snapshots, and non-blocking job handles with cooperative cancellation.

**Section sources**
- [README.md:21-39](file://README.md#L21-L39)
- [README.md:100-124](file://README.md#L100-L124)

## Project Structure
For integration purposes, the most relevant parts are:

| Area | Purpose |
|---|---|
| `DllHsBaSlicer` | Shared library with C ABI entry points such as `initialize()` and pipeline run functions. |
| `pipelinetypes/pipeline_types.h` | Standalone C-compatible struct and enum definitions used by all integrations. |
| `LibHsBaSlicer` | C++ API for full slicing workflows when you do not need a separate DLL. |
| `CMakeLists.txt` | Top-level build configuration, install targets, and platform feature toggles. |
| `vcpkg.json` | Dependency manifest, including optional copyleft kernels that affect licensing. |
| `docs/en/DllHsBaSlicer/*` | Official integration examples for Unity, Unreal Engine, Qt, and wxWidgets. |
| `android/README.md` | Minimal Android packaging instructions for native `.so` artifacts. |
| `ios/HsBaSlicerExample/*` | Swift example showing bridging to a C/C++ static-library entry point. |
| `webui/` | MVC-patterned web interface demonstrating modern GUI architecture patterns. |

```mermaid
graph TB
App["Your Application<br/>Unity / UE / Qt / wxWidgets / Android / iOS"] --> Dll["DllHsBaSlicer<br/>C ABI"]
Dll --> Types["pipeline_types.h<br/>Config & Result Structs"]
Dll --> Init["initialize()<br/>One-time setup"]
Dll --> Pipelines["FDM / SLA / SLS / SLM / LOM / TDP / WAAM / Custom"]
Dll --> GUIAPI["GUI Support API<br/>Progress / Geometry / Jobs"]
Lib["LibHsBaSlicer<br/>C++ API"] --> Pipelines
WebUI["Web UI (MVC)<br/>Model/View/Controller"] --> REST["REST API"]
```

**Diagram sources**
- [initialize.h:14-19](file://DllHsBaSlicer/initialize.h#L14-L19)
- [pipeline_types.h:53-127](file://pipelinetypes/pipeline_types.h#L53-L127)
- [progress_stage.h:23-42](file://DllHsBaSlicer/progress_stage.h#L23-L42)
- [geometry_export.h:37-143](file://DllHsBaSlicer/geometry_export.h#L37-L143)
- [job_control.h:34-146](file://DllHsBaSlicer/job_control.h#L34-L146)
- [README.md:100-124](file://README.md#L100-L124)

**Section sources**
- [README.md:100-150](file://README.md#L100-L150)
- [CMakeLists.txt:456-519](file://CMakeLists.txt#L456-L519)

## Core Components
The integration surface consists of three layers:

1. **Type layer**: `pipeline_types.h` defines all pipeline configurations and results. These structs are intentionally free of DLL export macros so downstream code can include them without linking the shared library.
2. **ABI layer**: `DllHsBaSlicer` exports C functions using `HSBA_SLICER_API`. On Windows, this resolves to `__declspec(dllexport)` or `__declspec(dllimport)`; on other platforms it is empty.
3. **Runtime layer**: Call `initialize()` once at process startup, then call the appropriate pipeline function with a configured struct and optional progress/result callbacks.

Key ABI concepts:

| Concept | Meaning |
|---|---|
| Config structs | Passed by value or pointer depending on the wrapper; string fields must remain valid during the pipeline call. |
| Result structs | Contain success flags, output paths or G-code content, error messages, and elapsed time. |
| Callbacks | Progress and result callbacks fire on library threads; marshal UI updates back to your application's main thread. |
| Memory ownership | Caller frees result strings through the matching `HsBaFree*PipelineResult` function. |

**Updated** All config structs now include GUI support tail fields (`capture_geometry`, `cancel_flag`) that must be properly initialized to avoid undefined behavior.

**Section sources**
- [pipeline_types.h:5-12](file://pipelinetypes/pipeline_types.h#L5-L12)
- [pipeline_types.h:53-127](file://pipelinetypes/pipeline_types.h#L53-L127)
- [dllexport.h:9-20](file://DllHsBaSlicer/dllexport.h#L9-L20)
- [initialize.h:14-19](file://DllHsBaSlicer/initialize.h#L14-L19)

## GUI Support API

**New Section** The GUI Support API provides three complementary capabilities for graphical hosts: structured progress stages, per-layer geometry snapshots, and non-blocking job handles with cooperative cancellation.

### Structured Progress Stages

The `HsBaStage_t` enumeration provides stable identifiers for pipeline progress, replacing reliance on human-readable text that may change between releases.

```c
typedef enum HsBaStage {
    HSBA_STAGE_UNKNOWN = 0,           // Unrecognised stage text
    HSBA_STAGE_LOAD_MODEL = 1,        // Model loading started
    HSBA_STAGE_MODEL_LOADED = 2,      // Model loaded, geometry validated
    HSBA_STAGE_SLICING = 3,           // Layer slicing started
    HSBA_STAGE_SLICING_LAYER = 4,     // Layer slicing in progress
    HSBA_STAGE_SLICE_DONE = 5,        // Layer slicing finished
    HSBA_STAGE_FLOOR = 6,             // Floor / raft generation started (SLA)
    HSBA_STAGE_FLOOR_DONE = 7,        // Floor / raft generation finished (SLA)
    HSBA_STAGE_SUPPORT = 8,           // Support generation started
    HSBA_STAGE_SUPPORT_DONE = 9,      // Support generation finished
    HSBA_STAGE_SUPPORT_DISABLED = 10, // Support stage skipped
    HSBA_STAGE_FILL = 11,             // Infill started
    HSBA_STAGE_FILLING_LAYER = 12,    // Infill in progress
    HSBA_STAGE_FILL_DONE = 13,        // Infill finished
    HSBA_STAGE_PATH = 14,             // Tool-path / G-code generation started
    HSBA_STAGE_WRITE_FILE = 15,       // Writing the output artefact
    HSBA_STAGE_EXPORT = 16,           // Packaging / Lua export in progress
    HSBA_STAGE_DONE = 17,             // Pipeline finished
    HSBA_STAGE_CANCELLED = 18         // Pipeline stopped because cancellation was requested
} HsBaStage_t;
```

Classification functions map progress text to stable enums:

| Function | Purpose |
|---|---|
| `HsBaStageFromText(const char* stage)` | Classify progress text into stable enum |
| `HsBaStageName(HsBaStage_t stage)` | Get snake_case identifier (e.g., "slicing.layer") |
| `HsBaStageLabel(HsBaStage_t stage)` | Get English display label |

### Layer Geometry Snapshots

When `cfg.capture_geometry = 1`, the pipeline stores per-layer geometry in a process-wide snapshot pool, accessible via geometry IDs.

```c
typedef struct HsBaRing {
    int point_count;    // Number of stored vertices
    const double* xy;   // Flattened interleaved coordinates, length 2 * point_count
    int closed;         // 1 = contour repeats first vertex, 0 = open polyline
} HsBaRing_t;

typedef struct HsBaLayerGeometry {
    int layer_index;            // Zero-based layer index
    double z_height;            // Platform height (mm)
    int outline_count;          // Outer / inner part contours
    const HsBaRing_t* outlines; // outline_count rings
    int fill_count;             // Infill lines / SLA floor rings
    const HsBaRing_t* fills;    // fill_count rings
    int support_count;          // Support cross-section rings
    const HsBaRing_t* supports; // support_count rings
    double bounds_min[2];       // [min_x, min_y] over every ring
    double bounds_max[2];       // [max_x, max_y] over every ring
} HsBaLayerGeometry_t;
```

For spiral/vase runs, continuous 3D toolpaths are also captured:

```c
typedef struct HsBaGeometryPath {
    int point_count;      // Number of xyz triples; 0 when none
    const double* xyz;    // Flattened x0,y0,z0,x1,y1,z1,...
    double bounds_min[2]; // min_x, min_y over the polyline
    double bounds_max[2]; // max_x, max_y over the polyline
    double z_min;         // Lowest point (mm)
    double z_max;         // Highest point (mm)
} HsBaGeometryPath_t;
```

### Job Handles and Cancellation

`HsBaSubmitJob` provides genuine non-blocking execution with cooperative cancellation:

```c
typedef void (*HsBaProgressCallbackEx)(int percent, HsBaStage_t stage, const char* stage_text, void* user_data);
typedef void (*HsBaJobDoneCallback)(HsBaJobHandle job, void* user_data);

HsBaJobHandle HsBaSubmitJob(HsBaPipelineKind kind, const void* config,
                           HsBaProgressCallbackEx on_progress, void* progress_user_data,
                           HsBaJobDoneCallback on_done, void* done_user_data);
int HsBaCancelJob(HsBaJobHandle job);
int HsBaWaitJob(HsBaJobHandle job, long long timeout_ms);
HsBaJobState_t HsBaJobGetState(HsBaJobHandle job);
```

Job states include `PENDING`, `RUNNING`, `SUCCEEDED`, `FAILED`, and `CANCELLED`.

**Section sources**
- [progress_stage.h:23-42](file://DllHsBaSlicer/progress_stage.h#L23-L42)
- [geometry_export.h:37-143](file://DllHsBaSlicer/geometry_export.h#L37-L143)
- [job_control.h:34-146](file://DllHsBaSlicer/job_control.h#L34-L146)
- [gui_support_api.md:40-324](file://docs/en/DllHsBaSlicer/gui_support_api.md#L40-L324)

## Architecture Overview
At runtime, an integrated application follows this flow:

```mermaid
sequenceDiagram
participant App as "Application"
participant ABI as "DllHsBaSlicer C ABI"
participant Types as "pipeline_types.h"
participant Pipeline as "Slicing Pipeline"
participant UI as "UI Thread"
participant GUIAPI as "GUI Support API"
App->>ABI : initialize()
App->>Types : Create default config struct
Note over App,GUIAPI : Set capture_geometry=1, cancel_flag if needed
App->>ABI : Run pipeline (sync or async)
ABI-->>App : ProgressCallback(percent, stage)
GUIAPI-->>App : Structured progress with stage enumeration
App->>UI : Marshal progress update
ABI-->>App : ResultCallback(result)
GUIAPI-->>App : Job handle with geometry_id
App->>Types : Copy output strings
App->>GUIAPI : Query layer geometry if available
App->>ABI : Free pipeline result
App->>UI : Show completion state
```

**Diagram sources**
- [initialize.h:14-19](file://DllHsBaSlicer/initialize.h#L14-L19)
- [pipeline_types.h:130-140](file://pipelinetypes/pipeline_types.h#L130-L140)
- [progress_stage.h:23-42](file://DllHsBaSlicer/progress_stage.h#L23-L42)
- [geometry_export.h:84-143](file://DllHsBaSlicer/geometry_export.h#L84-L143)
- [job_control.h:74-146](file://DllHsBaSlicer/job_control.h#L74-L146)

## Detailed Component Analysis

### Unity Integration
Unity integrates through C# P/Invoke against `DllHsBaSlicer`. The official guide shows:

- Platform-specific library names, including `"__Internal"` for iOS static libraries.
- C# struct declarations that mirror `pipeline_types.h` field order and types.
- A high-level service that keeps delegate references alive and marshals callbacks to the Unity main thread.
- IL2CPP considerations for by-value struct passing and sequential layout.

**Updated** Unity developers must account for the new tail fields in all config structs:

```csharp
// Required tail fields for all pipeline configs
public int capture_geometry;      // Enable geometry snapshots
public IntPtr cancel_flag;        // Pointer to std::atomic<bool>
```

```mermaid
flowchart TD
Start["Unity MonoBehaviour calls SliceFdmAsync"] --> CreateCfg["Create FdmPipelineConfig via P/Invoke"]
CreateCfg --> SetStrings["Set UTF-8 model_name and model_path"]
SetStrings --> SetGUIFields["Set capture_geometry=1, cancel_flag if needed"]
SetGUIFields --> KeepDelegates["Keep ProgressCallback and ResultCallback alive"]
KeepDelegates --> RunAsync["Call HsBaRunFdmPipelineAsync"]
RunAsync --> NativeProgress["Native ProgressCallback fires"]
NativeProgress --> MainThread["Post to Unity main thread"]
MainThread --> UpdateUI["Update progress bar"]
RunAsync --> NativeResult["Native ResultCallback fires"]
NativeResult --> FreeResult["Copy strings and call HsBaFreePipelineResult"]
FreeResult --> Done["Return Task result on main thread"]
```

**Diagram sources**
- [game_engine_integration.md:21-123](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L21-L123)
- [game_engine_integration.md:128-183](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L128-L183)
- [game_engine_integration.md:206-213](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L206-L213)

Practical guidance from the repository:

- Place per-ABI native libraries under the correct Unity plugin folders.
- Keep delegates stored in static or member fields so the garbage collector does not collect them while native code holds them.
- Never touch `UnityEngine.Object` directly inside native callbacks; marshal to the main thread first.
- For IL2CPP, match struct layouts exactly and use `LayoutKind.Sequential`.
- Initialize all tail fields explicitly to avoid reading uninitialized memory.

**Section sources**
- [game_engine_integration.md:7-18](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L7-L18)
- [game_engine_integration.md:21-123](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L21-L123)
- [game_engine_integration.md:128-213](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L128-L213)

### Unreal Engine Integration
Unreal Engine integrates through a C++ plugin that wraps `DllHsBaSlicer`:

- A `ThirdParty` module declares headers, import libraries, and runtime dependencies.
- A `GameInstanceSubsystem` exposes Blueprint-callable functions.
- Background tasks execute slicing off the GameThread.
- Progress and completion events are broadcast on the GameThread.

**Updated** The new GUI support API enables sophisticated progress tracking and cancellation:

```cpp
// Configure job with geometry capture and cancellation
HsBaFdmPipelineConfig_t config = HsBaFdmPipelineConfigDefault();
config.capture_geometry = 1;
std::atomic<bool> cancelFlag{false};
config.cancel_flag = &cancelFlag;

// Submit job and poll for completion
HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &config, 
    [](int percent, HsBaStage_t stage, const char* stage_text, void* ud) {
        // Structured progress with stable stage enumeration
        FString stageName = UTF8_TO_TCHAR(HsBaStageName(stage));
        // Broadcast to UI thread
        AsyncTask(ENamedThreads::GameThread, [stageName]() {
            OnSliceProgress.Broadcast(percent, stageName);
        });
    }, nullptr, nullptr, nullptr);
```

```mermaid
sequenceDiagram
participant BP as "Blueprint"
participant Subsystem as "UHsBaSlicerSubsystem"
participant Native as "DllHsBaSlicer"
participant GUIAPI as "GUI Support API"
participant GameThread as "UE GameThread"
BP->>Subsystem : SliceFdmAsync(ModelPath)
Subsystem->>Subsystem : Convert FString to UTF-8 bytes
Subsystem->>Native : HsBaSubmitJob(config, progress_cb)
Native-->>Subsystem : ProgressCallback with HsBaStage_t
Subsystem->>GameThread : AsyncTask(GameThread, Broadcast OnSliceProgress)
Native-->>Subsystem : JobDoneCallback
Subsystem->>GameThread : AsyncTask(GameThread, Broadcast OnSliceFinished)
```

**Diagram sources**
- [game_engine_integration.md:218-269](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L218-L269)
- [game_engine_integration.md:272-349](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L272-L349)
- [game_engine_integration.md:357-362](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L357-L362)
- [job_control.h:74-146](file://DllHsBaSlicer/job_control.h#L74-L146)

Important rules:

- Convert `FString` to UTF-8 before crossing the C boundary.
- Keep UTF-8 byte buffers alive for the entire slicing run.
- Always wrap delegate broadcasts in `AsyncTask(ENamedThreads::GameThread, ...)`.
- Package the DLL through `RuntimeDependencies`; delay loading requires proper DLL directory setup.
- Use `HsBaCancelJob` for cooperative cancellation rather than thread termination.

**Section sources**
- [game_engine_integration.md:218-362](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L218-L362)

### Qt Integration
Qt integration uses `DllHsBaSlicer` from a C++ desktop application. The guide recommends:

- Using `find_package(HsBaSlicer CONFIG REQUIRED)` or pointing directly at build artifacts.
- Running slicing on a worker thread, not the UI thread.
- Emitting Qt signals from the worker so slots automatically run on the UI thread.
- Keeping UTF-8 path strings alive while the pipeline runs.

**Updated** Qt applications can leverage the new job handle API for better UX:

```cpp
class SlicerWorker : public QObject {
    Q_OBJECT
public:
    explicit SlicerWorker(QObject* parent = nullptr) : QObject(parent) {}
    
    void startSlice(const HsBaFdmPipelineConfig_t& config) {
        // Enable geometry capture for preview
        HsBaFdmPipelineConfig_t cfg = config;
        cfg.capture_geometry = 1;
        
        job_ = HsBaSubmitJob(HSBA_PIPELINE_FDM, &cfg,
            [this](int percent, HsBaStage_t stage, const char* stage_text, void*) {
                emit progressChanged(percent, HsBaStageName(stage), stage_text);
            }, nullptr,
            [this](HsBaJobHandle job, void*) {
                HsBaJobResultView_t view = {};
                if (HsBaJobCopyResult(job, &view)) {
                    emit sliceCompleted(view.success, view.output_path, view.error_message);
                    HsBaFreeJobResultView(&view);
                }
                HsBaReleaseJob(job);
            }, nullptr);
    }
    
    void cancel() {
        if (job_) HsBaCancelJob(job_);
    }
    
private:
    HsBaJobHandle job_;
signals:
    void progressChanged(int percent, const char* stageId, const char* stageText);
    void sliceCompleted(bool success, const char* outputPath, const char* errorMessage);
};
```

```mermaid
flowchart TD
Start["User clicks Start Slice"] --> Init["Call initialize() once"]
Init --> Worker["Create QThread + SlicerWorker"]
Worker --> BuildCfg["Build HsBaFdmPipelineConfig_t with capture_geometry=1"]
BuildCfg --> RunSync["Call HsBaSubmitJob on worker thread"]
RunSync --> Progress["onProgress emits progressChanged"]
Progress --> UI["QProgressBar updated on UI thread"]
RunSync --> Finish["Emit sliceCompleted(success, gcode, error)"]
Finish --> Save["Write G-code to file"]
```

**Diagram sources**
- [qt_wxwidgets_integration.md:16-45](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L16-L45)
- [qt_wxwidgets_integration.md:47-106](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L47-L106)
- [qt_wxwidgets_integration.md:108-151](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L108-L151)
- [job_control.h:74-146](file://DllHsBaSlicer/job_control.h#L74-L146)

**Section sources**
- [qt_wxwidgets_integration.md:7-13](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L7-L13)
- [qt_wxwidgets_integration.md:16-167](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L16-L167)

### wxWidgets Integration
wxWidgets integration follows the same threading principles as Qt:

- Use a background thread for the blocking pipeline call.
- Use `CallAfter` to marshal progress and completion updates to the UI thread.
- Store UTF-8 model paths in the panel object so they outlive the callback.
- Prefer a managed thread pool over detached threads in production.

**Updated** wxWidgets applications benefit from the structured progress API and cancellation support:

```cpp
void MyFrame::OnStartSlice(wxCommandEvent&) {
    HsBaFdmPipelineConfig_t config = HsBaFdmPipelineConfigDefault();
    config.model_path = modelPath_.ToUTF8().c_str();
    config.capture_geometry = 1;
    
    std::atomic<bool> cancelFlag{false};
    config.cancel_flag = &cancelFlag;
    
    HsBaJobHandle job = HsBaSubmitJob(HSBA_PIPELINE_FDM, &config,
        [this, &cancelFlag](int percent, HsBaStage_t stage, const char* stage_text, void*) {
            CallAfter([percent, stage, stage_text, &cancelFlag]() {
                progressBar_->SetValue(percent);
                statusText_->SetStatusText(std::string(HsBaStageLabel(stage)));
                
                // Check if user cancelled
                if (cancelFlag.load()) {
                    HsBaCancelJob(job_);
                }
            });
        }, nullptr, nullptr, nullptr);
    
    job_ = job;
}
```

```mermaid
flowchart TD
Click["User starts slice"] --> StorePath["Store UTF-8 model path"]
StorePath --> Thread["Start background thread"]
Thread --> Run["Call HsBaSubmitJob with cancel_flag"]
Run --> Callback["OnProgress callback with structured stage"]
Callback --> CallAfter["CallAfter to UI thread"]
CallAfter --> Gauge["Update gauge and status text"]
Run --> Done["Copy result strings and free result"]
Done --> CallAfterDone["CallAfter to finish UI update"]
```

**Diagram sources**
- [qt_wxwidgets_integration.md:171-265](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L171-L265)
- [job_control.h:74-146](file://DllHsBaSlicer/job_control.h#L74-L146)

**Section sources**
- [qt_wxwidgets_integration.md:171-265](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L171-L265)

### Android Integration
Android integration packages the native `.so` files produced by the CMake build into an Android app:

- Build native libraries with the Android NDK toolchain and vcpkg.
- Copy the resulting `.so` files into `app/src/main/jniLibs/<abi>/`.
- Open the Android project in Android Studio and build or run the APK.
- Add a Java/Kotlin activity that loads the native library via `System.loadLibrary`.

```mermaid
flowchart TD
CMake["CMake build with Android NDK"] --> So["Produce libDllHsBaSlicer.so"]
So --> JniLibs["Copy to jniLibs/arm64-v8a/"]
JniLibs --> Gradle["Gradle builds APK"]
Gradle --> App["Android app loads native library"]
```

**Diagram sources**
- [android README.md:1-22](file://android/README.md#L1-L22)

**Section sources**
- [android README.md:1-22](file://android/README.md#L1-L22)

### iOS Integration
iOS integration uses a Swift example that bridges to a C/C++ entry point exported from the static library:

- `HsBaSlicerBridge.h` declares `HsBaRunPipelineExamples()` as an `extern "C"` function.
- `ViewController.swift` calls the bridge function on a background queue and updates the UI on the main queue.
- The example is explicitly marked as non-production and intended for testing.

```mermaid
sequenceDiagram
participant Swift as "Swift ViewController"
participant Bridge as "HsBaSlicerBridge.h"
participant StaticLib as "HsBaSlicer Static Library"
participant UI as "Main Thread"
Swift->>StaticLib : HsBaRunPipelineExamples() on background queue
StaticLib-->>Swift : Example completes
Swift->>UI : Dispatch to main thread and update label
```

**Diagram sources**
- [HsBaSlicerBridge.h:14-25](file://ios/HsBaSlicerExample/HsBaSlicerBridge.h#L14-L25)
- [ViewController.swift:43-55](file://ios/HsBaSlicerExample/ViewController.swift#L43-L55)

**Section sources**
- [HsBaSlicerBridge.h:1-32](file://ios/HsBaSlicerExample/HsBaSlicerBridge.h#L1-L32)
- [ViewController.swift:1-86](file://ios/HsBaSlicerExample/ViewController.swift#L1-L86)

### Web UI Architecture (MVC Pattern)

**New Section** The web interface demonstrates modern GUI architecture with clear separation of concerns using the Model-View-Controller pattern.

#### MVC Layers

| Layer | File | Responsibility | Dependencies |
|---|---|---|---|
| **Model** | `public/model.js` | Client-side state management, API communication, data transformation | No DOM access |
| **View** | `public/view.js` | DOM manipulation, SVG rendering, visual presentation | Model only |
| **Controller** | `public/app.js` | Event handling, orchestration, business logic | Model and View |

```mermaid
graph TB
Browser["Browser"] --> HTML["index.html"]
HTML --> Model["model.js<br/>State & API"]
HTML --> View["view.js<br/>DOM & Rendering"]
HTML --> Controller["app.js<br/>Logic & Events"]
Model --> REST["REST API"]
View --> Model
Controller --> Model
Controller --> View
```

**Diagram sources**
- [model.js:1-180](file://webui/public/model.js#L1-L180)
- [view.js:1-630](file://webui/public/view.js#L1-L630)
- [webui README.md:88-104](file://webui/README.md#L88-L104)

#### Key Architectural Features

1. **DOM-free Model**: The Model layer contains no DOM manipulation, making it testable in Node.js environments
2. **Separation of Concerns**: View only renders what Model provides; Controller orchestrates the interaction
3. **Event-driven Design**: User actions flow through Controller to Model, which updates View accordingly
4. **Namespace Isolation**: Each layer publishes its surface on `globalThis.HsBaApp` namespace
5. **Testability**: Pure functions in Model enable unit testing without browser dependencies

**Section sources**
- [model.js:1-180](file://webui/public/model.js#L1-L180)
- [view.js:1-630](file://webui/public/view.js#L1-L630)
- [webui README.md:88-104](file://webui/README.md#L88-L104)

### Pipeline Mapping Across Platforms
All major platforms share the same pipeline mapping pattern:

| Process | Config Type | Default Function | Run Function | Free Function |
|---|---|---|---|---|
| FDM | `HsBaFdmPipelineConfig_t` | `HsBaCreateDefaultConfig` | `HsBaRunFdmPipeline` or async variant | `HsBaFreePipelineResult` |
| SLA | `HsBaSlaPipelineConfig_t` | `HsBaCreateDefaultSlaConfig` | `HsBaRunSlaPipeline` or async variant | `HsBaFreeSlaPipelineResult` |
| SLS | `HsBaSlsPipelineConfig_t` | `HsBaCreateDefaultSlsConfig` | `HsBaRunSlsPipeline` or async variant | `HsBaFreeSlsPipelineResult` |

SLS requires an export Lua script; the result contains an exported file path rather than inline G-code.

**Updated** All config structs now require initialization of GUI support tail fields:

```c
HsBaFdmPipelineConfig_t config = HsBaFdmPipelineConfigDefault();
config.capture_geometry = 1;  // Enable geometry snapshots
config.cancel_flag = NULL;    // Or pointer to std::atomic<bool>
```

**Section sources**
- [game_engine_integration.md:366-377](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L366-L377)
- [qt_wxwidgets_integration.md:268-289](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L268-L289)
- [pipeline_types.h:115-127](file://pipelinetypes/pipeline_types.h#L115-L127)
- [pipeline_types.h:220-232](file://pipelinetypes/pipeline_types.h#L220-L232)
- [pipeline_types.h:285-297](file://pipelinetypes/pipeline_types.h#L285-L297)

## Dependency Analysis
HsBaSlicer uses CMake and vcpkg for dependency management. The top-level CMake configuration:

- Enables position-independent code for static libraries linked into shared libraries.
- Detects desktop vs mobile vs game-console builds.
- Controls optional copyleft kernel usage based on `HSBA_COPL` and vcpkg features.
- Installs targets, headers, and CMake package configuration files.

```mermaid
graph TB
Root["Top-level CMakeLists.txt"] --> Desktop["Desktop Features<br/>OpenCV, OpenVDB, SQL, CAD"]
Root --> Mobile["Mobile Features<br/>Android/iOS reduced set"]
Root --> Console["Game Console Features<br/>HSBA_GAME_CONSOLE"]
Root --> Install["Install Targets<br/>Headers + CMake Config"]
VCPKG["vcpkg.json"] --> Deps["Dependencies<br/>Boost, Clipper2, Lua, OpenSSL, etc."]
VCPKG --> Copyleft["Copyleft Feature<br/>CGAL, OpenCascade"]
```

**Diagram sources**
- [CMakeLists.txt:129-157](file://CMakeLists.txt#L129-L157)
- [CMakeLists.txt:170-204](file://CMakeLists.txt#L170-L204)
- [CMakeLists.txt:270-339](file://CMakeLists.txt#L270-L339)
- [CMakeLists.txt:456-519](file://CMakeLists.txt#L456-L519)
- [vcpkg.json:1-113](file://vcpkg.json#L1-L113)

**Section sources**
- [CMakeLists.txt:129-157](file://CMakeLists.txt#L129-L157)
- [CMakeLists.txt:170-204](file://CMakeLists.txt#L170-L204)
- [CMakeLists.txt:270-339](file://CMakeLists.txt#L270-L339)
- [CMakeLists.txt:456-519](file://CMakeLists.txt#L456-L519)
- [vcpkg.json:1-113](file://vcpkg.json#L1-L113)

## Performance Considerations
Performance-sensitive integrations should follow these patterns:

| Concern | Recommendation |
|---|---|
| Real-time slicing | Use async pipeline APIs where available; avoid blocking the UI or render thread. |
| Threading discipline | Keep native callbacks off UI threads; marshal progress and results back to the application's main thread. |
| String handling | Convert UTF-16 or platform strings to UTF-8 before calling C functions; keep buffers alive for the whole pipeline run. |
| Memory ownership | Copy result strings before calling `HsBaFree*PipelineResult`; never access freed memory. |
| Delegate lifetime | In Unity, keep delegates passed to native code in static or member fields to prevent GC collection. |
| Large models | Configure realistic layer heights, fill densities, and support settings; avoid unnecessary debug features. |
| Logging and heavy kernels | Disable or reduce logging and copyleft-heavy features on constrained platforms. |
| Geometry snapshots | Only enable `capture_geometry` when needed; snapshots consume significant memory proportional to layer count. |
| Job handles | Use `HsBaSubmitJob` for long-running operations; release job handles promptly to prevent resource leaks. |
| Cancellation | Use cooperative cancellation via `HsBaCancelJob` rather than thread termination for clean shutdown. |

**Updated** New performance considerations for GUI Support API:

- **Geometry Pool Management**: The snapshot pool has a default limit of 4 entries; use `HSBA_GEOMETRY_POOL_MAX` environment variable to adjust
- **Memory Efficiency**: Geometry snapshots copy all layer data as doubles; consider releasing snapshots when not actively viewing
- **Continuous Paths**: Spiral mode generates additional 3D path data; only enable for processes that support it (FDM, TDP, WAAM)
- **Structured Progress**: Classification of progress text is efficient but still involves string processing; cache stage mappings when possible

**Section sources**
- [game_engine_integration.md:128-213](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L128-L213)
- [game_engine_integration.md:357-362](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L357-L362)
- [qt_wxwidgets_integration.md:7-13](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L7-L13)
- [qt_wxwidgets_integration.md:47-106](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L47-L106)
- [gui_support_api.md:256-294](file://docs/en/DllHsBaSlicer/gui_support_api.md#L256-L294)

## Troubleshooting Guide
Common integration issues and their causes:

| Symptom | Likely Cause | Fix |
|---|---|---|
| Unity `DllNotFoundException` | Plugin placed in wrong folder or platform disabled in Inspector. | Place per-ABI libraries in the correct Unity plugin path and enable the target platform. |
| Unity crash after callback | Delegate was collected by the garbage collector. | Keep delegate references in static or member fields. |
| UE packaged build cannot find DLL | Missing runtime dependency or incorrect DLL location. | Add `RuntimeDependencies` and ensure the DLL ships with the build. |
| iOS link failure | Trying to load a dylib instead of using the static library. | Use the static `.a` library and `DllImport("__Internal")` in Unity. |
| Struct misalignment or random crashes | C# or C++ struct layout does not match `pipeline_types.h`. | Match field order, types, and sequential layout exactly. |
| Qt/wxWidgets UI freezes | Sync API called on the UI thread. | Move slicing to a worker thread and marshal callbacks to the UI thread. |
| Crash updating widgets in callback | Callback runs on a library thread. | Post UI updates via signals/slots or `CallAfter`. |
| Non-ASCII path failures | Wrong string encoding across the C boundary. | Pass UTF-8 encoded paths. |
| Garbled result strings | Calling free too early. | Copy strings first, then call the matching free function. |
| **New: Geometry snapshots unavailable** | `capture_geometry` was not enabled or snapshot was recycled. | Enable geometry capture and retain snapshots while viewing. |
| **New: Job handle leaks** | Not calling `HsBaReleaseJob` for kept handles. | Release job handles exactly once per reference. |
| **New: Cancellation not working** | `cancel_flag` was NULL or pipeline checkpoint not reached. | Set `cancel_flag` to valid atomic bool address and wait for checkpoint. |
| **New: Progress stages show "unknown"** | Custom Lua pipeline reporting non-standard text. | Use standard stage texts or rely on percentage-only progress. |

**Updated** New troubleshooting scenarios for GUI Support API:

| Symptom | Likely Cause | Fix |
|---|---|---|
| Geometry endpoint returns 404 | Snapshot already recycled by pool limit | Reduce concurrent jobs or increase `HSBA_GEOMETRY_POOL_MAX` |
| Continuous path not available | Not a spiral run or unsupported process | Check `has_path` flag and process type |
| Job appears stuck after cancel | Cooperative cancellation waiting for checkpoint | Wait for pipeline to reach next stage boundary |
| Struct fields read as garbage | Tail fields not initialized | Use `HsBa*ConfigDefault()` or explicitly zero new fields |
| Memory leak with geometry | Not calling `HsBaFreeLayerGeometry` or `HsBaFreeGeometryPath` | Match every get call with corresponding free |

**Section sources**
- [game_engine_integration.md:453-462](file://docs/en/DllHsBaSlicer/game_engine_integration.md#L453-L462)
- [qt_wxwidgets_integration.md:340-349](file://docs/en/DllHsBaSlicer/qt_wxwidgets_integration.md#L340-L349)
- [gui_support_api.md:295-317](file://docs/en/DllHsBaSlicer/gui_support_api.md#L295-L317)

## Security, Licensing, and Distribution
HsBaSlicer uses a conditional dual license:

- Builds that do not include GPL/LGPL components, such as iOS or game-console builds without CGAL or OpenCascade, are licensed under MIT.
- Builds that include copyleft kernels like CGAL or OpenCascade are licensed under GPL-3.0-or-later.
- The effective license is determined at configure time from vcpkg configuration and can be queried at runtime through version information.

Distribution implications:

| Platform | Typical License Outcome | Distribution Notes |
|---|---|---|
| iOS | Usually MIT | Avoid enabling copyleft kernels unless required; bundle static libraries as shown in the iOS example. |
| Game consoles | Usually MIT | The build system detects console triplets and disables some desktop-only features. |
| Desktop Windows/Linux/macOS | Often GPL-3.0-or-later if copyleft kernels are enabled | Ensure source distribution obligations are met if distributing GPL-covered builds. |
| Android | Often GPL-3.0-or-later if copyleft kernels are enabled | Follow Android packaging steps and consider whether CGAL/OpenCascade are needed. |

Dependency and build notes:

- Use `vcpkg.json` to manage dependencies and the optional `copyleft` feature.
- Use `find_package(HsBaSlicer CONFIG REQUIRED)` and link against `HsBaSlicer::DllHsBaSlicer` or other installed targets.
- Install artifacts with `cmake --install` and deploy the required runtime libraries next to your executable or into the platform-specific packaging structure.

**Updated** Security considerations for GUI Support API:

- **Geometry Data Exposure**: Geometry snapshots contain detailed model information; ensure proper access controls when exposing via network APIs
- **Cancellation Security**: Validate cancellation requests come from authorized sources to prevent denial-of-service attacks
- **Memory Safety**: Geometry snapshot pools can be exhausted by malicious clients requesting excessive geometry data
- **Input Validation**: Validate all geometry query parameters to prevent buffer overflows or invalid memory access

**Section sources**
- [LICENSE.txt:1-23](file://LICENSE.txt#L1-L23)
- [README.md:196-210](file://README.md#L196-L210)
- [CMakeLists.txt:170-204](file://CMakeLists.txt#L170-L204)
- [CMakeLists.txt:456-519](file://CMakeLists.txt#L456-L519)
- [vcpkg.json:1-113](file://vcpkg.json#L1-L113)

## Conclusion
HsBaSlicer provides a consistent C ABI for slicing across platforms. For Unity and Unreal Engine, focus on correct P/Invoke or plugin setup, background execution, and main-thread marshalling. For Qt and wxWidgets, use worker threads and UI-safe callback forwarding. For Android and iOS, follow the repository's packaging and bridging examples. 

**Updated** The new GUI Support API significantly enhances integration capabilities with structured progress tracking, per-layer geometry visualization, and robust job management with cooperative cancellation. Modern applications should leverage these APIs for improved user experience and better resource management.

Always respect memory ownership, string encoding, threading discipline, and the conditional dual license determined by your dependency configuration. When integrating the GUI Support API, pay special attention to proper initialization of tail fields, memory management of geometry snapshots, and cooperative cancellation patterns.