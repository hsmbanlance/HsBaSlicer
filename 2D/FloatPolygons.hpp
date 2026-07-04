#pragma once
#ifndef HSBA_SLICER_FLOATPOLYGONS_HPP
#define HSBA_SLICER_FLOATPOLYGONS_HPP

#include <clipper2/clipper.h>
#include <clipper2/clipper.offset.h>

#include "IntPolygon.hpp"
#include <string>
#include <string_view>

namespace HsBa::Slicer
{
/** @brief 2D point with double precision coordinates. */
using Point2D = Clipper2Lib::PointD;

/** @brief Polygon (path) with double precision coordinates. */
using PolygonD = Clipper2Lib::PathD;

/** @brief Collection of polygons with double precision coordinates. */
using PolygonsD = Clipper2Lib::PathsD;

/**
 * @brief Simplify a single polygon by removing collinear points.
 * @param p Input polygon.
 * @param epsilon Tolerance for simplification (default: 1e-6).
 * @return Simplified polygons.
 */
PolygonsD MakeSimple(const PolygonD& p, double epsilon = 1e-6);

/**
 * @brief Simplify multiple polygons by removing collinear points.
 * @param ps Input polygons.
 * @param epsilon Tolerance for simplification (default: 1e-6).
 * @return Simplified polygons.
 */
PolygonsD MakeSimple(const PolygonsD& ps, double epsilon = 1e-6);

/**
 * @brief Compute union of two polygons.
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from union operation.
 */
PolygonsD Union(const PolygonD& left, const PolygonD& right,
                Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute intersection of two polygons.
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from intersection operation.
 */
PolygonsD Intersection(const PolygonD& left, const PolygonD& right,
                       Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute difference of two polygons (left - right).
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from difference operation.
 */
PolygonsD Difference(const PolygonD& left, const PolygonD& right,
                     Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute XOR (symmetric difference) of two polygons.
 * @param left First polygon.
 * @param right Second polygon.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from XOR operation.
 */
PolygonsD Xor(const PolygonD& left, const PolygonD& right,
              Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute union of two polygon sets.
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from union operation.
 */
PolygonsD Union(const PolygonsD& left, const PolygonsD& right,
                Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute intersection of two polygon sets.
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from intersection operation.
 */
PolygonsD Intersection(const PolygonsD& left, const PolygonsD& right,
                       Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute difference of two polygon sets (left - right).
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from difference operation.
 */
PolygonsD Difference(const PolygonsD& left, const PolygonsD& right,
                     Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Compute XOR (symmetric difference) of two polygon sets.
 * @param left First set of polygons.
 * @param right Second set of polygons.
 * @param fill_rule Fill rule for boolean operation (default: EvenOdd).
 * @return Resulting polygons from XOR operation.
 */
PolygonsD Xor(const PolygonsD& left, const PolygonsD& right,
              Clipper2Lib::FillRule fill_rule = Clipper2Lib::FillRule::EvenOdd);

/**
 * @brief Calculate the area of a polygon.
 * @param p Input polygon.
 * @return Area value (positive for clockwise, negative for counter-clockwise).
 */
double Area(const PolygonD& p);

/**
 * @brief Calculate the total area of multiple polygons.
 * @param ps Input polygons.
 * @return Total area value.
 */
double Area(const PolygonsD& ps);

/**
 * @brief Create a rectangle polygon.
 * @param x X coordinate of bottom-left corner.
 * @param y Y coordinate of bottom-left corner.
 * @param width Width of the rectangle.
 * @param height Height of the rectangle.
 * @return Rectangle polygon.
 */
PolygonD MakeRectangle(double x, double y, double width, double height);

/**
 * @brief Create a circle polygon.
 * @param cx X coordinate of center.
 * @param cy Y coordinate of center.
 * @param radius Radius of the circle.
 * @param segments Number of segments to approximate the circle (default: 64).
 * @return Circle polygon.
 */
PolygonD MakeCircle(double cx, double cy, double radius, int segments = 64);

/**
 * @brief Create an ellipse polygon.
 * @param cx X coordinate of center.
 * @param cy Y coordinate of center.
 * @param rx Semi-major axis (X radius).
 * @param ry Semi-minor axis (Y radius).
 * @param segments Number of segments to approximate the ellipse (default: 64).
 * @param rotation Rotation angle in radians (default: 0.0).
 * @return Ellipse polygon.
 */
PolygonD MakeEllipse(double cx, double cy, double rx, double ry, int segments = 64, double rotation = 0.0);

/**
 * @brief Create a regular polygon.
 * @param cx X coordinate of center.
 * @param cy Y coordinate of center.
 * @param radius Distance from center to vertices.
 * @param sides Number of sides.
 * @param rotation Rotation angle in radians (default: 0.0).
 * @return Regular polygon.
 */
PolygonD MakeRegularPolygon(double cx, double cy, double radius, int sides, double rotation = 0.0);

/**
 * @brief Convert text to polygons using a font file.
 * @param utf8_text UTF-8 encoded text string.
 * @param font_file Path to the font file (TTF/OTF).
 * @param font_size Font size in points.
 * @param x X position offset (default: 0.0).
 * @param y Y position offset (default: 0.0).
 * @param curve_segments Number of segments per curve (default: 8).
 * @return Polygons representing the text outlines.
 */
PolygonsD TextToPolygons(const std::string& utf8_text, const std::string& font_file, double font_size, double x = 0.0,
                         double y = 0.0, int curve_segments = 8);

// Image IO functions are declared in ImageToPolygons.hpp

/**
 * @brief Convert double-precision polygon to integer polygon.
 * @param poly Input polygon with double coordinates.
 * @return Integerized polygon.
 */
Polygon Integerization(const PolygonD& poly);

/**
 * @brief Convert double-precision polygons to integer polygons.
 * @param polys Input polygons with double coordinates.
 * @return Integerized polygons.
 */
Polygons Integerization(const PolygonsD& polys);

/**
 * @brief Convert integer polygon to double-precision polygon.
 * @param poly Input integer polygon.
 * @return De-integerized polygon with double coordinates.
 */
PolygonD UnIntegerization(const Polygon& poly);

/**
 * @brief Convert integer polygons to double-precision polygons.
 * @param polys Input integer polygons.
 * @return De-integerized polygons with double coordinates.
 */
PolygonsD UnIntegerization(const Polygons& polys);

#ifdef HSBA_POLYGON_DUMP
/**
 * @brief Dump a polygon to SVG file for visualization (debug only).
 * @param p Polygon to dump.
 * @param filename Output SVG filename.
 * @param close_path Whether to close the path (default: true).
 */
void DumpPolygon(const PolygonD& p, std::string_view filename, bool close_path = true);

/**
 * @brief Dump multiple polygons to SVG file for visualization (debug only).
 * @param ps Polygons to dump.
 * @param filename Output SVG filename.
 * @param close_path Whether to close the paths (default: true).
 */
void DumpPolygons(const PolygonsD& ps, std::string_view filename, bool close_path = true);
#endif
}  // namespace HsBa::Slicer

/**
 * @brief Hash specialization for PolygonD.
 */
template <>
struct std::hash<HsBa::Slicer::PolygonD>
{
    /**
     * @brief Compute hash value for a polygon.
     * @param p Input polygon.
     * @return Hash value.
     */
    std::size_t operator()(const HsBa::Slicer::PolygonD& p) const;
};

/**
 * @brief Hash specialization for PolygonsD.
 */
template <>
struct std::hash<HsBa::Slicer::PolygonsD>
{
    /**
     * @brief Compute hash value for a set of polygons.
     * @param p Input polygons.
     * @return Hash value.
     */
    std::size_t operator()(const HsBa::Slicer::PolygonsD& p) const;
};

#endif  // !HSBA_SLICER_FLOATPOLYGONS_HPP
