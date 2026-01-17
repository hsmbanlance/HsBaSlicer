#include "mesh_slice.hpp"

namespace HsBa::Slicer
{
	HSBA_SLICER_LIB_API Polygons Slice(const IModel& model, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->Slice(height);
	}
	HSBA_SLICER_LIB_API UnSafePolygons UnSafeSlice(const IModel& model, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->UnSafeSlice(height);
	}

	HSBA_SLICER_LIB_API Polygons SliceLua(const IModel& model, const std::string& script, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->SliceLua(script, height);
	}

	HSBA_SLICER_LIB_API UnSafePolygons UnSafeSliceLua(const IModel& model, const std::string& script, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->UnSafeSliceLua(script, height);
	}
}// namespace HsBa::Slicer
