-- my_infill.lua
-- 自定义填充生成脚本（不含壁厚）
--
-- Lua 环境全局变量:
--   current_layer : 当前层多边形（已扣除壁厚区域）
--   layer_index   : 当前层索引（从 0 开始）
--   layer_height  : 层高（mm）
--   config        : 填充配置表，含 fill_spacing, fill_mode, fill_angle 等
--
-- 可用 PolygonOperations 函数:
--   offsetOperation(polys, delta)   -- 偏移
--   union / intersection / difference / xor
--   makeCircle / makeRectangle / makeRegularPolygon
--   area(poly)
--
-- 返回值: 填充多边形表

local PO = PolygonOperations

function generate_fill()
    if #current_layer == 0 then
        return {}
    end

    local spacing = config.fill_spacing or 0.4
    local angle = config.fill_angle or 45.0

    -- 计算当前层的填充角度（逐层旋转 60°，形成更均匀的力学结构）
    local layer_angle = angle + (layer_index % 6) * 60.0
    local rad = layer_angle * math.pi / 180.0

    -- 计算当前层的边界框
    local min_x, min_y = math.huge, math.huge
    local max_x, max_y = -math.huge, -math.huge

    for _, poly in ipairs(current_layer) do
        for _, pt in ipairs(poly) do
            if pt.x < min_x then min_x = pt.x end
            if pt.y < min_y then min_y = pt.y end
            if pt.x > max_x then max_x = pt.x end
            if pt.y > max_y then max_y = pt.y end
        end
    end

    -- 扩大扫描范围确保覆盖
    local margin = math.max(max_x - min_x, max_y - min_y) * 0.5
    local center_x = (min_x + max_x) / 2
    local center_y = (min_y + max_y) / 2
    local extent = math.max(max_x - min_x, max_y - min_y) / 2 + margin

    -- 生成平行线填充：沿指定角度方向创建线条
    local fill_lines = {}
    local cos_a = math.cos(rad)
    local sin_a = math.sin(rad)

    -- 线条数量
    local line_count = math.ceil(extent * 2 / spacing)
    local half_count = math.floor(line_count / 2)

    for i = -half_count, half_count do
        local offset = i * spacing
        -- 每条线用两个端点表示为一个细长矩形
        local perp_x = -sin_a * offset
        local perp_y = cos_a * offset

        local x1 = center_x + perp_x - cos_a * extent
        local y1 = center_y + perp_y - sin_a * extent
        local x2 = center_x + perp_x + cos_a * extent
        local y2 = center_y + perp_y + sin_a * extent

        -- 创建极细矩形（线宽 = spacing * 0.3）
        local half_w = spacing * 0.15
        local nx = -sin_a * half_w
        local ny = cos_a * half_w

        local line = {
            { x = x1 + nx, y = y1 + ny },
            { x = x2 + nx, y = y2 + ny },
            { x = x2 - nx, y = y2 - ny },
            { x = x1 - nx, y = y1 - ny },
        }
        table.insert(fill_lines, line)
    end

    -- 将填充线与当前层轮廓做交集，裁剪到有效区域
    local result = PO.intersection(fill_lines, current_layer)

    return result
end

return generate_fill()
