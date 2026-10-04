-- my_lom_pipeline.lua
-- 完全由 Lua 脚本驱动的 LOM（叠层实体切割）打包流水线
--
-- LOM 逐层对应一张板材轮廓，无需支撑 / 地板。切割 / 粘合工艺参数由脚本写入
-- config.json。这里复用 saveSlsPackage 的 zip + 数据库打包流程。
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
    local output = (output_path and #output_path > 0) and output_path or "output/custom_lom_pipeline.zip"

    local layer_height = param("layer_height", 0.2)
    local first_layer_height = param("first_layer_height", 0.2)

    -- 1. 加载模型
    HsBa.progress(2, "加载模型")
    HsBa.loadModel(model.name, model.path)
    local info = HsBa.modelInfo(model.name)
    local height = info.bbox_max.z - info.bbox_min.z
    print(string.format("[Lua LOM] 模型 %s: 高度 %.2f mm, 体积 %.2f mm^3", model.name, height, info.volume))

    local layers = HsBa.layerCount(model.name, layer_height, first_layer_height)
    HsBa.setLayers(layers)

    -- 2. 逐层切割轮廓
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

    -- 3. 组装切割 / 粘合工艺配置
    HsBa.progress(75, "生成配置")
    local config = (pipeline_config and #pipeline_config > 0)
        and pipeline_config
        or string.format(
            '{ "process": "LOM", "layers": %d, "sheet_thickness": %.3f, '
            .. '"cut_speed": %.1f, "cut_margin": %.3f, "laser_power": %.2f, '
            .. '"bond_temperature": %.1f, "bond_pressure": %.2f, "bond_time": %.2f, '
            .. '"cut_mode": "%s" }',
            layers, layer_height,
            param("cut_speed", 300.0),
            param("cut_margin", 0.5),
            param("laser_power", 0.8),
            param("bond_temperature", 150.0),
            param("bond_pressure", 1.0),
            param("bond_time", 5.0),
            param("cut_mode", "CONTOUR"))

    -- 4. 打包导出
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
        return false, "LOM 打包失败"
    end

    HsBa.setOutputPath(output)
    HsBa.removeModel(model.name)
    HsBa.progress(100, "完成")

    return string.format("Lua LOM 流水线完成: %d 层, 高度 %.2f mm, 输出 %s", layers, height, output)
end
