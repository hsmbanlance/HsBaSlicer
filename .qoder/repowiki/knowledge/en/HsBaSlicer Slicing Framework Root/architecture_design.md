The root `CMakeLists.txt` is the single entry point: it enforces C++20 (with optional C23 for C), runs feature checks for concepts/ranges/source_location/NTTP-class/template-template matching, then conditionally includes subdirectories based on platform (`HSBA_DESKTOP`, `HSBA_MOBILE`, `HSBA_GAME_CONSOLE`) and the copyleft-kernel toggle (`HSBA_COPL` = AUTO/ON/OFF). Dependency wiring follows a strict layering:
- Core primitives (`base`, `cipher`, `utils`, `logger`, `version`) are always built first.
- Model abstractions (`meshmodel`, optionally `cadmodel` when CGAL/OpenCASCADE are present, `pointcloud` via OpenVDB) implement the shared `IModel` interface from `base/IModel.hpp`.
- Pipeline stages (`preprocess`, `support`, `paths`, `fileoperator`, `convert`, `pipelinetypes`, `2D`) depend only on lower layers.
- The public surface is layered as `LibHsBaSlicer` (C++ API) → `ModuleHsBaSlicer` (C++20 module wrapper) → `DllHsBaSlicer` (stable C ABI for C#/Java/Python) → `HsBaSlicer` (desktop entry + examples).
- Platform apps (`android_example`, `hsbaslicer`) link the exported DLL; samples exercise each technology pipeline.

Cross-cutting contracts enforced at the root:
- All static libraries are built with `-fPIC` so they can be linked into the shared `DllHsBaSlicer`.
- Copyleft kernels (CGAL, OpenCASCADE) are opt-in via `HSBA_COPL`; enabling them switches the project license to GPL-3.0-or-later and enables `USE_OCCT`, `USE_CGAL`, `USE_CAD_KERNEL`, and raises `HSBA_MODEL_POOL_SIZE` to 10.
- Dynamic library loading (`HSBA_USE_DLL_LOADER`) is enabled only on desktop; Android/iOS/game-console builds define `HSBA_NO_DLL_LOADER`.
- Protobuf schemas in `proto/` are generated once and consumed by `convert`, `LibHsBaSlicer`, and multi-language bindings installed under `<prefix>/share/HsBaSlicer/proto/{cpp,cs,java,python,php}`.
- Installation exports all targets under the `HsBaSlicer::` namespace via `HsBaSlicerTargets.cmake` plus `HsBaSlicerConfig.cmake`/`Version.cmake`, allowing downstream projects to `find_package(HsBaSlicer)`.
- Tests (`tests/` Boost.UnitTest + `static_tests/` compile-time assertions) are gated by `HSBA_SLICER_USE_TESTS` / `HSBA_SLICER_USE_STATIC_TESTS` and disabled on mobile/console/cross-compile.