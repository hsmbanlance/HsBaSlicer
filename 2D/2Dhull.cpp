#include "2Dhull.hpp"

#include <algorithm>

#include <clipper2/clipper.h>

namespace HsBa::Slicer
{
	static double CrossProduct(const Point2& o, const Point2& a, const Point2& b)
	{
		return Clipper2Lib::CrossProduct(a - o, b - o);
	}

	static double CrossProductD(const Point2D& o, const Point2D& a, const Point2D& b)
	{
		return Clipper2Lib::CrossProduct(a - o, b - o);
	}

	static bool CmpFromAngleDistance(const Point2& left, const Point2& right)
	{
		double cp = Clipper2Lib::CrossProduct(left, right);
		if (cp == 0) {
			return (left.x * left.x + left.y * left.y) < (right.x * right.x + right.y * right.y);
		}
		return cp > 0;
	}

	static bool CmpFromAngleDistanceD(const Point2D& left, const Point2D& right)
	{
		double cp = Clipper2Lib::CrossProduct(left, right);
		if (cp == 0) {
			return (left.x * left.x + left.y * left.y) < (right.x * right.x + right.y * right.y);
		}
		return cp > 0;
	}

	Polygon ConcaveHullSimulation(const Polygon& polygon, int numAdditionalPoints)
	{

		auto convexHullPoints = ConvexHull(polygon);

		auto& concaveHullPoints = convexHullPoints;

		for (size_t i = 0; i != convexHullPoints.size(); ++i) {
			size_t j = (i + 1) % convexHullPoints.size();

			double length = std::sqrt(std::pow(convexHullPoints[j].x - convexHullPoints[i].x, 2) +
				std::pow(convexHullPoints[j].y - convexHullPoints[i].y, 2));

			for (int k = 1; k <= numAdditionalPoints; ++k) {
				double t = static_cast<double>(k) / (numAdditionalPoints + 1);
				Point2 newPoint(convexHullPoints[i].x + t * (convexHullPoints[j].x - convexHullPoints[i].x),
					convexHullPoints[i].y + t * (convexHullPoints[j].y - convexHullPoints[i].y));
				concaveHullPoints.push_back(newPoint);
			}
		}

		return concaveHullPoints;
	}
	Polygon ConvexHull(const Polygon& polygon)
	{
		size_t size = polygon.size();
		if (size <= 3)
		{
			return polygon;
		}
		Polygon points = polygon;
		size_t minIndex = 0;
		for (size_t i = 1; i != size; ++i) {
			if (points[i].y < points[minIndex].y ||
				(points[i].y == points[minIndex].y && points[i].x < points[minIndex].x)) {
				minIndex = i;
			}
		}
		std::swap(points[0], points[minIndex]);


		std::sort(points.begin() + 1, points.end(), CmpFromAngleDistance);


		Polygon hull;
		hull.push_back(points[0]);
		hull.push_back(points[1]);
		for (int i = 2; i != size; ++i) {
			while (hull.size() > 1 && CrossProduct(hull[hull.size() - 2], hull.back(), points[i]) <= 0) {
				hull.pop_back();
			}
			hull.emplace_back(points[i]);
		}

		return hull;
	}

	Polygon ConcaveHullSimulation(const Polygons& polygons, int numAdditionalPoints)
	{
		Polygon points_set;
		for (const auto& poly : polygons)
		{
			points_set.insert(points_set.end(), poly.begin(), poly.end());
		}
		return ConcaveHullSimulation(points_set, numAdditionalPoints);
	}
	Polygon ConvexHull(const Polygons& polygons)
	{
		Polygon points_set;
		for (const auto& poly : polygons)
		{
			points_set.insert(points_set.end(), poly.begin(), poly.end());
		}
		return ConvexHull(points_set);
	}

	PolygonD ConcaveHullSimulation(const PolygonD& polygon, int numAdditionalPoints)
	{
		auto convexHullPoints = ConvexHull(polygon);

		auto& concaveHullPoints = convexHullPoints;

		for (size_t i = 0; i != convexHullPoints.size(); ++i) {
			size_t j = (i + 1) % convexHullPoints.size();

			double length = std::sqrt(std::pow(convexHullPoints[j].x - convexHullPoints[i].x, 2) +
				std::pow(convexHullPoints[j].y - convexHullPoints[i].y, 2));

			for (int k = 1; k <= numAdditionalPoints; ++k) {
				double t = static_cast<double>(k) / (numAdditionalPoints + 1);
				Point2D newPoint(convexHullPoints[i].x + t * (convexHullPoints[j].x - convexHullPoints[i].x),
					convexHullPoints[i].y + t * (convexHullPoints[j].y - convexHullPoints[i].y));
				concaveHullPoints.emplace_back(newPoint);
			}
		}

		return concaveHullPoints;
	}
	PolygonD ConvexHull(const PolygonD& polygon)
	{
		size_t size = polygon.size();
		if (size <= 3)
		{
			return polygon;
		}
		PolygonD points = polygon;
		size_t minIndex = 0;
		for (size_t i = 1; i != size; ++i) {
			if (points[i].y < points[minIndex].y ||
				(points[i].y == points[minIndex].y && points[i].x < points[minIndex].x)) {
				minIndex = i;
			}
		}
		std::swap(points[0], points[minIndex]);


		std::sort(points.begin() + 1, points.end(), CmpFromAngleDistanceD);


		PolygonD hull;
		hull.push_back(points[0]);
		hull.push_back(points[1]);
		for (int i = 2; i != size; ++i) {
			while (hull.size() > 1 && CrossProductD(hull[hull.size() - 2], hull.back(), points[i]) <= 0) {
				hull.pop_back();
			}
			hull.push_back(points[i]);
		}

		return hull;
	}

	PolygonD ConcaveHullSimulation(const PolygonsD& polygons, int numAdditionalPoints)
	{
		PolygonD points_set;
		for (const auto& poly : polygons)
		{
			points_set.insert(points_set.end(), poly.begin(), poly.end());
		}
		return ConcaveHullSimulation(points_set, numAdditionalPoints);
	}
	PolygonD ConvexHull(const PolygonsD& polygons)
	{
		PolygonD points_set;
		for (const auto& poly : polygons)
		{
			points_set.insert(points_set.end(), poly.begin(), poly.end());
		}
		return ConvexHull(points_set);
	}
}// namespace HsBa::Slicer