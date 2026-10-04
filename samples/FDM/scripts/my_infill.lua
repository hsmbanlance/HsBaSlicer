-- my_infill.lua
-- 自定义填充生成脚本（不含壁厚）
--
-- 调用约定（与 LuaCustomFill 胶水一致）:
--   胶水先执行本 chunk（仅定义函数，不要末尾自调用），
--   再调用 generate_fill(polygons)，以"参数"传入当前层多边形并接收返回的填充多边形。
--   本阶段不注入任何全局变量，层号/配置不可用，请使用脚本内默认值。
--
-- 参数 current_layer : 当前层多边形（已扣除壁厚区域）{ { {x=..,y=..}, ... }, ... }
-- 返回值: 填充多边形表
--
-- 可用 PolygonOperations 函数:
--   offsetOperation(polys, delta)   -- 偏移
--   union / intersection / difference / xor
--   makeCircle / makeRectangle / makeRegularPolygon
--   area(poly)

local PO = PolygonOperations

function generate_fill(current_layer)
    if #current_layer == 0 then
        return {}
    end

    -- 填充参数（胶水未传入 config，此处使用脚本内默认值）
    local spacing = 0.4
    local angle = 45.0

    local rad = angle * math.pi / 180.0

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
