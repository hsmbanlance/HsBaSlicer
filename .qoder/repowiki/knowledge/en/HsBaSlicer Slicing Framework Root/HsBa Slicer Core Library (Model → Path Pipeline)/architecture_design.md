Single CMake target `LibHsBaSlicer` (`CMakeLists.txt`) that composes several internal subsystems into one public C++ API under namespace `HsBa::Slicer`, with every exported symbol decorated by `HSBA_SLICER_LIB_API` from `export.h`.

- Public boundary: headers in each subdirectory (`Slice/`, `Preprocess/`, `Support/`, `Fill/`, `Path/`, `Floor/`, `Transfer/`, `ParamStore/`) declare functions/structs marked `HSBA_SLICER_LIB_API`; `version_info.{hpp,cpp}` exposes `GetVersionJson` / `GetVersionXml` as the only version-query entry point.
- Internal layering (one folder per responsibility):
  - `Preprocess/model_preprocess` — model loading & normalization.
  - `Slice/mesh_slice` — planar Z-slicing (`Slice`, `UnSafeSlice`, `BuildSliceTopology`, `SliceLayer`) plus Lua-driven variants.
  - `Support/fdm_support` — FDM support structure generation.
  - `Fill/polygon_fill` — polygon fill algorithms.
  - `Path/*` — FDM G-code path generation (`path_generator`, `spiral_path`, `path_optimizer`) and process-specific exporters (`sls_export`, `waam_export`).
  - `Floor/sla_floor` — SLA floor generation.
  - `Transfer/file_transfer` — file I/O helpers.
  - `ParamStore/param_store_ops` — parameter storage operations.
- Extensibility via Lua: `Extends/lua_pipeline` provides `SetupLuaPipelineEnvironment` / `RunLuaPipeline` with `LuaPipelineContext` and `LuaPipelineOutput`; `Extends/LuaAddFunction`, `EventSourceFunction`, `LuaCommonTypes` register the `HsBa` global table (model ops, slicing, support, fill, path output, packaging) so scripts can drive an arbitrary stage order. The registration API is dimension-scoped through `Add2DFunctions` / `Add3DFunctions` / `AddFileFunctions` plus named event callbacks (`AddEventCallback`).
- Dependency direction: this library links PUBLIC to `HsBaSlicerBase`, `HsBaSlicerMesh`, `HsBaSlicer2D`, `HsBaSupport`, `HsBaPaths` (core data + 2D geometry + paths), PRIVATE to `HsBaSlicerUtils`, `HsBaPreprocess`, `HsBaVersion`, `HsBaSlicerFileOperator`, and conditionally `HsBaSlicerCADModel` when OCCT is available (`HSBA_COPL` and not Android/iOS/game-console). The CAD kernel is excluded on constrained platforms because of its copyleft license.
- Build: built as STATIC or SHARED depending on `BUILD_SHARED_LIBS`; Windows DLL export uses `HSBA_SLICER_EXPORTS` and includes `../DllHsBaSlicer/dllexport.h` for shared builds; precompiled headers via `pch_headers.hpp`.
- Recent AreaGraph extension: `Path/path_optimizer.hpp` adds `RegionPathOptimizer::FullGraphData` (N×N gate-aware shortest-path cost matrix + flat edge list) and `ExpandedGraphData` (all entry/exit gate vertices and internal transit/inter-region edges) exposed through `getFullGraph()` / `getExpandedGraph()`, plus `setCustomTour()` so Lua users can implement arbitrary TSP/custom ordering and feed the result back into `buildPaths()` / `buildPolygons()`.