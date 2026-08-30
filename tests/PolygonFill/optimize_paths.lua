-- 路径输出前置优化示例脚本（两种模式，同一优化器内不可混用）
-- 模式1 填充结果模式（填充后执行）: 输入为填充路径，支持多点折线，输出完整填充路径（optimize_paths）
-- 模式2 多边形模式（填充前执行）: 输入为多边形本身，输出优化顺序的多边形集合（optimize_polygons）
-- regions 结构: 区域数组，每个区域 = 折线/多边形数组，每条折线/多边形 = {x=.., y=..} 点数组
-- 环境内已注册: PathOptimize / PolygonFill / 多边形操作函数

function optimize_paths(regions)
    -- 填充结果模式内置策略：把每个区域作为图顶点，TSP 求解区域访问顺序后输出完整填充路径（支持多点折线）
    return PathOptimize.optimizeRegions(regions)
end

-- 自定义策略示例（可按需启用）：手动构建优化器，覆盖区域间空走代价
function optimize_paths_manual(regions)
    local opt = PathOptimize.new()
    for i, paths in ipairs(regions) do
        opt:addRegion(i, paths)
    end
    -- 例：手动指定区域 1 -> 区域 2 的空走代价
    -- opt:addRoute(1, 2, 15.0)
    local order = opt:optimizeOrder()
    return opt:buildPaths()
end

-- 多边形模式（填充前执行）：区域输入为多边形本身，输出优化顺序的多边形集合，供后续填充使用
function optimize_polygons(regions)
    return PathOptimize.optimizePolygons(regions)
end

return {
    optimize_paths = optimize_paths,
    optimize_paths_manual = optimize_paths_manual,
    optimize_polygons = optimize_polygons,
}
