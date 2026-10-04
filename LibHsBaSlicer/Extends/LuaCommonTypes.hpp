#pragma once
#ifndef HSBA_SLICER_LUA_COMMON_TYPES_HPP
#define HSBA_SLICER_LUA_COMMON_TYPES_HPP

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "2D/FloatPolygons.hpp"
#include "2D/IntPolygon.hpp"
#include "LibHsBaSlicer/export.h"
#include "base/any_object.hpp"
#include "utils/LuaAnyObject.hpp"

/**
 * @file LuaCommonTypes.hpp
 * @brief Registers commonly used custom types with `Utils::AnyObject` and exposes Lua
 *        conversions between them and plain Lua tables.
 *
 * Two things happen here:
 *   1. `GetTypeInfo<T>()` specializations add reflective metadata for every registered type:
 *      a stable `Name`, the scalar component fields (x / y / z / w) for the types that store
 *      them contiguously (so `AnyObject::ForeachField` can iterate them), and a `cast_<Name>`
 *      entry in `TypeInfo::methods` that implements `AnyObject::cast<T>()` (reachable from Lua
 *      as `obj:invoke("cast_<Name>")`). Any copy/move-constructible type can already be *stored*
 *      in an `AnyObject` through the primary template; these specializations only add metadata.
 *   2. A set of Lua adapters (see the .cpp) convert each registered type to/from a Lua
 *      table and are installed via `RegisterCommonAnyObjectTypes` /
 *      `GetCommonAnyObjectTypes`.
 *
 * Encoding used for the Lua tables:
 *   - Fixed vectors / quaternions : map with named keys x, y, (z), (w). Reading also
 *     accepts a plain sequence {v1, v2, ...}.
 *   - Matrices (fixed & dynamic)  : nested sequence of row sequences, e.g. {{1,0},{0,1}}.
 *   - Clipper points              : map {x, y}.
 *   - Polygon / Polygons          : sequence of points / sequence of polygons.
 *
 * Include this header in every translation unit that wraps one of the registered types in
 * an `AnyObject` so the field specializations stay ODR-consistent.
 */

namespace HsBa::Slicer::Utils
{
namespace hsba_detail
{
// Install identity destroy/copy/move hooks and reset the reflection tables for a concrete type.
template <typename T>
inline void fill_basic_typeinfo(TypeInfo& info, std::string_view name)
{
    info.Name = name;
    info.destroy = [](void* data) { delete static_cast<T*>(data); };
    info.copy = [](const void* data) -> void* { return new T(*static_cast<const T*>(data)); };
    info.move = [](void* data) -> void* { return new T(std::move(*static_cast<T*>(data))); };
    info.fields.clear();
    info.methods.clear();
}

// Build a TypeInfo method that mirrors `AnyObject::cast<T>()`: it returns a non-owning view
// (flag == 0) of the stored value, so the typed object can be passed around without a copy and
// without a double free. Reached from Lua as `obj:invoke("cast_<Name>")`.
template <typename T>
inline TypeInfo::Method make_self_cast()
{
    return +[](void* obj, std::span<AnyObject>) -> AnyObject
    { return AnyObject(GetTypeInfo<T>(), static_cast<T*>(obj)); };
}
}  // namespace hsba_detail

// ---------------------------------------------------------------------------
// AnyObject type reflection for the common custom types.
//   * Reflected types expose their scalar components (x / y / (z) / (w)) as fields so
//     `AnyObject::ForeachField` can iterate them; this relies on the components being stored
//     contiguously, so a component's offset is i * sizeof(Scalar).
//   * Opaque types (matrices, polygon containers) only get a stable name.
//   * Every type registers a `cast_<Name>` method implementing `AnyObject::cast<T>()`.
// The `LuaName` argument must match the name used by the Lua table adapter in the .cpp so the
// reflected method and the `new_` / `cast_` globals stay in sync.
// ---------------------------------------------------------------------------
#define HSBA_DEFINE_REFLECTED_TYPEINFO(CleanName, LuaName, Type, Scalar, N)                                            \
    template <>                                                                                                        \
    inline TypeInfo* GetTypeInfo<Type>()                                                                               \
    {                                                                                                                  \
        static TypeInfo info;                                                                                          \
        hsba_detail::fill_basic_typeinfo<Type>(info, CleanName);                                                       \
        info.fields.emplace("x", std::make_pair(GetTypeInfo<Scalar>(), 0 * sizeof(Scalar)));                           \
        info.fields.emplace("y", std::make_pair(GetTypeInfo<Scalar>(), 1 * sizeof(Scalar)));                           \
        if constexpr ((N) >= 3)                                                                                        \
            info.fields.emplace("z", std::make_pair(GetTypeInfo<Scalar>(), 2 * sizeof(Scalar)));                       \
        if constexpr ((N) >= 4)                                                                                        \
            info.fields.emplace("w", std::make_pair(GetTypeInfo<Scalar>(), 3 * sizeof(Scalar)));                       \
        info.methods.emplace("cast_" LuaName, hsba_detail::make_self_cast<Type>());                                    \
        return &info;                                                                                                  \
    }

#define HSBA_DEFINE_OPAQUE_TYPEINFO(CleanName, LuaName, Type)                                                          \
    template <>                                                                                                        \
    inline TypeInfo* GetTypeInfo<Type>()                                                                               \
    {                                                                                                                  \
        static TypeInfo info;                                                                                          \
        hsba_detail::fill_basic_typeinfo<Type>(info, CleanName);                                                       \
        info.methods.emplace("cast_" LuaName, hsba_detail::make_self_cast<Type>());                                    \
        return &info;                                                                                                  \
    }

// Eigen fixed vectors (float / double / int): coefficients are contiguous.
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector2f", "Vector2f", Eigen::Vector2f, float, 2)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector3f", "Vector3f", Eigen::Vector3f, float, 3)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector4f", "Vector4f", Eigen::Vector4f, float, 4)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector2d", "Vector2d", Eigen::Vector2d, double, 2)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector3d", "Vector3d", Eigen::Vector3d, double, 3)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector4d", "Vector4d", Eigen::Vector4d, double, 4)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector2i", "Vector2i", Eigen::Vector2i, int, 2)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector3i", "Vector3i", Eigen::Vector3i, int, 3)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Vector4i", "Vector4i", Eigen::Vector4i, int, 4)

// Eigen quaternions: internal coefficient order is x, y, z, w.
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Quaternionf", "Quaternionf", Eigen::Quaternionf, float, 4)
HSBA_DEFINE_REFLECTED_TYPEINFO("Eigen::Quaterniond", "Quaterniond", Eigen::Quaterniond, double, 4)

// Clipper2 points: { x, y } stored contiguously.
HSBA_DEFINE_REFLECTED_TYPEINFO("Clipper2::Point2D", "Point2D", Point2D, double, 2)
HSBA_DEFINE_REFLECTED_TYPEINFO("Clipper2::Point2", "Point2", Point2, long long, 2)

// Eigen matrices (fixed & dynamic): no contiguous scalar layout.
HSBA_DEFINE_OPAQUE_TYPEINFO("Eigen::Matrix2d", "Matrix2d", Eigen::Matrix2d)
HSBA_DEFINE_OPAQUE_TYPEINFO("Eigen::Matrix3d", "Matrix3d", Eigen::Matrix3d)
HSBA_DEFINE_OPAQUE_TYPEINFO("Eigen::Matrix4d", "Matrix4d", Eigen::Matrix4d)
HSBA_DEFINE_OPAQUE_TYPEINFO("Eigen::MatrixXf", "MatrixXf", Eigen::MatrixXf)
HSBA_DEFINE_OPAQUE_TYPEINFO("Eigen::MatrixXi", "MatrixXi", Eigen::MatrixXi)

// Clipper2 polygon containers.
HSBA_DEFINE_OPAQUE_TYPEINFO("Clipper2::PolygonD", "PolygonD", PolygonD)
HSBA_DEFINE_OPAQUE_TYPEINFO("Clipper2::PolygonsD", "PolygonsD", PolygonsD)
HSBA_DEFINE_OPAQUE_TYPEINFO("Clipper2::Polygon", "Polygon", Polygon)
HSBA_DEFINE_OPAQUE_TYPEINFO("Clipper2::Polygons", "Polygons", Polygons)

#undef HSBA_DEFINE_REFLECTED_TYPEINFO
#undef HSBA_DEFINE_OPAQUE_TYPEINFO
}  // namespace HsBa::Slicer::Utils

namespace HsBa::Slicer
{
/**
 * @brief Return Lua new/cast adapters for all registered common custom types.
 *
 * The adapters only cover the custom types (Eigen vectors / matrices / quaternions and the
 * Clipper2 geometry types); built-in scalar adapters are not included. Use this to merge the
 * custom types into an existing `RegisterAnyObject` call when scalar types are already
 * registered elsewhere.
 *
 * @return A vector of non-owning pointers to process-lifetime singleton adapters.
 */
HSBA_SLICER_LIB_API std::vector<LuaAnyObjectNewCastBase*> GetCommonAnyObjectTypes();

/**
 * @brief Register the built-in scalar adapters plus all common custom types into @p L.
 *
 * This is a convenience wrapper around `RegisterAnyObject`. Because `RegisterAnyObject`
 * (re)creates the global `AnyObject` table, this function must be the single entry point for
 * populating that table; do not call it after another `RegisterAnyObject` invocation, or the
 * previously registered globals will be replaced.
 *
 * @param L Lua state to register into.
 */
HSBA_SLICER_LIB_API void RegisterCommonAnyObjectTypes(lua_State* L);

/**
 * @brief Install the common AnyObject types into the pipeline Lua-registration pools.
 *
 * Pushes a single `LuaRegFunc` wrapper around `RegisterCommonAnyObjectTypes` into the
 * generic 2D / 3D / File pools (see `LuaAddFunction.hpp`), so every pipeline stage that
 * builds its Lua state from those pools (Slice, Support, Fill, SLS/SLA Output) can use the
 * registered types without stage-specific wiring. Thread-safe and idempotent: repeated
 * calls add no further entries, and the per-state guard inside
 * `RegisterCommonAnyObjectTypes` keeps stages that consume several pools (Support: 2D+3D,
 * SLA: 2D+File) from registering the same state twice.
 */
HSBA_SLICER_LIB_API void InstallCommonAnyObjectTypes();
}  // namespace HsBa::Slicer

#endif  // HSBA_SLICER_LUA_COMMON_TYPES_HPP
