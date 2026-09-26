-- my_sla_pipeline.lua
-- 完全由 Lua 脚本驱动的 SLA（光固化）打包流水线
--
-- 阶段（全部由本脚本编排）:
--   1. 加载模型并逐层切片
--   2. 生成 SLA 支撑
--   3. 由底层轮廓生成地板 / Raft
--   4. 调用 HsBa.saveSlaPackage 渲染层图并打包 zip
--
-- Lua 环境全局变量:
--   HsBa            : 流水线算子表
--   model_name      : string, 模型名
--   model_path      : string, 模型文件路径
--   output_path     : string, 输出 zip 路径
--   pipeline_config : string, C++ 传入的 JSON，原样作为包内 config 内容
--   machine         : table, 由 pipeline_lua_source 预置的参数（可选）
--
-- 返回值: 字符串（成功）/ false（失败）

local function param(name, default)
    if type(machine) == "table" and machine[name] ~= nil then
        return machine[name]
    end
    return default
end

function run_pipeline()
    local model = {
        name = (model_name and #model_name > 0) and model_name or "custom_model",
        path = (model_path and #model_path > 0) and model_path or "models/stanford_bunny.stl",
    }
    local output = (output_path and #output_path > 0) and output_path or "output/custom_sla_pipeline.zip"

    local layer_height = param("layer_height", 0.05)
    local first_layer_height = param("first_layer_height", 0.1)

    -- 1. 模型
    HsBa.progress(2, "加载模型")
    HsBa.loadModel(model.name, model.path)
    local info = HsBa.modelInfo(model.name)
    local layers = HsBa.layerCount(model.name, layer_height, first_layer_height)
    HsBa.setLayers(layers)

    -- 2. 切片（以模型底面为 Z 起点）
    local z_base = info.bbox_min.z
    local outlines = {}
    for i = 0, layers - 1 do
        outlines[i + 1] = HsBa.slice(model.name, z_base + HsBa.layerZ(i, layer_height, first_layer_height))
        if i % 50 == 0 then
            HsBa.progress(5 + math.floor(45 * i / math.max(layers, 1)), "切片")
        end
    end

    -- 3. 支撑
    HsBa.progress(55, "生成支撑")
    local supports = HsBa.slaSupport(outlines, {
        layer_height = layer_height,
        overhang_angle = param("overhang_angle", 35.0),
        support_gap = 0.05,
        support_diameter = param("support_diameter", 1.2),
        support_density = param("support_density", 0.15),
        tip_diameter = 0.3,
        raft_thickness = 0.2,
    })

    -- 4. 地板 / Raft（以最底层轮廓为着地足迹）
    HsBa.progress(70, "生成地板")
    local floor = HsBa.floor(outlines[1] or {}, {
        raft_offset = param("raft_offset", 2.0),
        border_width = 1.0,
        fill_spacing = 0.5,
        fill_angle_deg = 0.0,
        border_count = 3,
        use_convex_hull = param("floor_convex_hull", true),
        concave_hull_points = 0,
    })

    -- 5. 打包（层图 + 支撑图 + 地板图 + config.json）
    HsBa.progress(80, "渲染并打包")
    local ok = HsBa.saveSlaPackage({
        outlines = outlines,
        supports = supports,
        floor = floor,
        config = (pipeline_config and #pipeline_config > 0)
            and pipeline_config
            or string.format('{ "process": "SLA", "layers": %d, "layer_height": %.3f }', layers, layer_height),
        output = output,
        imageWidth = param("image_width", 1920),
        imageHeight = param("image_height", 1080),
        imageExtension = param("image_extension", ".png"),
    })

    if not ok then
        HsBa.removeModel(model.name)
        return false, "SLA 打包失败"
    end

    HsBa.setOutputPath(output)
    HsBa.removeModel(model.name)
    HsBa.progress(100, "完成")

    return string.format("Lua SLA 流水线完成: %d 层, 高度 %.2f mm, 输出 %s",
        layers, info.bbox_max.z - info.bbox_min.z, output)
end
