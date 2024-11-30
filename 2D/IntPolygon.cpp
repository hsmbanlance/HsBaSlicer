#include "IntPolygon.hpp"

#include <boost/container_hash/hash.hpp>

namespace HsBa::Slicer
{
	Polygons MakeSimple(const Polygon& p,double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(Polygons{ p }, epsilon);
	}
	Polygons MakeSimple(const Polygons& ps, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(ps, epsilon);
	}

	Polygons Union(const Polygon& left, const Polygon& right,
		Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(Polygons{ left }, Polygons{ right }, fill_rule);
	}
	Polygons Intersection(const Polygon& left, const Polygon& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(Polygons{ left }, Polygons{ right }, fill_rule);
	}
	Polygons Difference(const Polygon& left, const Polygon& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(Polygons{ left }, Polygons{ right }, fill_rule);
	}
	Polygons Xor(const Polygon& left, const Polygon& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(Polygons{ left }, Polygons{ right }, fill_rule);
	}

	Polygons Union(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(left, right, fill_rule);
	}
	Polygons Intersection(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(left, right, fill_rule);
	}
	Polygons Difference(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(left, right, fill_rule);
	}
	Polygons Xor(const Polygons& left, const Polygons& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(left, right, fill_rule);
	}

	Polygons Offset(const Polygon& p, double delta, Clipper2Lib::JoinType join_type, Clipper2Lib::EndType end_type)
	{
		Polygons res;
		Clipper2Lib::ClipperOffset offset;
		offset.AddPath(p, join_type, end_type);
		offset.Execute(delta, res);
		return res;
	}
	Polygons Offset(const Polygons& ps, double delta, Clipper2Lib::JoinType join_type, Clipper2Lib::EndType end_type)
	{
		Polygons res;
		Clipper2Lib::ClipperOffset offset;
		offset.AddPaths(ps, join_type, end_type);
		offset.Execute(delta, res);
		return res;
	}

	double Area(const Polygon& p)
	{
		return Clipper2Lib::Area(p);
	}
	double Area(const Polygons& ps)
	{
		return Clipper2Lib::Area(ps);
	}
}// namespace HsBa::Slicer

std::size_t std::hash<HsBa::Slicer::Polygon>::operator()(const HsBa::Slicer::Polygon& p)
{
	size_t seed = 0;
	for (const auto& point : p)
	{
		boost::hash_combine(seed, point.x);
		boost::hash_combine(seed, point.y);
	}
	return seed;
}

std::size_t std::hash<HsBa::Slicer::Polygons>::operator()(const HsBa::Slicer::Polygons& p)
{
	size_t seed = 0;
	for (const auto& poly : p)
	{
		std::hash<HsBa::Slicer::Polygon> hasher;
		boost::hash_combine(seed, hasher(poly));
	}
	return seed;
}