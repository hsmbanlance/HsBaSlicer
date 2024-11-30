#include "FloatPolygons.hpp"

#include <boost/container_hash/hash.hpp>

namespace HsBa::Slicer
{
	PolygonsD MakeSimple(const PolygonD& p, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(PolygonsD{ p }, epsilon);
	}
	PolygonsD MakeSimple(const PolygonsD& ps, double epsilon)
	{
		return Clipper2Lib::SimplifyPaths(ps, epsilon);
	}

	PolygonsD Union(const PolygonD& left, const PolygonD& right,
		Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}
	PolygonsD Intersection(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}
	PolygonsD Difference(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}
	PolygonsD Xor(const PolygonD& left, const PolygonD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(PolygonsD{ left }, PolygonsD{ right }, fill_rule);
	}

	PolygonsD Union(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Union(left, right, fill_rule);
	}
	PolygonsD Intersection(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Intersect(left, right, fill_rule);
	}
	PolygonsD Difference(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Difference(left, right, fill_rule);
	}
	PolygonsD Xor(const PolygonsD& left, const PolygonsD& right, Clipper2Lib::FillRule fill_rule)
	{
		return Clipper2Lib::Xor(left, right, fill_rule);
	}

	double Area(const PolygonD& p)
	{
		return Clipper2Lib::Area(p);
	}
	double Area(const PolygonsD& ps)
	{
		return Clipper2Lib::Area(ps);
	}

	Polygon Integerization(const PolygonD& poly)
	{
		Polygon res;
		for (const auto& p : poly)
		{
			res.emplace_back(Point2{ p.x * integerization,p.y * integerization });
		}
		return res;
	}
	Polygons Integerization(const PolygonsD& polys)
	{
		Polygons res;
		for (const auto& poly : polys)
		{
			res.emplace_back(Integerization(poly));
		}
		return res;
	}

	PolygonD UnIntegerization(const Polygon& poly)
	{
		PolygonD res;
		for (const auto& p : poly)
		{
			res.emplace_back(Point2D{ p.x / integerization,p.y / integerization });
		}
		return res;
	}

	PolygonsD UnIntegerization(const Polygons& polys)
	{
		PolygonsD res;
		for (const auto& poly : polys)
		{
			res.emplace_back(UnIntegerization(poly));
		}
		return res;
	}


}// namespace HsBa::Slicer

std::size_t std::hash<HsBa::Slicer::PolygonD>::operator()(const HsBa::Slicer::PolygonD& p)
{
	size_t seed = 0;
	for (const auto& point : p)
	{
		boost::hash_combine(seed, point.x);
		boost::hash_combine(seed, point.y);
	}
	return seed;
}

std::size_t std::hash<HsBa::Slicer::PolygonsD>::operator()(const HsBa::Slicer::PolygonsD& p)
{
	size_t seed = 0;
	for (const auto& poly : p)
	{
		std::hash<HsBa::Slicer::PolygonD> hasher;
		boost::hash_combine(seed, hasher(poly));
	}
	return seed;
}