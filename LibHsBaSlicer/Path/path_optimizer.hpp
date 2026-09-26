#pragma once
#ifndef HSBA_SLICER_LIB_PATH_OPTIMIZER_HPP
#define HSBA_SLICER_LIB_PATH_OPTIMIZER_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../export.h"
#include "2D/FloatPolygons.hpp"

// forward-declare lua state to avoid including lua.hpp in this header
struct lua_State;

namespace HsBa::Slicer
{
/**
 * @brief 路径输出前置优化器：把独立的多边形区域作为图顶点（AreaGraph 区域），
 *        求解使空走代价最小的区域访问顺序。
 *
 * 按执行时机分两种优化模式（同一优化器内不可混用）：
 * - 多边形模式（填充前）：区域输入为多边形本身（轮廓），全部顶点作为出入门禁，
 *   输出优化顺序的多边形集合，供后续填充使用；
 * - 填充结果模式（填充后）：区域输入为填充路径，支持多点折线，每条折线的首/尾端点作为门禁，
 *   输出完整填充路径。
 *
 * 建模方式：
 * - 每个区域（region）是图中的一个面积顶点，门禁为候选出入门点；
 * - 区域内部门禁间代价为门禁直线距离（区域内移动），区域间路由代价为门禁直线距离（空走）；
 * - 通过遗传 TSP 求解区域访问顺序，区域内按最近邻贪心编排出入顺序。
 */
class HSBA_SLICER_LIB_API RegionPathOptimizer
{
public:
    RegionPathOptimizer();
    ~RegionPathOptimizer();
    RegionPathOptimizer(const RegionPathOptimizer&) = delete;
    RegionPathOptimizer& operator=(const RegionPathOptimizer&) = delete;

    /**
     * @brief 添加一个基于填充结果的区域（填充后优化，支持多点折线）。
     * @param regionId 区域唯一标识。
     * @param paths 该区域的填充路径集合（每条为多点折线）。
     * @note 不可与 addPolygonRegion 在同一优化器内混用。
     */
    void addRegion(int regionId, const PolygonsD& paths);

    /**
     * @brief 添加一个基于多边形本身的区域（填充前优化）。
     * @param regionId 区域唯一标识。
     * @param polygons 该区域包含的多边形（轮廓）集合，全部顶点作为出入门禁。
     * @note 不可与 addRegion 在同一优化器内混用。
     */
    void addPolygonRegion(int regionId, const PolygonsD& polygons);

    /**
     * @brief 手动指定区域间空走代价（对称），覆盖自动计算的端点最小距离。
     */
    void addRoute(int fromId, int toId, double cost);

    /**
     * @brief 求解区域访问顺序（区域数 >= 2 时用 TSP，否则按添加顺序）。
     * @return 按序排列的区域 id 列表。
     */
    std::vector<int> optimizeOrder();

    /**
     * @brief 按优化顺序输出完整填充路径（填充结果模式；区域内路径方向/次序经贪心编排以减少跳变）。
     * @note 需先调用 optimizeOrder()；仅可用于填充结果模式。
     */
    PolygonsD buildPaths();

    /**
     * @brief 按优化顺序输出多边形集合（多边形模式；区域内多边形次序经贪心编排，
     *        每个多边形旋转起点至入门禁顶点，不改变环绕方向）。
     * @note 需先调用 optimizeOrder()；仅可用于多边形模式。
     */
    PolygonsD buildPolygons();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief 注册 Lua 路径优化函数（全局表 PathOptimize）。
 *
 * 注册后 Lua 中可用：
 * - PathOptimize.new()                -> 优化器对象（addRegion/addPolygons/addRoute/optimizeOrder/buildPaths/buildPolygons）
 * - PathOptimize.optimizeRegions(regions)  -> 填充结果模式一键优化，返回完整填充路径表（支持多点折线）
 * - PathOptimize.optimizePolygons(regions) -> 多边形模式一键优化，返回优化顺序的多边形表（填充前执行）
 * 其中 regions = 区域数组，每个区域 = 折线/多边形数组，每条折线/多边形 = {x=.., y=..} 点数组。
 *
 * @param L Lua state pointer.
 */
HSBA_SLICER_LIB_API void RegisterLuaPathOptimizeFunctions(lua_State* L);

/**
 * @brief 通过 Lua 脚本文件对独立区域的填充路径做前置优化（Lua 脚本嵌入方案）。
 *
 * Lua 函数签名：function optimize_paths(regions) return paths end
 * - regions: 区域数组，每个区域为折线数组（折线为 {x=.., y=..} 点数组）
 * - 返回值: 优化后的完整填充路径（折线数组）
 * 脚本环境中已注册多边形操作函数、填充函数与 PathOptimize 优化函数。
 *
 * @param regions 独立多边形区域集合（每个区域一组填充路径）。
 * @param scriptPath Lua 脚本文件路径。
 * @param functionName Lua 函数名（默认 "optimize_paths"）。
 * @param lua_reg 可选的额外 Lua 注册回调。
 * @return 优化后的完整填充路径。
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPaths(const std::vector<PolygonsD>& regions,
                                                     const std::string& scriptPath,
                                                     const std::string& functionName = "optimize_paths",
                                                     const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief 通过内联 Lua 脚本代码对独立区域的填充路径做前置优化。
 * @param regions 独立多边形区域集合（每个区域一组填充路径）。
 * @param script 内联 Lua 脚本代码。
 * @param functionName Lua 函数名（默认 "optimize_paths"）。
 * @param lua_reg 可选的额外 Lua 注册回调。
 * @return 优化后的完整填充路径。
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPathsString(const std::vector<PolygonsD>& regions,
                                                           const std::string& script,
                                                           const std::string& functionName = "optimize_paths",
                                                           const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief 通过 Lua 脚本文件对独立区域的多边形本身做前置优化（填充前执行，Lua 脚本嵌入方案）。
 *
 * Lua 函数签名：function optimize_polygons(regions) return polygons end
 * - regions: 区域数组，每个区域为多边形数组（多边形为 {x=.., y=..} 点数组）
 * - 返回值: 优化顺序后的多边形集合（多边形数组）
 * 脚本环境中已注册多边形操作函数、填充函数与 PathOptimize 优化函数。
 *
 * @param regions 独立多边形区域集合（每个区域一组多边形）。
 * @param scriptPath Lua 脚本文件路径。
 * @param functionName Lua 函数名（默认 "optimize_polygons"）。
 * @param lua_reg 可选的额外 Lua 注册回调。
 * @return 优化顺序后的多边形集合。
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPolygons(const std::vector<PolygonsD>& regions,
                                                        const std::string& scriptPath,
                                                        const std::string& functionName = "optimize_polygons",
                                                        const std::function<void(lua_State*)>& lua_reg = {});

/**
 * @brief 通过内联 Lua 脚本代码对独立区域的多边形本身做前置优化（填充前执行）。
 * @param regions 独立多边形区域集合（每个区域一组多边形）。
 * @param script 内联 Lua 脚本代码。
 * @param functionName Lua 函数名（默认 "optimize_polygons"）。
 * @param lua_reg 可选的额外 Lua 注册回调。
 * @return 优化顺序后的多边形集合。
 */
HSBA_SLICER_LIB_API PolygonsD LuaOptimizeRegionPolygonsString(const std::vector<PolygonsD>& regions,
                                                              const std::string& script,
                                                              const std::string& functionName = "optimize_polygons",
                                                              const std::function<void(lua_State*)>& lua_reg = {});

}  // namespace HsBa::Slicer

#endif  // !HSBA_SLICER_LIB_PATH_OPTIMIZER_HPP
