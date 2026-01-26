#pragma once
#ifndef HSBA_SLICER_POLYGONFILL_HPP
#define HSBA_SLICER_POLYGONFILL_HPP

#include "IntPolygon.hpp"
#include "FloatPolygons.hpp"
#include <functional>

// forward-declare lua state to avoid including lua.hpp in this header
struct lua_State;

namespace HsBa::Slicer
{
    // only one outer polygon and multiple holes supported

    Polygons OffsetFill(const Polygons& poly, double spacing,
        Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);


    Polygons LineFill(const Polygons& poly, double spacing, double angle_deg,
        double lineThickness = 0.5);

    Polygons SimpleZigzagFill(const Polygons& poly, double spacing, double angle_deg,
        double lineThickness = 0.5);

    Polygons ZigzagFill(const Polygons& poly, double spacing, double angle_deg,
        double lineThickness = 0.5);

    enum class FillMode { Line, SimpleZigzag, Zigzag };

    Polygons CompositeOffsetFill(const Polygons& poly, double spacing,
        double offsetStep, int outwardCount, int inwardCount, FillMode mode,
        double angle_deg, double lineThickness = 0.5,
        Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);
    
    // offset frist, then fill last offset polygons
    Polygons HybridFill(const Polygons& poly, double spacing,
        double offsetStep, int outwardCount, int inwardCount, FillMode mode,
        double angle_deg, double lineThickness = 0.5,
        Clipper2Lib::JoinType join_type = Clipper2Lib::JoinType::Square);

    Polygons LuaCustomFill(const Polygons& poly, const std::string& scriptPath, const std::string& functionName = "generate_fill",
        double lineThickness = 0.5, const std::function<void(lua_State*)>& lua_reg = {});

    Polygons LuaCustomFillString(const Polygons& poly, const std::string& script, const std::string& functionName = "generate_fill",
        double lineThickness = 0.5, const std::function<void(lua_State*)>& lua_reg = {});
}

#endif // !HSBA_SLICER_POLYGONFILL_HPP
