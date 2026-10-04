#include "mesh_slice.hpp"

#include <exception>
#include <memory>
#include <string>

#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "base/error.hpp"

namespace HsBa::Slicer
{
namespace
{
// Choke-point translation for the slicing subsystem.
//
// All entry points below delegate to FullTopoModel / the concrete model mesh
// extraction, whose geometry kernels (CGAL, and OCCT during tessellation) may
// report failures as exception types outside the project hierarchy (CGAL::Exception,
// Standard_Failure, or even a raw std::runtime_error). Such types must never
// propagate across the extern "C" pipeline boundary, so each exported function
// funnels through this wrapper: project exceptions (RuntimeError-derived) pass
// through unchanged, while any foreign exception becomes the project's IOError.
template <typename Fn>
auto GuardSlice(Fn&& fn) -> decltype(fn())
{
    try
    {
        return fn();
    }
    catch (const RuntimeError&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw IOError(std::string("Slicing failed: ") + e.what());
    }
}
}  // namespace

HSBA_SLICER_LIB_API Polygons Slice(const IModel& model, const float height, double tolerance)
{
    return GuardSlice(
        [&]
        {
            auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
            return topo_mesh->Slice(height, tolerance);
        });
}

HSBA_SLICER_LIB_API UnSafePolygons UnSafeSlice(const IModel& model, const float height, double tolerance)
{
    return GuardSlice(
        [&]
        {
            auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
            return topo_mesh->UnSafeSlice(height, tolerance);
        });
}

HSBA_SLICER_LIB_API Polygons SliceLua(const IModel& model, const std::string& script, const float height)
{
    return GuardSlice(
        [&]
        {
            auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
            return topo_mesh->SliceLua(script, height, Get3DFunctions());
        });
}

HSBA_SLICER_LIB_API UnSafePolygons UnSafeSliceLua(const IModel& model, const std::string& script, const float height)
{
    return GuardSlice(
        [&]
        {
            auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
            return topo_mesh->UnSafeSliceLua(script, height, Get3DFunctions());
        });
}

HSBA_SLICER_LIB_API PolygonsD NormalizeUnSafePolygons(const UnSafePolygons& unsafe_polys)
{
    Polygons int_polys;
    int_polys.reserve(unsafe_polys.size() * 2);
    for (const auto& up : unsafe_polys)
    {
        // For FDM/FFF only closed polygons are valid; skip open polylines
        if (!up.closed || up.path.size() < 3)
        {
            continue;
        }
        auto normalized = NormalizeToSimplePolygons(up.path);
        for (const auto& simple_poly : normalized)
        {
            int_polys.push_back(simple_poly);
        }
    }
    return UnIntegerization(int_polys);
}

HSBA_SLICER_LIB_API std::shared_ptr<FullTopoModel> BuildSliceTopology(const IModel& model)
{
    // Build the full topology once; the resulting object owns its own vertex/face
    // copies and no longer depends on the source model's lifetime.
    return GuardSlice([&] { return std::make_shared<FullTopoModel>(model); });
}

HSBA_SLICER_LIB_API PolygonsD SliceLayer(const FullTopoModel& topo, float z, double tolerance)
{
    return GuardSlice([&] { return NormalizeUnSafePolygons(topo.UnSafeSlice(z, tolerance)); });
}

}  // namespace HsBa::Slicer
