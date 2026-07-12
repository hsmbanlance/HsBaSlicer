-- my_sla_support.lua
-- 自定义SLA支撑生成脚本
--
-- Lua 环境全局变量:
--   current_layer : 当前层多边形 { { {x=..,y=..}, ... }, ... }
--   prev_layer    : 上一层多边形（首层为空）
--   layer_height  : 层高（mm）
--   config        : 支撑配置表，含以下字段:
--     overhang_angle_threshold - 悬垂角度阈值 (度)
--     layer_height             - 层高 (mm)
--     support_gap              - 支撑与模型间隙 (mm)
--     support_diameter         - 支撑柱直径 (mm)
--     support_density          - 支撑填充密度 [0,1]
--     support_pattern          - 支撑模式 (0=牺牲柱, 1=锥形)
--
-- 可用 PolygonOperations 函数:
--   offsetOperation(polys, delta)   -- 偏移
--   union(polys_a, polys_b)         -- 并集
--   difference(polys_a, polys_b)    -- 差集
--   intersection(polys_a, polys_b)  -- 交集
--   makeCircle(cx, cy, r)           -- 生成圆形多边形
--   area(poly)                      -- 计算面积
--
-- 可用 Support 模块:
--   Support.new_sla()               -- 创建SLA牺牲支撑生成器
--   Support.generate(obj, current, prev, height, cfg) -- 用指定生成器生成支撑
--   Support.detect_overhang(current, prev, height, angle) -- 检测悬垂区域
--   Support.default_config()        -- 获取默认支撑配置表
--
-- 返回值: 支撑截面多边形表（格式同 current_layer）

local PO = PolygonOperations

function generate_support()
    -- 首层无需支撑
    if #prev_layer == 0 then
        return {}
    end

    -- 1. 检测悬垂区域
    local overhang_angle = config.overhang_angle_threshold or 45.0
    local overhang_regions = Support.detect_overhang(
        current_layer, prev_layer, layer_height, overhang_angle)

    if #overhang_regions == 0 then
        return {}
    end

    -- 2. 使用内置SLA支撑生成器
    local generator = Support.new_sla()

    local support_cfg = Support.default_config()
    support_cfg.overhang_angle_threshold = overhang_angle
    support_cfg.layer_height = layer_height
    support_cfg.support_gap = config.support_gap or 0.5
    support_cfg.support_diameter = config.support_diameter or 2.0
    support_cfg.support_density = config.support_density or 0.3

    local result = Support.generate(
        generator, current_layer, prev_layer, layer_height, support_cfg)

    -- 3. 可选：根据密度过滤支撑点
    --    此处演示保留面积大于阈值的支撑
    local min_area = 0.1  -- 最小面积阈值 mm²
    local filtered = {}
    for _, poly in ipairs(result) do
        if PO.area(poly) > min_area then
            table.insert(filtered, poly)
        end
    end

    return filtered
end

return generate_support()
