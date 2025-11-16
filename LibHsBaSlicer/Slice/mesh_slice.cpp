#include "mesh_slice.hpp"

namespace HsBa::Slicer
{
	Polygons Slice(const IModel& model, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->Slice(height);
	}
	UnSafePolygons UnSafeSlice(const IModel& model, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->UnSafeSlice(height);
	}

	Polygons SliceLua(const IModel& model, const std::string& script, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->SliceLua(script, height);
	}

	UnSafePolygons UnSafeSliceLua(const IModel& model, const std::string& script, const float height)
	{
		auto topo_mesh = std::make_unique<FullTopoModel>(FullTopoModel(model));
		return topo_mesh->UnSafeSliceLua(script, height);
	}
}// namespace HsBa::Slicer
