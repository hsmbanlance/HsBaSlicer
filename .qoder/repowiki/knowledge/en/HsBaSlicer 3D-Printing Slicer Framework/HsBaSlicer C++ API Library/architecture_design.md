The module is a single CMake target (`LibHsBaSlicer`) that composes several internal stages of the slicer into one public C++ API under namespace `HsBa::Slicer`. Internal sub-packages follow a stage-oriented layout:
- `Preprocess/` — model loading, transform, boolean/thick-solid ops (CGAL-backed when `USE_CGAL` is defined).
- `Slice/` — planar mesh slicing (`Slice`, `UnSafeSlice`, reusable `BuildSliceTopology`).
- `Support/` — FDM support generation.
- `Fill/` — polygon infill algorithms.
- `Floor/` — SLA layer floor generation.
- `Path/` — per-layer G-code path generation, spiral/vase mode, SLS/WAAM exporters.
- `Transfer/` — file I/O packaging.
- `ParamStore/` — parameter storage operations.
- `Extends/` — Lua embedding: registers a `HsBa` global table exposing all pipeline building blocks to scripts; provides `SetupLuaPipelineEnvironment` / `RunLuaPipeline` for fully script-driven workflows.

Dependency direction is strictly downward: this library links PUBLIC against `HsBaSlicerBase`, `HsBaSlicerMesh`, `HsBaSlicer2D`, `HsBaSupport`, `HsBaPaths` (types & core geometry) and PRIVATE against `HsBaSlicerUtils`, `HsBaPreprocess`, `HsBaVersion`, `HsBaSlicerFileOperator`, plus optional `HsBaSlicerCADModel` when OCCT is available (`HSBA_COPL` and not Android/iOS/game-console). Public symbols are exported via the `HSBA_SLICER_LIB_API` macro from `export.h` (`__declspec(dllexport/dllimport)` on Windows, no-op elsewhere), and precompiled headers come from `pch_headers.hpp`.