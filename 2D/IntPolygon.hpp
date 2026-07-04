#pragma once
#ifndef HSBA_SLICER_INTPOLYGON_HPP
#define HSBA_SLICER_INTPOLYGON_HPP

#include <clipper2/clipper.h>
#include <clipper2/clipper.offset.h>
#include <string_view>

namespace HsBa::Slicer
{
/** @brief Scaling factor for converting between integer and floating-point coordinates. */
constexpr double integerization = 1e6;

/** @brief 2D point with 64-bit integer coordinates. */
using Point2 = Clipper2Lib::Point64;

/** @brief Polygon (path) with 64-bit integer coordinates. */
using Polygon = Clipper2Lib::Path64;

/** @brief Collection of polygons with 64-bit integer coordinates. */
using Polygons = Clipper2Lib::Paths64;

/**
 * @brief Simplify a single polygon by removing collinear points.
 * @param p Input polygon.
 * @param epsilon Tolerance for simplification (default: 1e-3).
 * @return Simplified polygons.
 */
Polygons MakeSimple(const Polygon& p, double epsilon = 1e-3);

/**
 * @brief Simplify multiple polygons by removing collinear points.
 * @param ps Input polygons.
 * @param epsilon Tolerance for simplification (default: 1e-3).
 * @return Simplified polygons.
 */
Polygons MakeSimple(const Polygons& ps, double epsilon = 1e-3);

/**
 * @brief Simplify a polygon and split into non-overlapping simple polygons.
 * @param p Input polygon.
 * @param epsilon Tolerance for simplification (default: 1e-3).
 * @return Vector of simplified and split polygons.
 */
std::vector<Polygons> MakeSimpleAndSplit(const Polygon& p, double epsilon = 1e-3);

/**
 * @brief Compute union of two polygons.
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from union operation.
 */
Polygons Union(const Polygon& left, const Polygon& right,
               Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute intersection of two polygons.
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from intersection operation.
 */
Polygons Intersection(const Polygon& left, const Polygon& right,
                      Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute difference of two polygons (left - right).
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from difference operation.
 */
Polygons Difference(const Polygon& left, const Polygon& right,
                    Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute XOR (symmetric difference) of two polygons.
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from XOR operation.
 */
Polygons Xor(const Polygon& left, const Polygon& right,
             Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute union of two polygon sets.
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from union operation.
 */
Polygons Union(const Polygons& left, const Polygons& right,
               Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute intersection of two polygon sets.
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from intersection operation.
 */
Polygons Intersection(const Polygons& left, const Polygons& right,
                      Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute difference of two polygon sets (left - right).
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from difference operation.
 */
Polygons Difference(const Polygons& left, const Polygons& right,
                    Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute XOR (symmetric difference) of two polygon sets.
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from XOR operation.
 */
Polygons Xor(const Polygons& left, const Polygons& right,
             Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Offset (inflate/deflate) a polygon by a specified distance.
 * @param p Input polygon.
 * @param delta Offset distance (positive for expansion, negative for contraction).
 * @param join_type Type of join for corners (default: Square).
 * @param end_type Type of end for open paths (default: Polygon).
 * @return Offset polygons.
 */
Polygons Offset(const Polygon& p, double delta, Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square,
                Clipper2Lib::EndType end_type = Clipper2Lib::EndType::Polygon);

/**
 * @brief Offset (inflate/deflate) multiple polygons by a specified distance.
 * @param ps Input polygons.
 * @param delta Offset distance (positive for expansion, negative for contraction).
 * @param join_type Type of join for corners (default: Square).
 * @param end_type Type of end for open paths (default: Polygon).
 * @return Offset polygons.
 */
Polygons Offset(const Polygons& ps, double delta, Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square,
                Clipper2Lib::EndType end_type = Clipper2Lib::EndType::Polygon);

/**
 * @brief Test if a point is inside polygons.
 * @param point Test point.
 * @param polys Input polygons.
 * @param isEvenOdd Use even-odd rule if true, non-zero winding rule if false (default: true).
 * @return Point-in-polygon result enumeration.
 */
Clipper2Lib::PointInPolygonResult PointInPolygons(const Clipper2Lib::Point64& point, const Polygons& polys,
                                                  bool isEvenOdd = true);

/**
 * @brief Calculate the area of a polygon.
 * @param p Input polygon.
 * @return Area value (positive for clockwise, negative for counter-clockwise).
 */
double Area(const Polygon& p);

/**
 * @brief Calculate the total area of multiple polygons.
 * @param ps Input polygons.
 * @return Total area value.
 */
double Area(const Polygons& ps);

#ifdef HSBA_POLYGON_DUMP
/**
 * @brief Dump a polygon to SVG file for visualization (debug only).
 * @param p Polygon to dump.
 * @param filename Output SVG filename.
 * @param close_path Whether to close the path (default: true).
 */
void DumpPolygon(const Polygon& p, std::string_view filename, bool close_path = true);

/**
 * @brief Dump multiple polygons to SVG file for visualization (debug only).
 * @param ps Polygons to dump.
 * @param filename Output SVG filename.
 * @param close_path Whether to close the paths (default: true).
 */
void DumpPolygons(const Polygons& ps, std::string_view filename, bool close_path = true);
#endif

}  // namespace HsBa::Slicer

/**
 * @brief Hash specialization for Polygon.
 */
template <>
struct std::hash<HsBa::Slicer::Polygon>
{
    /**
     * @brief Compute hash value for a polygon.
     * @param p Input polygon.
     * @return Hash value.
     */
    std::size_t operator()(const HsBa::Slicer::Polygon& p) const;
};

/**
 * @brief Hash specialization for Polygons.
 */
template <>
struct std::hash<HsBa::Slicer::Polygons>
{
    /**
     * @brief Compute hash value for a set of polygons.
     * @param p Input polygons.
     * @return Hash value.
     */
    std::size_t operator()(const HsBa::Slicer::Polygons& p) const;
};
#endif  // !HSBA_SLICER_INTPOLYGON_HPP