-- my_fdm_pipeline.lua
-- 完全由 Lua 脚本驱动的 FDM 切片流水线
--
-- 与内置 FDM 流水线（阶段顺序在 C++ 中固定，仅支撑/填充可被 Lua 替换）不同，
-- 本脚本自行决定“加载模型 -> 切片 -> 支撑 -> 填充 -> 生成路径 -> 写文件”的
-- 全部阶段与顺序，C++ 侧只负责调用 run_pipeline()。
--
-- Lua 环境全局变量:
--   HsBa            : 流水线算子表（loadModel/slice/fdmSupport/fill/toGcode/...）
--   model_name      : string, C++ 传入的模型名（可能为空串）
--   model_path      : string, C++ 传入的模型文件路径（可能为空串）
--   output_path     : string, C++ 传入的默认输出路径（可能为空串）
--   pipeline_config : string, C++ 传入的 JSON 字符串，原样写入 G-code 注释
--   machine         : table, 由 C++ 侧 pipeline_lua_source 预置的机台参数（可选）
--
-- 已注册的通用 Lua 库:
--   PolygonOperations : union / intersection / difference / xor / offsetOperation / ...
--   Support           : 支撑工具函数
--   PolygonFill       : 填充工具函数
--   PathOptimize      : 路径顺序优化
--
-- 返回值: 字符串（作为结果上报给 C++），返回 false/nil 表示流水线失败

-- 取机台参数：优先使用 C++ 注入的 machine 表，其次用脚本默认值
local function param(name, default)
    if type(machine) == "table" and machine[name] ~= nil then
        return machine[name]
    end
    return default
end

-- 计算所有层轮廓的 XY 包围盒尺寸（单位 mm）
local function extentOfLayers(layer_list)
    local min_x, min_y = math.huge, math.huge
    local max_x, max_y = -math.huge, -math.huge
    for _, polys in ipairs(layer_list) do
        for _, poly in ipairs(polys) do
            for _, pt in ipairs(poly) do
                if pt.x < min_x then min_x = pt.x end
                if pt.y < min_y then min_y = pt.y end
                if pt.x > max_x then max_x = pt.x end
                if pt.y > max_y then max_y = pt.y end
            end
        end
    end
    if min_x == math.huge then
        return 0, 0
    end
    return max_x - min_x, max_y - min_y
end

function run_pipeline()
    local model = {
        name = (model_name and #model_name > 0) and model_name or "custom_model",
        path = (model_path and #model_path > 0) and model_path or "models/stanford_bunny.stl",
    }
    local output = (output_path and #output_path > 0) and output_path or "output/custom_fdm_pipeline.gcode"

    local layer_height = param("layer_height", 0.2)
    local first_layer_height = param("first_layer_height", 0.25)

    -- ------------------------------------------------------------------
    -- 1. 加载模型
    -- ------------------------------------------------------------------
    HsBa.progress(1, "加载模型")
    HsBa.loadModel(model.name, model.path)
    local info = HsBa.modelInfo(model.name)
    local height = info.bbox_max.z - info.bbox_min.z
    print(string.format("[Lua FDM] 模型 %s: 高度 %.2f mm, 体积 %.2f mm^3",
        model.name, height, info.volume))

    local layers = HsBa.layerCount(model.name, layer_height, first_layer_height)
    HsBa.setLayers(layers)
    local z_base = info.bbox_min.z  -- 以模型底面为 Z 起点（与内置流水线一致）

    -- ------------------------------------------------------------------
    -- 2. 逐层切片 + 3. 填充（此处由脚本自行交错执行，而非分两轮循环）
    -- ------------------------------------------------------------------
    local outlines = {}
    local fills = {}
    local z_heights = {}
    local fill_spacing = param("fill_spacing", 0.4)
    local border_count = param("wall_count", 2)

    for i = 0, layers - 1 do
        local z = HsBa.layerZ(i, layer_height, first_layer_height)
        local polys = HsBa.slice(model.name, z_base + z)
        outlines[i + 1] = polys
        z_heights[i + 1] = z

        -- 每 3 层旋转一次填充角度：纯 Lua 决定的工艺策略
        local angle = 45.0 + (i % 3) * 60.0
        fills[i + 1] = HsBa.fill(polys, {
            spacing = fill_spacing,
            angle = angle,
            mode = "zigzag",
            borderCount = border_count,
        })

        if i % 20 == 0 then
            HsBa.progress(5 + math.floor(60 * i / math.max(layers, 1)), "切片与填充")
        end
    end

    -- ------------------------------------------------------------------
    -- 4. 支撑（可被参数关闭：这体现了 Lua 对阶段顺序的完全掌控）
    -- ------------------------------------------------------------------
    local supports = {}
    if param("enable_support", false) then
        HsBa.progress(70, "生成支撑")
        supports = HsBa.fdmSupport(outlines, {
            layer_height = layer_height,
            overhang_angle = param("overhang_angle", 45.0),
            support_gap = 0.3,
            support_diameter = 2.0,
            support_density = param("support_density", 0.2),
            interface_layers = 2,
            interface_density = 0.6,
        })
    else
        for i = 1, layers do
            supports[i] = {}
        end
    end

    -- ------------------------------------------------------------------
    -- 5. 组装每层路径数据并生成 G-code
    -- ------------------------------------------------------------------
    HsBa.progress(80, "生成路径")
    local path_layers = {}
    for i = 1, layers do
        path_layers[i] = {
            outlines = outlines[i],
            fills = fills[i],
            supports = supports[i],
            zHeight = z_heights[i],
        }
    end

    local gcode = HsBa.toGcode(path_layers, {
        layerHeight = layer_height,
        lineWidth = param("line_width", 0.45),
        printSpeed = param("print_speed", 60),
        travelSpeed = param("travel_speed", 150),
        extrusionMultiplier = 1.0,
        firmware = param("firmware", "marlin"),
        nozzleDiameter = param("nozzle_diameter", 0.4),
        filamentDiameter = param("filament_diameter", 1.75),
        nozzleTemp = param("nozzle_temp", 210),
        bedTemp = param("bed_temp", 60),
    })

    -- ------------------------------------------------------------------
    -- 6. 写文件（由 Lua 直接调用文件算子，不经过 C++ 的输出逻辑）
    -- ------------------------------------------------------------------
    HsBa.progress(92, "写入 G-code")
    local size_x, size_y = extentOfLayers(outlines)
    local header = string.format(
        "; Generated by the Custom Lua pipeline (my_fdm_pipeline.lua)\n" ..
        "; model=%s layers=%d layer_height=%.3f first_layer_height=%.3f\n" ..
        "; model_size=%.2fx%.2f mm\n; config=%s\n",
        model.path, layers, layer_height, first_layer_height, size_x, size_y,
        (pipeline_config and #pipeline_config > 0) and pipeline_config or "n/a")
    HsBa.writeFile(output, header .. gcode)

    HsBa.setOutputPath(output)
    HsBa.removeModel(model.name)
    HsBa.progress(100, "完成")

    return string.format("Lua FDM 流水线完成: %d 层, 输出 %s", layers, output)
end
