/** @file 2Dhull.cpp
 * @brief Implementations of convex and concave hull generation for polygons.
 * @author HsBa
 */
#include "2Dhull.hpp"

#include <algorithm>
#include <utility>

#include <clipper2/clipper.h>

namespace HsBa::Slicer
{
namespace
{
double CrossProduct(const Point2& o, const Point2& a, const Point2& b)
{
    return Clipper2Lib::CrossProduct(a - o, b - o);
}

double CrossProductD(const Point2D& o, const Point2D& a, const Point2D& b)
{
    return Clipper2Lib::CrossProduct(a - o, b - o);
}

bool CmpFromAngleDistance(const Point2& left, const Point2& right)
{
    double const cp = Clipper2Lib::CrossProduct(left, right);
    if (cp == 0)
    {
        return (left.x * left.x + left.y * left.y) < (right.x * right.x + right.y * right.y);
    }
    return cp > 0;
}

bool CmpFromAngleDistanceD(const Point2D& left, const Point2D& right)
{
    double const cp = Clipper2Lib::CrossProduct(left, right);
    if (cp == 0)
    {
        return (left.x * left.x + left.y * left.y) < (right.x * right.x + right.y * right.y);
    }
    return cp > 0;
}
}  // namespace

Polygon ConcaveHullSimulation(const Polygon& polygon, int numAdditionalPoints)
{
    const auto convexHullPoints = ConvexHull(polygon);
    Polygon concaveHullPoints;
    const size_t size = convexHullPoints.size();
    if (size == 0)
    {
        return concaveHullPoints;
    }

    // Each hull edge contributes its start vertex plus 'numAdditionalPoints'
    // interpolated points, so the result holds size * (numAdditionalPoints + 1)
    // vertices. The output is a separate vector: the previous implementation
    // aliased it to convexHullPoints and pushed into it while iterating, which
    // kept growing the loop bound and never terminated for numAdditionalPoints
    // >= 1.
    concaveHullPoints.reserve(size * (static_cast<size_t>(numAdditionalPoints) + 1));
    for (size_t i = 0; i != size; ++i)
    {
        size_t const j = (i + 1) % size;
        for (int k = 0; k <= numAdditionalPoints; ++k)
        {
            double const t = static_cast<double>(k) / (numAdditionalPoints + 1);
            double const xi = static_cast<double>(convexHullPoints[i].x);
            double const xj = static_cast<double>(convexHullPoints[j].x);
            double const yi = static_cast<double>(convexHullPoints[i].y);
            double const yj = static_cast<double>(convexHullPoints[j].y);
            Point2 const newPoint(xi + t * (xj - xi), yi + t * (yj - yi));
            concaveHullPoints.push_back(newPoint);
        }
    }

    return concaveHullPoints;
}
Polygon ConvexHull(const Polygon& polygon)
{
    size_t const size = polygon.size();
    if (size <= 3)
    {
        return polygon;
    }
    Polygon points = polygon;
    size_t minIndex = 0;
    for (size_t i = 1; i != size; ++i)
    {
        if (points[i].y < points[minIndex].y || (points[i].y == points[minIndex].y && points[i].x < points[minIndex].x))
        {
            minIndex = i;
        }
    }
    std::swap(points[0], points[minIndex]);


    std::sort(points.begin() + 1, points.end(), CmpFromAngleDistance);


    Polygon hull;
    hull.push_back(points[0]);
    hull.push_back(points[1]);
    for (int i = 2; std::cmp_not_equal(i, size); ++i)
    {
        while (hull.size() > 1 && CrossProduct(hull[hull.size() - 2], hull.back(), points[i]) <= 0)
        {
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
    const auto convexHullPoints = ConvexHull(polygon);
    PolygonD concaveHullPoints;
    const size_t size = convexHullPoints.size();
    if (size == 0)
    {
        return concaveHullPoints;
    }

    // See the integer overload: the output must be a separate vector so the
    // interpolation loop is bounded by the fixed convex-hull size.
    concaveHullPoints.reserve(size * (static_cast<size_t>(numAdditionalPoints) + 1));
    for (size_t i = 0; i != size; ++i)
    {
        size_t const j = (i + 1) % size;
        for (int k = 0; k <= numAdditionalPoints; ++k)
        {
            double const t = static_cast<double>(k) / (numAdditionalPoints + 1);
            Point2D const newPoint(convexHullPoints[i].x + t * (convexHullPoints[j].x - convexHullPoints[i].x),
                                   convexHullPoints[i].y + t * (convexHullPoints[j].y - convexHullPoints[i].y));
            concaveHullPoints.push_back(newPoint);
        }
    }

    return concaveHullPoints;
}
PolygonD ConvexHull(const PolygonD& polygon)
{
    size_t const size = polygon.size();
    if (size <= 3)
    {
        return polygon;
    }
    PolygonD points = polygon;
    size_t minIndex = 0;
    for (size_t i = 1; i != size; ++i)
    {
        if (points[i].y < points[minIndex].y || (points[i].y == points[minIndex].y && points[i].x < points[minIndex].x))
        {
            minIndex = i;
        }
    }
    std::swap(points[0], points[minIndex]);


    std::sort(points.begin() + 1, points.end(), CmpFromAngleDistanceD);


    PolygonD hull;
    hull.push_back(points[0]);
    hull.push_back(points[1]);
    for (int i = 2; std::cmp_not_equal(i, size); ++i)
    {
        while (hull.size() > 1 && CrossProductD(hull[hull.size() - 2], hull.back(), points[i]) <= 0)
        {
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
}  // namespace HsBa::Slicer