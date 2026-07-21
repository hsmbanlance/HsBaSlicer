# Unity / Unreal Engine Integration Guide

This guide shows how to call the DllHsBaSlicer pipelines from **Unity (C# P/Invoke)** and **Unreal Engine (C++ plugin + Blueprint)**.

> Prerequisite: [DllHsBaSlicer Module Overview](./README.md)

## Per-Platform Library Placement

| Platform | Artifact | Unity location | UE location |
| --- | --- | --- | --- |
| Windows x64 | `DllHsBaSlicer.dll` | `Assets/Plugins/x86_64/` | Plugin `ThirdParty/.../Win64/` |
| Linux | `libDllHsBaSlicer.so` | `Assets/Plugins/x86_64/` | `ThirdParty/.../Linux/` |
| macOS | `libDllHsBaSlicer.dylib` | `Assets/Plugins/` | `ThirdParty/.../Mac/` |
| Android | `libDllHsBaSlicer.so` | `Assets/Plugins/Android/libs/<abi>/` | Packaged via engine toolchain |
| iOS | `libDllHsBaSlicer.a` (static) | `Assets/Plugins/iOS/` | `ThirdParty/.../iOS/` |

> On iOS the library is a **static library**; the C# `DllImport` name must be `"__Internal"` (see below).

---

## 1. Unity (C# P/Invoke)

### 1.1 Binding Declarations

```csharp
// HsBaSlicerNative.cs
using System;
using System.Runtime.InteropServices;

public static class HsBaSlicerNative
{
#if UNITY_IOS && !UNITY_EDITOR
    private const string Lib = "__Internal";   // iOS static library
#else
    private const string Lib = "DllHsBaSlicer";
#endif

    // ---- Structs: field-for-field match with pipeline_types.h (Sequential layout) ----
    [StructLayout(LayoutKind.Sequential)]
    public struct FdmPipelineConfig
    {
        public IntPtr model_name;          // const char* (UTF-8)
        public IntPtr model_path;
        public float  layer_height;
        public float  first_layer_height;
        public double fill_spacing;
        public int    fill_mode;           // HsBaFillMode_t
        public double fill_angle;
        public int    wall_count;
        public int    top_layer_count;
        public int    bottom_layer_count;
        public double infill_density;
        public int    enable_support;
        public float  overhang_angle;
        public float  support_gap;
        public float  support_diameter;
        public float  support_density;
        public int    support_pattern;     // HsBaSupportPattern_t
        public int    interface_layers;
        public float  interface_density;
        public float  line_width;
        public float  print_speed;
        public float  travel_speed;
        public float  extrusion_multiplier;
        public IntPtr support_lua_script;
        public IntPtr support_lua_func;
        public IntPtr infill_lua_script;
        public IntPtr infill_lua_func;
        public IntPtr output_path;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct FdmPipelineResult
    {
        public int    success;
        public int    total_layers;
        public IntPtr gcode_content;       // char*, freed by HsBaFreePipelineResult
        public IntPtr error_message;
        public double elapsed_seconds;
    }

    // ---- Callback delegates: must match the C function-pointer signatures ----
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ProgressCallback(int percent, IntPtr stage, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ResultCallback(FdmPipelineResult result, IntPtr userData);

    // ---- Exported functions ----
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void initialize();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern FdmPipelineConfig HsBaCreateDefaultConfig();

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern FdmPipelineResult HsBaRunFdmPipeline(
        ref FdmPipelineConfig config, ProgressCallback callback, IntPtr userData);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void HsBaRunFdmPipelineAsync(
        ref FdmPipelineConfig config, ProgressCallback callback, IntPtr userData,
        ResultCallback resultCallback, IntPtr resultUserData);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void HsBaFreePipelineResult(ref FdmPipelineResult result);
}
```

> SLA / SLS structs have more fields but are declared the same way: mirror the **order and types** from `pipeline_types.h`, using `IntPtr` for every `const char*` and `int` for enums.

### 1.2 High-Level Wrapper (Async + Main-Thread Marshalling)

```csharp
// HsBaSlicerService.cs
using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using UnityEngine;

public static class HsBaSlicerService
{
    // Keep delegate references alive so the GC cannot collect them
    // (a collected delegate behind a native callback = crash).
    private static HsBaSlicerNative.ProgressCallback s_progress;
    private static HsBaSlicerNative.ResultCallback   s_result;

    public static void Init() => HsBaSlicerNative.initialize();

    public static Task<(bool ok, string gcode, string error)> SliceFdmAsync(
        string modelPath, Action<int, string> onProgress = null)
    {
        var tcs = new TaskCompletionSource<(bool, string, string)>();

        var cfg = HsBaSlicerNative.HsBaCreateDefaultConfig();
        // Unmanaged UTF-8 memory: config is read only during the call,
        // so it can be freed once the async call returns.
        var namePtr = Marshal.StringToCoTaskMemUTF8("model");
        var pathPtr = Marshal.StringToCoTaskMemUTF8(modelPath);
        cfg.model_name = namePtr;
        cfg.model_path = pathPtr;

        s_progress = (percent, stagePtr, _) =>
        {
            var stage = Marshal.PtrToStringUTF8(stagePtr) ?? "";
            // Callback runs on the native worker thread -> post to Unity main thread
            UnityMainThread.Post(() => onProgress?.Invoke(percent, stage));
        };

        s_result = (result, _) =>
        {
            var gcode = Marshal.PtrToStringUTF8(result.gcode_content) ?? "";
            var err   = Marshal.PtrToStringUTF8(result.error_message) ?? "";
            var ok    = result.success != 0;
            HsBaSlicerNative.HsBaFreePipelineResult(ref result);   // free after copying

            Marshal.FreeCoTaskMem(namePtr);
            Marshal.FreeCoTaskMem(pathPtr);

            UnityMainThread.Post(() => tcs.SetResult((ok, gcode, err)));
        };

        HsBaSlicerNative.HsBaRunFdmPipelineAsync(ref cfg, s_progress, IntPtr.Zero,
                                                 s_result, IntPtr.Zero);
        return tcs.Task;
    }
}
```

### 1.3 Usage in a MonoBehaviour

```csharp
public class SliceDemo : MonoBehaviour
{
    public UnityEngine.UI.Slider progressBar;

    async void Start()
    {
        HsBaSlicerService.Init();

        var (ok, gcode, err) = await HsBaSlicerService.SliceFdmAsync(
            Application.streamingAssetsPath + "/stanford_bunny.stl",
            (percent, stage) => progressBar.value = percent / 100f);

        Debug.Log(ok ? $"Sliced OK, {gcode.Length} chars of G-code" : $"Failed: {err}");
    }
}
```

### 1.4 Unity Notes

- **Keep delegates alive**: delegates handed to native code must be stored in static/member fields; a GC-collected delegate crashes when the native callback fires;
- **Main-thread marshalling**: native callbacks are not on the Unity main thread and must not touch `UnityEngine.Object` directly — post through a main-thread queue (e.g. `UnitySynchronizationContext` or a custom `MainThread.Post`);
- **Android**: place per-ABI `libDllHsBaSlicer.so` files under `Assets/Plugins/Android/libs/arm64-v8a/` etc. and enable the Android platform in the Inspector;
- **iOS**: place `libDllHsBaSlicer.a` and all of its static dependencies into `Assets/Plugins/iOS/`, and use `"__Internal"` in `DllImport`;
- **IL2CPP**: by-value struct passing works under IL2CPP, but keep `LayoutKind.Sequential` and match field types exactly (`float` for `float`, `double` for `double`).

---

## 2. Unreal Engine

### 2.1 ThirdParty Module Layout

```
Plugins/HsBaSlicer/
├── HsBaSlicer.uplugin
├── Source/
│   ├── HsBaSlicer/                    # wrapper module (UFUNCTIONs for Blueprint)
│   │   ├── HsBaSlicer.Build.cs
│   │   └── Private/...
│   └── ThirdParty/DllHsBaSlicer/
│       ├── DllHsBaSlicer.Build.cs     # third-party library module
│       ├── include/                   # copy of all DllHsBaSlicer headers
│       └── lib/
│           ├── Win64/DllHsBaSlicer.dll + .lib
│           ├── Linux/libDllHsBaSlicer.so
│           └── Mac/libDllHsBaSlicer.dylib
```

### 2.2 ThirdParty Module Build.cs

```csharp
// ThirdParty/DllHsBaSlicer/DllHsBaSlicer.Build.cs
using System.IO;
using UnrealBuildTool;

public class DllHsBaSlicer : ModuleRules
{
    public DllHsBaSlicer(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string LibDir = Path.Combine(ModuleDirectory, "lib", Target.Platform.ToString());
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "DllHsBaSlicer.lib"));
            PublicDelayLoadDLLs.Add("DllHsBaSlicer.dll");
            RuntimeDependencies.Add("$(BinaryOutputDir)/DllHsBaSlicer.dll",
                                    Path.Combine(LibDir, "DllHsBaSlicer.dll"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libDllHsBaSlicer.so"));
            RuntimeDependencies.Add(Path.Combine(LibDir, "libDllHsBaSlicer.so"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libDllHsBaSlicer.dylib"));
        }
    }
}
```

### 2.3 C++ Wrapper (Async + GameThread Marshalling)

```cpp
// HsBaSlicerSubsystem.h
#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "HsBaSlicerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSliceProgress, int32, Percent, FString, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSliceFinished, bool, bSuccess, FString, GCode, FString, Error);

UCLASS()
class UHsBaSlicerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "HsBaSlicer")
    void SliceFdmAsync(const FString& ModelPath);

    UPROPERTY(BlueprintAssignable) FOnSliceProgress  OnSliceProgress;
    UPROPERTY(BlueprintAssignable) FOnSliceFinished  OnSliceFinished;
};
```

```cpp
// HsBaSlicerSubsystem.cpp
#include "HsBaSlicerSubsystem.h"
#include "Async/Async.h"

#include "initialize.h"      // from the ThirdParty include path
#include "fdm_pipeline.h"

void UHsBaSlicerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    initialize();   // once per process
}

void UHsBaSlicerSubsystem::SliceFdmAsync(const FString& ModelPath)
{
    // UTF-8 bytes must outlive the slicing run -> capture in a shared pointer
    auto PathUtf8 = MakeShared<TArray<uint8>>();
    FTCHARToUTF8 Conv(*ModelPath);
    PathUtf8->Append((const uint8*)Conv.Get(), Conv.Length() + 1);

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [this, PathUtf8]()
        {
            HsBaFdmPipelineConfig_t Cfg = HsBaCreateDefaultConfig();
            Cfg.model_name = "ue_model";
            Cfg.model_path = (const char*)PathUtf8->GetData();

            // Progress callback: native thread -> GameThread
            HsBaFdmPipelineResult_t Result = HsBaRunFdmPipeline(
                &Cfg,
                [](int Percent, const char* Stage, void* UserData)
                {
                    auto* Self = static_cast<UHsBaSlicerSubsystem*>(UserData);
                    FString StageText = UTF8_TO_TCHAR(Stage ? Stage : "");
                    AsyncTask(ENamedThreads::GameThread,
                        [Self, Percent, StageText]()
                        { Self->OnSliceProgress.Broadcast(Percent, StageText); });
                },
                this);

            const bool bOk    = Result.success != 0;
            FString GCode     = UTF8_TO_TCHAR(Result.gcode_content ? Result.gcode_content : "");
            FString Error     = UTF8_TO_TCHAR(Result.error_message ? Result.error_message : "");
            HsBaFreePipelineResult(&Result);

            AsyncTask(ENamedThreads::GameThread,
                [this, bOk, GCode = MoveTemp(GCode), Error = MoveTemp(Error)]()
                { OnSliceFinished.Broadcast(bOk, GCode, Error); });
        });
}
```

### 2.4 Usage from Blueprint

1. In a level or widget Blueprint, get `Get Game Instance Subsystem (HsBaSlicer Subsystem)`;
2. Bind the `OnSliceProgress` / `OnSliceFinished` dynamic delegates;
3. Call `Slice Fdm Async`, then update progress bars / save G-code inside the bound events.

### 2.5 UE Notes

- **String encoding**: UE uses TCHAR (UTF-16 on Windows) internally; convert to UTF-8 before crossing the C boundary (`FTCHARToUTF8`), and keep the byte array alive for the whole slicing run;
- **Thread discipline**: `UFUNCTION`s and delegate broadcasts must happen on the GameThread — always wrap native callbacks in `AsyncTask(ENamedThreads::GameThread, ...)`;
- **Packaging**: `RuntimeDependencies` ships the DLL with the build/pak; when using delay loading (`PublicDelayLoadDLLs`) on Windows, call `FPlatformProcess::PushDllDirectory` at module startup or keep the DLL in the Binaries directory;
- **Server/console targets**: conditionally exclude the module in Build.cs for platforms with no slicing requirement.

---

## 3. SLA / SLS Mapping

Calling SLA / SLS from Unity or UE follows the identical pattern — swap according to this table:

| Process | Config type | Default-config function | Run function | Free function |
| --- | --- | --- | --- | --- |
| FDM | `HsBaFdmPipelineConfig_t` | `HsBaCreateDefaultConfig` | `HsBaRunFdmPipeline(Async)` | `HsBaFreePipelineResult` |
| SLA | `HsBaSlaPipelineConfig_t` | `HsBaCreateDefaultSlaConfig` | `HsBaRunSlaPipeline(Async)` | `HsBaFreeSlaPipelineResult` |
| SLS | `HsBaSlsPipelineConfig_t` | `HsBaCreateDefaultSlsConfig` | `HsBaRunSlsPipeline(Async)` | `HsBaFreeSlsPipelineResult` |

> SLS requires `export_lua_script` (a Lua export script shipped with your app); the result's `export_path` is the exported file path.

## FAQ

| Symptom | Cause & Fix |
| --- | --- |
| `DllNotFoundException` in Unity | Plugin in the wrong folder or platform not ticked in the Inspector; editor and device use different folders |
| Unity crash after a callback | Delegate was GC-collected; keep a strong reference to it |
| UE packaged build can't find the DLL | Missing `RuntimeDependencies`; check the Binaries directory |
| iOS link failure (Unity/UE) | iOS requires the static `.a` library and `DllImport("__Internal")`; dylibs cannot be loaded |
| Struct fields misaligned / random crashes | P/Invoke struct field order or types don't match `pipeline_types.h` |
