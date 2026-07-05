-- my_support.lua
-- 自定义支撑生成脚本 —— 使用内置支撑生成器
--
-- Lua 环境全局变量:
--   current_layer : 当前层多边形 { { {x=..,y=..}, ... }, ... }
--   prev_layer    : 上一层多边形（首层为空）
--   layer_height  : 层高（mm）
--   config        : 支撑配置表，含 overhang_angle_threshold, support_diameter,
--                   support_gap, support_density, support_pattern 等字段
--
-- 可用 PolygonOperations 函数:
--   offsetOperation(polys, delta)   -- 偏移（正=膨胀，负=收缩）
--   intersection(polys_a, polys_b)  -- 交集
--   difference(polys_a, polys_b)    -- 差集
--   union(polys_a, polys_b)         -- 并集
--   makeCircle(cx, cy, r)           -- 生成圆形多边形
--   area(poly)                      -- 计算面积
--
-- 可用 Support 模块:
--   Support.new_plane()             -- 创建平面支撑生成器
--   Support.new_tree()              -- 创建树状支撑生成器
--   Support.new_honeycomb()         -- 创建蜂窝支撑生成器
--   Support.new_sla()               -- 创建 SLA 支撑生成器
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

    -- 1. 使用内置悬垂检测器找出需要支撑的区域
    local overhang_angle = config.overhang_angle_threshold or 45.0
    local overhang_regions = Support.detect_overhang(
        current_layer, prev_layer, layer_height, overhang_angle)

    if #overhang_regions == 0 then
        return {}
    end

    -- 2. 根据配置中的支撑模式选择对应生成器
    local generator
    local pattern = config.support_pattern or 0
    if pattern == 0 then
        -- 平面支撑
        generator = Support.new_plane()
    elseif pattern == 1 then
        -- 树状支撑
        generator = Support.new_tree()
    elseif pattern == 2 then
        -- 蜂窝支撑
        generator = Support.new_honeycomb()
    else
        -- 默认使用平面支撑
        generator = Support.new_plane()
    end

    -- 3. 使用选定的生成器生成支撑结构
    local support_cfg = Support.default_config()
    support_cfg.overhang_angle_threshold = overhang_angle
    support_cfg.layer_height = layer_height
    support_cfg.support_gap = config.support_gap or 0.5
    support_cfg.support_diameter = config.support_diameter or 2.0
    support_cfg.support_density = config.support_density or 0.5

    local result = Support.generate(
        generator, current_layer, prev_layer, layer_height, support_cfg)

    -- 4. 可选：用多边形操作进一步处理（如缩小支撑接触点）
    --    此处演示将支撑结果收缩 0.1mm 使接触更精细
    local refined = PO.offsetOperation(result, -0.1)

    return refined
end

return generate_support()
