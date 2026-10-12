-- 全组合图自定义路径优化示例脚本
-- 演示如何用新增的 getFullGraph / getExpandedGraph / setCustomTour 接口，
-- 在 Lua 侧实现任意路径优化算法，再把结果反馈给优化器构建完整填充路径。
-- regions 结构: 区域数组，每个区域 = 折线数组，每条折线 = {x=.., y=..} 点数组
-- 环境内已注册: PathOptimize / PolygonFill / 多边形操作函数

local function indexOf(list, value)
    for i = 1, #list do
        if list[i] == value then
            return i
        end
    end
    return nil
end

-- 最近邻贪心：从首个区域出发，每步选择代价最小且未访问的区域
function optimize_paths_custom(regions)
    local opt = PathOptimize.new()
    for i, paths in ipairs(regions) do
        opt:addRegion(i, paths)
    end

    -- 获取区域级全组合图（所有区域间最短路径代价矩阵）
    local graph = opt:getFullGraph()

    local n = #graph.vertices
    local visited = {}
    local tour = { graph.vertices[1] }
    visited[graph.vertices[1]] = true
    for _ = 2, n do
        local curr = tour[#tour]
        local ci = indexOf(graph.vertices, curr)
        local bestId, bestCost = nil, math.huge
        for j = 1, n do
            local nextId = graph.vertices[j]
            if not visited[nextId] then
                local cost = graph.distMatrix[ci][j]
                -- false 表示不可达，跳过
                if cost and cost < bestCost then
                    bestCost = cost
                    bestId = nextId
                end
            end
        end
        if bestId == nil then
            break
        end
        tour[#tour + 1] = bestId
        visited[bestId] = true
    end

    -- 反馈自定义顺序，跳过内置遗传 TSP，直接构建路径
    opt:setCustomTour(tour)
    return opt:buildPaths()
end

-- 暴力枚举 TSP（仅适用于很小的 N）：遍历全排列取总代价最小者
function optimize_paths_bruteforce(regions)
    local opt = PathOptimize.new()
    for i, paths in ipairs(regions) do
        opt:addRegion(i, paths)
    end

    local graph = opt:getFullGraph()
    local verts = graph.vertices
    local n = #verts

    local bestTour, bestCost = nil, math.huge
    local perm = {}
    local used = {}

    local function tourCost(order)
        local total = 0
        for i = 1, n do
            local a = indexOf(verts, order[i])
            local b = indexOf(verts, order[(i % n) + 1])
            local c = graph.distMatrix[a][b]
            if not c then
                return nil
            end
            total = total + c
        end
        return total
    end

    local function backtrack(depth)
        if depth > n then
            local c = tourCost(perm)
            if c and c < bestCost then
                bestCost = c
                bestTour = {}
                for i = 1, n do
                    bestTour[i] = perm[i]
                end
            end
            return
        end
        for i = 1, n do
            if not used[i] then
                used[i] = true
                perm[depth] = verts[i]
                backtrack(depth + 1)
                used[i] = false
            end
        end
    end

    if n > 0 and n <= 8 then
        backtrack(1)
    else
        -- 区域过多时退化为最近邻贪心
        return optimize_paths_custom(regions)
    end

    if bestTour then
        opt:setCustomTour(bestTour)
    end
    return opt:buildPaths()
end

-- 门级展开图只读查询示例：统计顶点/边数量，验证接口可用
function optimize_paths_probe(regions)
    local opt = PathOptimize.new()
    for i, paths in ipairs(regions) do
        opt:addRegion(i, paths)
    end
    local exp = opt:getExpandedGraph()
    assert(exp.vertexCount >= 0)
    assert(exp.edgeCount >= 0)
    assert(#exp.vertices == exp.vertexCount)
    assert(#exp.edges == exp.edgeCount)
    return opt:buildPaths()
end

return {
    optimize_paths_custom = optimize_paths_custom,
    optimize_paths_bruteforce = optimize_paths_bruteforce,
    optimize_paths_probe = optimize_paths_probe,
}
