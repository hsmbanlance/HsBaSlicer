Built as a single static library `HsBaSlicerBase` (see `base/CMakeLists.txt`) that exposes two namespaces: `HsBa::Slicer` for core runtime types (`IModel`, `ModelFormat`, `MemoryPool`, `StaticMemoryPool`, `InplaceVector`, `ThreadPool`, `Singleton`, `units.hpp`) and `HsBa::Slicer::Utils` for cross-cutting helpers (`AnyObject` reflection, `Delegate`/`Event`/`EventSource`, `ITranslator<T>`, `concepts`, `template_helper`, `tuple_each`, `coroutine`).

Internal layering:
- Model abstraction layer: `IModel.hpp` + `ModelFormat.{hpp,cpp}` define the `IModel` interface plus the `ModelFormat` enum (PLY/OBJ/STL/OFF/VRML/STEP/IGES/BREP/SLDPRT/CATPART/XYZ/VDB) used by format loaders elsewhere.
- Reflection/type-erasure layer: `any_object.{hpp,cpp}` + `static_reflect.hpp` + `any_visit.hpp` implement a `TypeInfo`-backed `AnyObject` with field/method dispatch via `GetTypeInfo<T>()` template specializations; `boost_std_convert.hpp` bridges Boost and std containers.
- Eventing layer: `delegate.hpp` provides thread-safe `Delegate<R(Args...)>` backed by `std::shared_mutex`, with `Event` and CRTP `EventSource<Derived>` for pub/sub.
- Memory layer: `memory_pool.hpp` (per-instance `MemoryPool<T,N>` sharing `MemoryPoolState`), `object_pool.hpp`, `StaticMemoryPool<T,N>` (process-wide buffer), and `InplaceVector.hpp`.
- Platform/IO helpers: `error.{hpp,cpp}`, `encoding_convert.{hpp,cpp}`, `filename_check.{hpp,cpp}`, `eigen_translator.{hpp,cpp}` (Eigen ↔ native conversions).

Dependency direction is one-way outward: this module depends on Eigen3, Boost (locale, nowide), and magic_enum, but nothing in the rest of the repo may be included from here. The `HSBA_SLICER_USE_TESTS` CMake flag compiles in `MockRegistry` inside `AnyObject` so tests can stub `Invoke` without production overhead.