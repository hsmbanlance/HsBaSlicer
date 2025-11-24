#pragma once
#ifndef HSBA_SLICER_FLOATPOLYGONS_HPP
#define HSBA_SLICER_FLOATPOLYGONS_HPP

#include <clipper2/clipper.h>
#include <clipper2/clipper.offset.h>

#include "IntPolygon.hpp"
#include <string>

namespace HsBa::Slicer
{
	using Point2D = Clipper2Lib::PointD;
	using PolygonD = Clipper2Lib::PathD;
	using PolygonsD = Clipper2Lib::PathsD;

	PolygonsD MakeSimple(const PolygonD& p, double epsilon = 1e-6);
	PolygonsD MakeSimple(const PolygonsD& ps, double epsilon = 1e-6);

	PolygonsD Union(const PolygonD& left, const PolygonD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	PolygonsD Intersection(const PolygonD& left, const PolygonD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	PolygonsD Difference(const PolygonD& left, const PolygonD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	PolygonsD Xor(const PolygonD& left, const PolygonD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);

	PolygonsD Union(const PolygonsD& left, const PolygonsD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	PolygonsD Intersection(const PolygonsD& left, const PolygonsD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	PolygonsD Difference(const PolygonsD& left, const PolygonsD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);
	PolygonsD Xor(const PolygonsD& left, const PolygonsD& right,
		Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd
	);

	double Area(const PolygonD& p);
	double Area(const PolygonsD& ps);

	// Image IO functions are declared in ImageToPolygons.hpp

	Polygon Integerization(const PolygonD& poly);
	Polygons Integerization(const PolygonsD& polys);

	PolygonD UnIntegerization(const Polygon& poly);
	PolygonsD UnIntegerization(const Polygons& polys);
}// namespace HsBa::Slicer
#endif // !HSBA_SLICER_FLOATPOLYGONS_HPP

template<>
struct std::hash<HsBa::Slicer::PolygonD>
{
	std::size_t operator()(const HsBa::Slicer::PolygonD& p) const;
};

template<>
struct std::hash<HsBa::Slicer::PolygonsD>
{
	std::size_t operator()(const HsBa::Slicer::PolygonsD& p) const;
};


