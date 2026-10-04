-- my_slm_pipeline.lua
-- 完全由 Lua 脚本驱动的 SLM（选择性激光熔金属）打包流水线
--
-- 与内置 SLM 流水线（阶段顺序在 C++ 中固定，仅导出交给 Lua）不同，本脚本自行
-- 决定“加载模型 -> 逐层切片 -> 打包导出”的全部阶段与顺序。
--
-- SLM 是金属粉末床工艺，与 SLS 高度相似（无需支撑 / 地板），因此复用
-- saveSlsPackage 的 zip + 数据库打包流程，只是 config 内写入金属工艺参数。
--
-- Lua 环境全局变量:
--   HsBa / model_name / model_path / output_path / pipeline_config / machine
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
    local output = (output_path and #output_path > 0) and output_path or "output/custom_slm_pipeline.zip"

    local layer_height = param("layer_height", 0.06)
    local first_layer_height = param("first_layer_height", 0.08)

    -- 1. 加载模型
    HsBa.progress(2, "加载模型")
    HsBa.loadModel(model.name, model.path)
    local info = HsBa.modelInfo(model.name)
    local height = info.bbox_max.z - info.bbox_min.z
    print(string.format("[Lua SLM] 模型 %s: 高度 %.2f mm, 体积 %.2f mm^3", model.name, height, info.volume))

    local layers = HsBa.layerCount(model.name, layer_height, first_layer_height)
    HsBa.setLayers(layers)

    -- 2. 逐层切片
    local z_base = info.bbox_min.z
    local outlines = {}
    local z_heights = {}
    for i = 0, layers - 1 do
        local z = HsBa.layerZ(i, layer_height, first_layer_height)
        outlines[i + 1] = HsBa.slice(model.name, z_base + z)
        z_heights[i + 1] = z
        if i % 50 == 0 then
            HsBa.progress(5 + math.floor(65 * i / math.max(layers, 1)), "切片")
        end
    end

    -- 3. 组装金属熔覆工艺配置，写入包内 config.json
    HsBa.progress(75, "生成配置")
    local config = (pipeline_config and #pipeline_config > 0)
        and pipeline_config
        or string.format(
            '{ "process": "SLM", "layers": %d, "layer_height": %.3f, '
            .. '"laser_power": %.1f, "scan_speed": %.1f, '
            .. '"hatch_spacing": %.3f, "hatch_rotation": %.1f, '
            .. '"material": "%s", "bed_temperature": %.1f }',
            layers, layer_height,
            param("laser_power", 200.0),
            param("scan_speed", 1000.0),
            param("hatch_spacing", 0.1),
            param("hatch_rotation", 67.0),
            param("material", "TITANIUM"),
            param("bed_temperature", 100.0))

    -- 4. 打包导出（复用 SLS 式 zip + 数据库打包，导出脚本决定最终格式）
    HsBa.progress(85, "打包导出")
    local ok = HsBa.saveSlsPackage({
        outlines = outlines,
        zHeights = z_heights,
        config = config,
        output = output,
        script = "scripts/my_sls_export.lua",
        func = "export_sls",
    })

    if not ok then
        HsBa.removeModel(model.name)
        return false, "SLM 打包失败"
    end

    HsBa.setOutputPath(output)
    HsBa.removeModel(model.name)
    HsBa.progress(100, "完成")

    return string.format("Lua SLM 流水线完成: %d 层, 高度 %.2f mm, 输出 %s", layers, height, output)
end
