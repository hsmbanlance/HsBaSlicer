#pragma once
#ifndef HSBA_SLICER_INTPOLYGON_HPP
#define HSBA_SLICER_INTPOLYGON_HPP

#include <clipper2/clipper.h>
#include <clipper2/clipper.offset.h>

namespace HsBa::Slicer
{
	constexpr double integerization = 1e6;

	using Point2 = Clipper2Lib::Point64;
	using Polygon = Clipper2Lib::Path64;
	using Polygons = Clipper2Lib::Paths64;

	Polygons MakeSimple(const Polygon& p, double epsilon = 1e-3);
	Polygons MakeSimple(const Polygons& ps, double epsilon = 1e-3);

	std::vector<Polygons> MakeSimpleAndSplit(const Polygon& p, double epsilon = 1e-3);

	Polygons Union(const Polygon& left, const Polygon& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	Polygons Intersection(const Polygon& left, const Polygon& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	Polygons Difference(const Polygon& left, const Polygon& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	Polygons Xor(const Polygon& left, const Polygon& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);

	Polygons Union(const Polygons& left, const Polygons& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	Polygons Intersection(const Polygons& left, const Polygons& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	Polygons Difference(const Polygons& left, const Polygons& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	Polygons Xor(const Polygons& left, const Polygons& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);

	Polygons Offset(const Polygon& p, double delta,
		Clipper2Lib::JoinType join_type=Clipper2Lib::JoinType::Square,
		Clipper2Lib::EndType end_type=Clipper2Lib::EndType::Polygon
		);
	Polygons Offset(const Polygons& ps, double delta,
		Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square,
		Clipper2Lib::EndType end_type = Clipper2Lib::EndType::Polygon
	);

	Clipper2Lib::PointInPolygonResult PointInPolygons(const Clipper2Lib::Point64& point, const Polygons& polys, bool isEvenOdd = true);

	double Area(const Polygon& p);
	double Area(const Polygons& ps);

}// namespace HsBa::Slicer
#endif // !HSBA_SLICER_INTPOLYGON_HPP

template<>
struct std::hash<HsBa::Slicer::Polygon>
{
	std::size_t operator() (const HsBa::Slicer::Polygon& p) const;
};

template<>
struct std::hash<HsBa::Slicer::Polygons>
{
	std::size_t operator()(const HsBa::Slicer::Polygons& p) const;
};
