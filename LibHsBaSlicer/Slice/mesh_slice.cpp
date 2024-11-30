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
}// namespace HsBa::Slicer
