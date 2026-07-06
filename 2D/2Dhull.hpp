#pragma once
#ifndef HSBA_SLICER_2DHULL_HPP
#define HSBA_SLICER_2DHULL_HPP

#include "FloatPolygons.hpp"
#include "IntPolygon.hpp"

namespace HsBa::Slicer
{
/**
 * @brief Compute concave hull simulation for a polygon.
 * @param polygon Input polygon.
 * @param numAdditionalPoints Number of additional points to add for concavity.
 * @return Polygon representing the concave hull.
 */
Polygon ConcaveHullSimulation(const Polygon& polygon, int numAdditionalPoints);

/**
 * @brief Compute convex hull for a polygon.
 * @param polygon Input polygon.
 * @return Polygon representing the convex hull.
 */
Polygon ConvexHull(const Polygon& polygon);

/**
 * @brief Compute concave hull simulation for multiple polygons.
 * @param polygons Input polygons.
 * @param numAdditionalPoints Number of additional points to add for concavity.
 * @return Polygon representing the concave hull of all input polygons.
 */
Polygon ConcaveHullSimulation(const Polygons& polygons, int numAdditionalPoints);

/**
 * @brief Compute convex hull for multiple polygons.
 * @param polygons Input polygons.
 * @return Polygon representing the convex hull of all input polygons.
 */
Polygon ConvexHull(const Polygons& polygons);

/**
 * @brief Compute concave hull simulation for a double-precision polygon.
 * @param polygon Input polygon with double coordinates.
 * @param numAdditionalPoints Number of additional points to add for concavity.
 * @return PolygonD representing the concave hull.
 */
PolygonD ConcaveHullSimulation(const PolygonD& polygon, int numAdditionalPoints);

/**
 * @brief Compute convex hull for a double-precision polygon.
 * @param polygon Input polygon with double coordinates.
 * @return PolygonD representing the convex hull.
 */
PolygonD ConvexHull(const PolygonD& polygon);

/**
 * @brief Compute concave hull simulation for multiple double-precision polygons.
 * @param polygons Input polygons with double coordinates.
 * @param numAdditionalPoints Number of additional points to add for concavity.
 * @return PolygonD representing the concave hull of all input polygons.
 */
PolygonD ConcaveHullSimulation(const PolygonsD& polygons, int numAdditionalPoints);

/**
 * @brief Compute convex hull for multiple double-precision polygons.
 * @param polygons Input polygons with double coordinates.
 * @return PolygonD representing the convex hull of all input polygons.
 */
PolygonD ConvexHull(const PolygonsD& polygons);
}  // namespace HsBa::Slicer
#endif  // !HSBA_SLICER_2DHULL_HPP
