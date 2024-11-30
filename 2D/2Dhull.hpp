#pragma once
#ifndef HSBA_SLICER_2DHULL_HPP
#define HSBA_SLICER_2DHULL_HPP

#include "IntPolygon.hpp"
#include "FloatPolygons.hpp"

namespace HsBa::Slicer
{
	Polygon ConcaveHullSimulation(const Polygon& polygon, int numAdditionalPoints);
	Polygon ConvexHull(const Polygon& polygon);
	Polygon ConcaveHullSimulation(const Polygons& polygons, int numAdditionalPoints);
    Polygon ConvexHull(const Polygons& polygons);

	PolygonD ConcaveHullSimulation(const PolygonD& polygon, int numAdditionalPoints);
	PolygonD ConvexHull(const PolygonD& polygon);
	PolygonD ConcaveHullSimulation(const PolygonsD& polygons, int numAdditionalPoints);
	PolygonD ConvexHull(const PolygonsD& polygons);
}// namespace HsBa::Slicer
#endif // !HSBA_SLICER_2DHULL_HPP
