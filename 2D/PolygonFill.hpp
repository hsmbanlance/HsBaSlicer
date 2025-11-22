#pragma once
#ifndef HSBA_SLICER_POLYGONFILL_HPP
#define HSBA_SLICER_POLYGONFILL_HPP

#include "IntPolygon.hpp"
#include "FloatPolygons.hpp"

namespace HsBa::Slicer
{
    Polygons OffsetFill(const Polygon& poly, double spacing,
        Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);


    Polygons LineFill(const Polygon& poly, double spacing, double angle_deg,
        double lineThickness = 0.5);

    Polygons SimpleZigzagFill(const Polygon& poly, double spacing, double angle_deg,
        double lineThickness = 0.5);

    Polygons ZigzagFill(const Polygon& poly, double spacing, double angle_deg,
        double lineThickness = 0.5);

    enum class FillMode { Line, SimpleZigzag, Zigzag };

    Polygons CompositeOffsetFill(const Polygon& poly, double spacing,
        double offsetStep, int outwardCount, int inwardCount, FillMode mode,
        double angle_deg, double lineThickness = 0.5,
        Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);
    
    // offset frist, then fill last offset polygons
    Polygons HybridFill(const Polygon& poly, double spacing,
        double offsetStep, int outwardCount, int inwardCount, FillMode mode,
        double angle_deg, double lineThickness = 0.5,
        Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);

    Polygons LuaCustomFill(const Polygon& poly, const std::string& scriptPath, const std::string& functionName = "generate_fill",
        double lineThickness = 0.5);
}

#endif // !HSBA_SLICER_POLYGONFILL_HPP
