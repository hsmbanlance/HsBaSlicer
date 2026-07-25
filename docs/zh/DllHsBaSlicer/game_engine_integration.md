# Unity / Unreal Engine 集成指南

本文演示如何在 **Unity（C# P/Invoke）** 与 **Unreal Engine（C++ 插件 + Blueprint）** 中调用 DllHsBaSlicer 流水线。

> 前置阅读：[DllHsBaSlicer 模块总览](./README.md)

## 各平台库文件放置

| 平台 | 产物 | Unity 放置位置 | UE 放置位置 |
| --- | --- | --- | --- |
| Windows x64 | `DllHsBaSlicer.dll` | `Assets/Plugins/x86_64/` | 插件 `ThirdParty/.../Win64/` |
| Linux | `libDllHsBaSlicer.so` | `Assets/Plugins/x86_64/` | `ThirdParty/.../Linux/` |
| macOS | `libDllHsBaSlicer.dylib` | `Assets/Plugins/` | `ThirdParty/.../Mac/` |
| Android | `libDllHsBaSlicer.so` | `Assets/Plugins/Android/libs/<abi>/` | 随引擎工具链打包 |
| iOS | `libDllHsBaSlicer.a`（静态库） | `Assets/Plugins/iOS/` | `ThirdParty/.../iOS/` |

> iOS 上库为**静态库**，C# 侧 `DllImport` 必须写 `"__Internal"`（见下文）。

---

## 一、Unity（C# P/Invoke）

### 1. 绑定声明

```csharp
// HsBaSlicerNative.cs
using System;
using System.Runtime.InteropServices;

public static class HsBaSlicerNative
{
#if UNITY_IOS && !UNITY_EDITOR
    private const string Lib = "__Internal";   // iOS 静态库
#else
    private const string Lib = "DllHsBaSlicer";
#endif

    // ---- 结构体：与 pipeline_types.h 逐字段对齐（LayoutKind.Sequential）----
    [StructLayout(LayoutKind.Sequential)]
    public struct FdmPipelineConfig
    {
        public IntPtr model_name;          // const char*（UTF-8）
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
        public IntPtr gcode_content;       // char*，用 HsBaFreePipelineResult 释放
        public IntPtr error_message;
        public double elapsed_seconds;
    }

    // ---- 回调委托：必须与 C 端函数指针签名一致 ----
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ProgressCallback(int percent, IntPtr stage, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ResultCallback(FdmPipelineResult result, IntPtr userData);

    // ---- 导出函数 ----
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

    // ---- Lua 扩展函数注册 ----
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void LuaRegFn(IntPtr luaState);   // void (*)(lua_State*)

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void HsBaAdd2DFunction(LuaRegFn func);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void HsBaAdd3DFunction(LuaRegFn func);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void HsBaAddFileFunction(LuaRegFn func);

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)]
    public static extern void HsBaAddEventCallback(IntPtr eventName, LuaRegFn func);
}
```

> SLA / SLS 结构体字段较多，声明方式相同：按 `pipeline_types.h` 中字段**顺序与类型**逐一映射，`const char*` 一律用 `IntPtr`，枚举用 `int`。

### 2. 高层封装（异步 + 主线程调度）

```csharp
// HsBaSlicerService.cs
using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using UnityEngine;

public static class HsBaSlicerService
{
    // 持有委托引用，防止被 GC 回收导致原生回调崩溃
    private static HsBaSlicerNative.ProgressCallback s_progress;
    private static HsBaSlicerNative.ResultCallback   s_result;

    public static void Init() => HsBaSlicerNative.initialize();

    public static Task<(bool ok, string gcode, string error)> SliceFdmAsync(
        string modelPath, Action<int, string> onProgress = null)
    {
        var tcs = new TaskCompletionSource<(bool, string, string)>();

        var cfg = HsBaSlicerNative.HsBaCreateDefaultConfig();
        // UTF-8 非托管内存：config 仅在调用期间被读取，返回后即可释放
        var namePtr = Marshal.StringToCoTaskMemUTF8("model");
        var pathPtr = Marshal.StringToCoTaskMemUTF8(modelPath);
        cfg.model_name = namePtr;
        cfg.model_path = pathPtr;

        s_progress = (percent, stagePtr, _) =>
        {
            var stage = Marshal.PtrToStringUTF8(stagePtr) ?? "";
            // 回调在原生工作线程 → 借助 Unity 同步上下文回主线程
            UnityMainThread.Post(() => onProgress?.Invoke(percent, stage));
        };

        s_result = (result, _) =>
        {
            var gcode = Marshal.PtrToStringUTF8(result.gcode_content) ?? "";
            var err   = Marshal.PtrToStringUTF8(result.error_message) ?? "";
            var ok    = result.success != 0;
            HsBaSlicerNative.HsBaFreePipelineResult(ref result);   // 拷贝后立即释放

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

### 3. MonoBehaviour 中使用

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

        Debug.Log(ok ? $"切片完成，{gcode.Length} 字符 G-code" : $"失败：{err}");
    }
}
```

### 4. Unity 注意事项

- **委托必须保活**：传给原生的委托需保存为静态/成员引用，否则 GC 回收后回调触发即崩溃；
- **主线程调度**：原生回调不在 Unity 主线程，不能直接操作 `UnityEngine.Object`，需经主线程队列（如 `UnitySynchronizationContext` 或自实现 `MainThread.Post`）；
- **Android**：将各 ABI 的 `libDllHsBaSlicer.so` 放入 `Assets/Plugins/Android/libs/arm64-v8a/` 等目录，并在 Inspector 中勾选 Android 平台；
- **iOS**：将 `libDllHsBaSlicer.a` 及其全部静态依赖放入 `Assets/Plugins/iOS/`，`DllImport` 使用 `"__Internal"`；
- **IL2CPP**：结构体按值传递在 IL2CPP 下正常工作，但请保持 `LayoutKind.Sequential` 且字段类型与 C 端严格一致（`float` 对 `float`、`double` 对 `double`）。

---

## 二、Unreal Engine

### 1. ThirdParty 模块结构

```
Plugins/HsBaSlicer/
├── HsBaSlicer.uplugin
├── Source/
│   ├── HsBaSlicer/                    # 包装模块（UFUNCTION 暴露给 Blueprint）
│   │   ├── HsBaSlicer.Build.cs
│   │   └── Private/...
│   └── ThirdParty/DllHsBaSlicer/
│       ├── DllHsBaSlicer.Build.cs     # 第三方库模块
│       ├── include/                   # 复制 DllHsBaSlicer 全部头文件
│       └── lib/
│           ├── Win64/DllHsBaSlicer.dll + .lib
│           ├── Linux/libDllHsBaSlicer.so
│           └── Mac/libDllHsBaSlicer.dylib
```

### 2. ThirdParty 模块 Build.cs

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

### 3. C++ 包装层（异步 + GameThread 调度）

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

#include "initialize.h"      // ThirdParty include 路径
#include "fdm_pipeline.h"

void UHsBaSlicerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    initialize();   // 进程内调用一次
}

void UHsBaSlicerSubsystem::SliceFdmAsync(const FString& ModelPath)
{
    // UTF-8 字节需存活到切片结束 → 放入共享指针
    auto PathUtf8 = MakeShared<TArray<uint8>>();
    FTCHARToUTF8 Conv(*ModelPath);
    PathUtf8->Append((const uint8*)Conv.Get(), Conv.Length() + 1);

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
        [this, PathUtf8]()
        {
            HsBaFdmPipelineConfig_t Cfg = HsBaCreateDefaultConfig();
            Cfg.model_name = "ue_model";
            Cfg.model_path = (const char*)PathUtf8->GetData();

            // 进度回调：原生线程 → GameThread
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

### 4. Blueprint 中使用

1. 在关卡蓝图或 Widget 蓝图中获取 `Get Game Instance Subsystem (HsBaSlicer Subsystem)`；
2. 绑定 `OnSliceProgress` / `OnSliceFinished` 动态委托；
3. 调用 `Slice Fdm Async`，在事件回调中更新进度条 / 保存 G-code。

### 5. UE 注意事项

- **字符串编码**：UE 内部为 TCHAR（Windows 上 UTF-16），传入 C 接口前必须转 UTF-8（`FTCHARToUTF8`），且字节数组生命周期需覆盖整个切片过程；
- **线程纪律**：`UFUNCTION`/委托广播必须在 GameThread，原生回调中一律先 `AsyncTask(ENamedThreads::GameThread, ...)`；
- **打包**：`RuntimeDependencies` 确保 DLL 随 Pak/构建产物分发；Windows 使用延迟加载（`PublicDelayLoadDLLs`）时需在模块启动时 `FPlatformProcess::PushDllDirectory` 或保证 DLL 在 Binaries 目录；
- **专用服务器/主机平台**：无切片需求的目标平台应在 Build.cs 中条件排除该模块。

---

## 三、SLA / SLS 对应关系

Unity 与 UE 中调用 SLA / SLS 的模式与 FDM 相同，对照替换即可：

| 工艺 | 配置类型 | 默认配置函数 | 运行函数 | 释放函数 |
| --- | --- | --- | --- | --- |
| FDM | `HsBaFdmPipelineConfig_t` | `HsBaCreateDefaultConfig` | `HsBaRunFdmPipeline(Async)` | `HsBaFreePipelineResult` |
| SLA | `HsBaSlaPipelineConfig_t` | `HsBaCreateDefaultSlaConfig` | `HsBaRunSlaPipeline(Async)` | `HsBaFreeSlaPipelineResult` |
| SLS | `HsBaSlsPipelineConfig_t` | `HsBaCreateDefaultSlsConfig` | `HsBaRunSlsPipeline(Async)` | `HsBaFreeSlsPipelineResult` |

> SLS 的 `export_lua_script` 必须提供（指向随包分发的 Lua 导出脚本），结果 `export_path` 为导出文件路径。

---

## 四、Lua 扩展函数注册

在流水线运行前注册自定义 Lua 函数，各阶段创建 Lua 环境时自动注入。

### Unity（C# P/Invoke）

```csharp
// 委托必须保活（静态引用）
private static HsBaSlicerNative.LuaRegFn s_luaReg;

public static void RegisterLuaExtensions()
{
    s_luaReg = (IntPtr luaState) =>
    {
        // 注意：此处收到的是 lua_State* 指针
        // 若需在 C# 侧操作 Lua，可配合 NLua/MoonSharp 等库使用
        // 通常建议将自定义函数写在原生 C/C++ 侧
    };

    HsBaSlicerNative.HsBaAdd3DFunction(s_luaReg);  // 切片/支撑阶段
    // HsBaSlicerNative.HsBaAdd2DFunction(s_luaReg);
    // HsBaSlicerNative.HsBaAddFileFunction(s_luaReg);

    // 事件回调
    var namePtr = Marshal.StringToCoTaskMemUTF8("zipper.on_add");
    HsBaSlicerNative.HsBaAddEventCallback(namePtr, s_luaReg);
    // namePtr 不可释放（库内部保留指针）
}
```

> **实践建议**：由于 Lua C API 操作在原生侧更自然，建议将自定义 Lua 函数编写为 C/C++ 插件（单独 .dll/.so），在其 `DllMain`/`__attribute__((constructor))` 中调用 `HsBaAddXXXFunction`，或在游戏初始化时显式调用。

### Unreal Engine（C++）

```cpp
#include "lua_register.h"
#include <lua.hpp>

static int l_my_ue_func(lua_State* L)
{
    // 自定义实现
    return 0;
}

static void registerMyLuaFunctions(lua_State* L)
{
    lua_register(L, "my_ue_func", l_my_ue_func);
}

// 在模块启动或 GameInstance 初始化时调用
void UHsBaSlicerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    initialize();

    // 注册 Lua 扩展函数
    HsBaAdd3DFunction(registerMyLuaFunctions);   // 切片/支撑
    HsBaAdd2DFunction(registerMyLuaFunctions);   // 支撑/填充/SLA输出
    // HsBaAddFileFunction(registerMyLuaFunctions);
    // HsBaAddEventCallback("zipper.on_add", registerMyLuaFunctions);
}
```

### 各阶段可用函数类型

| 流水线阶段 | 注册函数 | 说明 |
| --- | --- | --- |
| Slice | `HsBaAdd3DFunction` | 3D 模型操作 |
| Support | `HsBaAdd2DFunction` + `HsBaAdd3DFunction` | 2D + 3D |
| Fill | `HsBaAdd2DFunction` | 2D 多边形 |
| SLS Output | `HsBaAddFileFunction` | 文件输出 |
| SLA Output | `HsBaAdd2DFunction` + `HsBaAddFileFunction` | 2D + 文件 |

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| Unity 中 `DllNotFoundException` | 插件未放对目录或未在 Inspector 勾选当前平台；编辑器与真机目录不同 |
| 回调后 Unity 崩溃 | 委托被 GC 回收；保存委托的强引用 |
| UE 打包后找不到 DLL | 未配置 `RuntimeDependencies`；检查 Binaries 目录 |
| iOS 链接失败（Unity/UE） | iOS 必须使用静态库 `.a` 且 `DllImport("__Internal")`，不能加载 dylib |
| 结构体字段错位 / 随机崩溃 | P/Invoke 结构体字段顺序或类型与 `pipeline_types.h` 不一致 |
