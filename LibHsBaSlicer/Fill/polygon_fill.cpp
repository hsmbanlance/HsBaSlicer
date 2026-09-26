#include "polygon_fill.hpp"

#include "2D/PolygonFill.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"
#include "LibHsBaSlicer/Path/path_optimizer.hpp"

namespace HsBa::Slicer
{
HSBA_SLICER_LIB_API Polygons FillPolygon(const Polygons& poly, double spacing, FillMode mode, double angle_deg)
{
    switch (mode)
    {
    case FillMode::Line:
        return LineFill(poly, spacing, angle_deg);
    case FillMode::SimpleZigzag:
        return SimpleZigzagFill(poly, spacing, angle_deg);
    case FillMode::Zigzag:
        return ZigzagFill(poly, spacing, angle_deg);
    default:
        return ZigzagFill(poly, spacing, angle_deg);
    }
}

HSBA_SLICER_LIB_API Polygons FillWithBorder(const Polygons& poly, double spacing, int border_count, FillMode fill_mode,
                                            double angle_deg)
{
    // 使用CompositeOffsetFill：边框偏移 + 内部填充
    // outwardCount=0 表示不向外偏移，inwardCount=border_count 向内偏移生成边框
    return CompositeOffsetFill(poly, spacing, spacing, 0, border_count, fill_mode, angle_deg);
}

HSBA_SLICER_LIB_API Polygons LuaCustomFillByFile(const Polygons& poly, const std::string& scriptPath,
                                                 const std::string& functionName, double lineThickness)
{
    // Compose external 2D functions and the path optimize functions (PathOptimize)
    // into lua_reg callback, so fill scripts can optimize regions before path output
    std::function<void(lua_State*)> reg = [](lua_State* L)
    {
        for (auto& f : Get2DFunctions())
            f(L);
        RegisterLuaPathOptimizeFunctions(L);
    };
    return LuaCustomFill(poly, scriptPath, functionName, lineThickness, reg);
}

}  // namespace HsBa::Slicer
