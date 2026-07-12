-- my_sla_floor.lua
-- 自定义SLA地板生成脚本
--
-- Lua 环境全局变量:
--   bottom_layer : 底层多边形 { { {x=..,y=..}, ... }, ... }
--   config       : 地板配置表，含以下字段:
--     raft_offset      - Raft向外偏移量 (mm)
--     border_width     - 边框环宽度 (mm)
--     fill_spacing     - 填充线间距 (mm)
--     fill_angle_deg   - 填充角度 (度)
--     border_count     - 边框环数量
--     use_convex_hull  - 是否使用凸包
--     concave_hull_points - 凹包额外点数 (0=禁用)
--
-- 可用 PolygonOperations 函数:
--   offsetOperation(polys, delta)   -- 偏移（正=膨胀，负=收缩）
--   union(polys_a, polys_b)         -- 并集
--   difference(polys_a, polys_b)    -- 差集
--   intersection(polys_a, polys_b)  -- 交集
--
-- 可用 PolygonFill 函数:
--   zigzagFill(polys, spacing, angle) -- 之字形填充
--   lineFill(polys, spacing, angle)   -- 平行线填充
--   offsetFill(polys, spacing)        -- 偏移填充
--
-- 返回值: 地板多边形表（格式同 bottom_layer）

local PO = PolygonOperations

function generate_floor()
    -- 1. 计算底层轮廓的凸包或直接使用原始轮廓
    local footprint = bottom_layer
    if config.use_convex_hull then
        -- 使用凸包简化轮廓
        footprint = PO.convexHull(bottom_layer)
    end

    -- 2. 向外偏移生成Raft外边界
    local raft_outer = PO.offsetOperation(footprint, config.raft_offset or 2.0)

    -- 3. 向内收缩生成内边界
    local border_width = config.border_width or 1.0
    local raft_inner = PO.offsetOperation(raft_outer, -border_width)

    -- 4. 生成边框环（外边界 - 内边界）
    local border = PO.difference(raft_outer, raft_inner)

    -- 5. 生成内部填充
    local fill_area = PO.offsetOperation(raft_inner, -0.1)  -- 微小偏移避免重叠
    local fill_spacing = config.fill_spacing or 0.5
    local fill_angle = config.fill_angle_deg or 0.0
    local fill = zigzagFill(fill_area, fill_spacing, fill_angle)

    -- 6. 合并边框和填充
    local result = PO.union(border, fill)

    return result
end

return generate_floor()
