# C++ Usage Guide (CMake Integration)

This document describes how to use the three core modules of HsBaSlicer in an external C++ project via CMake:

| Module | Type | Description |
| --- | --- | --- |
| **LibHsBaSlicer** | C++ library (SHARED/STATIC) | Core slicing algorithms; supports both traditional headers and C++20 modules |
| **ModuleHsBaSlicer** | C++20 module wrapper | Class-based API (Model/FdmPipeline/SlaPipeline), `import hsba.slicer;` |
| **DllHsBaSlicer** | C export library (SHARED/STATIC) | Pure C ABI pipeline interface for cross-language integration |
| **HsBaSlicer** | Executable / platform library | Application entry (exe on desktop, .so on Android, .a on iOS) |

---

## Prerequisites

- **CMake** ≥ 3.28
- **C++20** compatible compiler (MSVC 19.36+, GCC 13+, Clang 16+)
- HsBaSlicer installed via `cmake --install`, or included as a subdirectory

---

## Installing HsBaSlicer

```bash
cmake --preset <preset-name>
cmake --build --preset <preset-name>
cmake --install build --prefix /path/to/install
```

Installed directory layout:

```
<prefix>/
├── bin/                          # Executables, DLLs (Windows)
├── lib/                          # Static libs / import libs / shared libs (Linux)
│   └── cmake/HsBaSlicer/        # CMake package config files
│       ├── HsBaSlicerConfig.cmake
│       ├── HsBaSlicerConfigVersion.cmake
│       └── HsBaSlicerTargets.cmake
├── include/HsBaSlicer/           # All public headers
│   ├── LibHsBaSlicer/            # LibHsBaSlicer C++ API
│   ├── modules/                  # C++20 module interface files (.cppm)
│   ├── pipelinetypes/            # C struct definitions
│   ├── 2D/                       # Polygon types
│   ├── meshmodel/                # Mesh model
│   ├── paths/                    # Path types
│   ├── support/                  # Support config
│   ├── convert/                  # Proto conversion
│   ├── proto/                    # Protobuf generated headers
│   ├── base/                     # Base types
│   └── logger/                   # Logger
└── share/HsBaSlicer/proto/       # Proto multi-language generated files
```

---

## Method 1: find_package (Recommended)

### Basic CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::LibHsBaSlicer)
```

Specify the install prefix at configure time:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/install
```

---

## Using LibHsBaSlicer (Non-Module — Traditional Headers)

Works with any C++20 compatible compiler; no module support required.

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MySlicerApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::LibHsBaSlicer)
```

### Code Example

```cpp
#include <LibHsBaSlicer/Preprocess/model_preprocess.hpp>
#include <LibHsBaSlicer/Slice/mesh_slice.hpp>
#include <LibHsBaSlicer/Support/fdm_support.hpp>
#include <LibHsBaSlicer/Fill/polygon_fill.hpp>
#include <LibHsBaSlicer/Path/path_generator.hpp>
#include <LibHsBaSlicer/Path/sls_export.hpp>
#include <LibHsBaSlicer/Floor/sla_floor.hpp>

using namespace HsBa::Slicer;

int main()
{
    // 1. Preprocess: load model
    // auto model = LoadModel("model.stl");

    // 2. Slice: generate layer contours
    // auto contours = Slice(model, layer_height);

    // 3. Support: generate FDM supports
    // auto supports = GenerateAllFdmSupport(...);

    // 4. Fill: polygon infill
    // auto fill_paths = FillPolygon(...);

    // 5. Path: generate G-code
    // auto gcode = GenerateGCodePath(...);

    return 0;
}
```

### Available Targets

| CMake Target | Description |
| --- | --- |
| `HsBaSlicer::LibHsBaSlicer` | Core C++ slicing library |
| `HsBaSlicer::ModuleHsBaSlicer` | C++20 module wrapper (class-based API) |
| `HsBaSlicer::DllHsBaSlicer` | C ABI export layer |
| `HsBaSlicer::HsBaPipelineTypes` | Header-only type definitions (INTERFACE) |
| `HsBaSlicer::HsBaSlicerProto` | Protobuf message library |
| `HsBaSlicer::HsBaSlicerConverter` | Proto ↔ internal type conversion |

---

## Using ModuleHsBaSlicer (C++20 Module)

Provides a class-based C++ API (`Model`, `FdmPipeline`, `SlaPipeline`, etc.) with exceptions and RAII.
Requires a C++20 module-capable compiler (MSVC 19.34+, GCC 14+, Clang 16+).

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyModuleApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::ModuleHsBaSlicer)
```

### Code Example

```cpp
import hsba.slicer;

#include <iostream>
#include <format>

using namespace HsBa::Slicer;

int main()
{
    try
    {
        // RAII model management (auto cleanup on destruction)
        Model model("bunny", "models/stanford_bunny.stl");
        auto info = model.info();
        std::cout << std::format("Volume: {:.2f} mm^3", info.volume) << std::endl;

        // FDM full pipeline (using pipeline_types.h config)
        HsBaFdmPipelineConfig_t cfg = defaultFdmConfig();
        cfg.layer_height = 0.2f;
        cfg.fill_mode    = HSBA_FILL_ZIGZAG;
        cfg.output_path  = "output/result.gcode";

        FdmPipeline pipeline(cfg);
        FdmResult result = pipeline.run(model);

        std::cout << std::format("Layers: {}, G-code: {} bytes",
                                 result.total_layers, result.gcode.size()) << std::endl;
    }
    catch (const SlicerError& e)
    {
        std::cerr << "Slicer error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### Main Classes

| Class | Description |
| --- | --- |
| `Model` | RAII model handle (load/transform/slice/auto-release) |
| `FdmPipeline` | FDM full pipeline (slice→support→fill→path) |
| `SlaPipeline` | SLA full pipeline (slice→support→floor→render→package) |
| `SlsPipeline` | SLS Lua-driven export |
| `SlicerError` | Unified exception type |

### Platform Notes

> **MSVC (Windows)**
>
> 1. **`#include` must precede `import`**: In MSVC module consumers, standard library `#include` directives must appear before `import hsba.slicer;`. Otherwise the BMI's std declarations conflict with re-included headers, producing C2572 redefinition errors.
> 2. **Compile environment consistency (C5050)**: CGAL propagates `/fp:strict` and `_SCL_SECURE_NO_WARNINGS` via INTERFACE properties. `ModuleHsBaSlicer` re-exports these flags as PUBLIC, so consumers inherit them automatically through linking—no manual setup required.
> 3. **Static library**: `ModuleHsBaSlicer` is always built as STATIC (avoids DLL link-stage dependency on system libraries like `oleaut32.lib`). Consumers receive the full transitive dependency chain via PUBLIC linkage.

> **Clang / GCC (Linux / Android NDK)**
>
> 1. **GMF types are NOT exported by the module** (C++20 standard limitation): Types introduced via `#include` in the module's Global Module Fragment (GMF)—such as `ModelInfo`, `LayerPathData`, `PointsPath`—are **not** visible to consumers. Consumers must include the relevant headers directly:
>    ```cpp
>    #include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"  // ModelInfo
>    #include "pipelinetypes/pipeline_types.h"                 // HsBaFdmPipelineConfig_t, etc.
>    ```
> 2. Standard library headers are unaffected; `#include`/`import` ordering is not sensitive under Clang/GCC.

> **General**
>
> - Pipeline config structs (`HsBaFdmPipelineConfig_t`, `HsBaSlaPipelineConfig_t`, `HsBaSlsPipelineConfig_t`) are defined in `pipelinetypes/pipeline_types.h`. The module version and header version share the same definitions—do NOT redefine them.
> - The module is a single-file implementation (`ModuleHsBaSlicer/hsba_slicer.cppm`), merging interface and implementation to avoid MSVC C2572 errors caused by implicit imports in separate implementation units.

---

## Using DllHsBaSlicer (C ABI Interface)

Suitable for calling the full pipeline from C/C++, or as a cross-language bridging layer (C#/Java/Python/Swift, etc.).

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyPipelineApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::DllHsBaSlicer)
```

### Code Example

```cpp
#include <initialize.h>
#include <fdm_pipeline.h>
#include <sla_pipeline.h>
#include <sls_pipeline.h>

static void OnProgress(int percent, const char* stage, void* /*ud*/)
{
    // Progress notification
}

int main()
{
    // Must be called first
    initialize();

    // FDM pipeline
    HsBaFdmPipelineConfig_t cfg = HsBaCreateDefaultConfig();
    cfg.model_name  = "my_model";
    cfg.model_path  = "models/my_model.stl";
    cfg.output_path = "output/my_model.gcode";

    HsBaFdmPipelineResult_t result = HsBaRunFdmPipeline(&cfg, OnProgress, nullptr);
    if (result.success)
    {
        // Use result.gcode_content ...
    }
    HsBaFreePipelineResult(&result);

    return 0;
}
```

### Header-Only Type Definitions (No DLL Dependency)

If you only need struct/enum definitions (e.g., for P/Invoke mirroring), link the header-only target:

```cmake
target_link_libraries(my_app PRIVATE HsBaSlicer::HsBaPipelineTypes)
```

```cpp
#include <pipelinetypes/pipeline_types.h>
// Use HsBaFdmPipelineConfig_t etc. without linking any library
```

---

## Using the HsBaSlicer Executable Target

`HsBaSlicer` is the final application entry point. It builds as an executable on desktop, a shared library (.so) on Android, and a static library (.a) on iOS.

Normally you don't need to link this target from external projects. To integrate the entire project as a subdirectory:

### Subdirectory Integration (add_subdirectory)

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the entire HsBaSlicer project
add_subdirectory(third_party/HsBaSlicer)

# Link the desired target
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE LibHsBaSlicer)        # Non-module version
# or
target_link_libraries(my_app PRIVATE ModuleHsBaSlicer)     # C++20 module version
# or
target_link_libraries(my_app PRIVATE DllHsBaSlicer)        # C ABI version
```

> **Note**: When using `add_subdirectory`, target names do NOT carry the `HsBaSlicer::` namespace prefix.

---

## Build Options Reference

| Option | Default | Description |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | `ON` | Build shared libraries (OFF for static) |
| `HSBA_SLICER_MODULE` | `ON` | Build ModuleHsBaSlicer C++20 module wrapper |
| `HSBA_SLICER_BUILD_SAMPLES` | `ON` | Build sample programs |
| `HSBA_SLICER_USE_TESTS` | `ON` | Build tests |
| `HSBA_PROTOBUF_OUT` | `ON` | Output Proto multi-language generated files |

---

## Transitive Dependencies

`find_package(HsBaSlicer)` automatically locates the following public dependencies:

- Eigen3
- magic_enum
- Clipper2
- Lua
- Protobuf
- OpenSSL

Ensure these packages are discoverable via `CMAKE_PREFIX_PATH` (usually handled automatically by the vcpkg toolchain).

---

## Complete Example Project Structure

```
my_project/
├── CMakeLists.txt
├── main.cpp
└── vcpkg.json          # If using vcpkg for dependency management
```

**CMakeLists.txt**:

```cmake
cmake_minimum_required(VERSION 3.28)
project(MySlicerApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(HsBaSlicer REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE HsBaSlicer::LibHsBaSlicer)
```

---

## Related Samples

- `samples/LibHsBaSlicer/main_header.cpp` — Full example (traditional #include, full FDM slicing workflow)
- `samples/LibHsBaSlicer/main_module.cpp` — C++20 module example (`import hsba.slicer;`, class-based API)
- `samples/FDM/` — DllHsBaSlicer C ABI sync/async, Lua custom examples
- `samples/SLA/` — SLA pipeline example
- `samples/SLS/` — SLS pipeline example

## Related Documentation

- [LibHsBaSlicer Module](./LibHsBaSlicer/) — Core C++ slicing API reference
- [DllHsBaSlicer Module](./DllHsBaSlicer/) — C ABI pipeline interface & cross-platform integration
- [Qt / wxWidgets Integration](./DllHsBaSlicer/qt_wxwidgets_integration.md)
- [Unity / Unreal Engine Integration](./DllHsBaSlicer/game_engine_integration.md)
