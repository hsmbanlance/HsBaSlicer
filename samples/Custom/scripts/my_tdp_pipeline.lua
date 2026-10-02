-- my_tdp_pipeline.lua
-- 完全由 Lua 脚本驱动的 3DP（粘结剂喷射）打包流水线
--
-- 3DP 是粉末床工艺：逐层向粉末床喷射液态粘结剂，无需支撑 / 地板。
-- 喷头 / 液滴 / 固化等参数由脚本写入 config.json。这里复用 saveSlsPackage
-- 的 zip + 数据库打包流程。
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
    local output = (output_path and #output_path > 0) and output_path or "output/custom_tdp_pipeline.zip"

    local layer_height = param("layer_height", 0.1)
    local first_layer_height = param("first_layer_height", 0.12)

    -- 1. 加载模型
    HsBa.progress(2, "加载模型")
    HsBa.loadModel(model.name, model.path)
    local info = HsBa.modelInfo(model.name)
    local height = info.bbox_max.z - info.bbox_min.z
    print(string.format("[Lua 3DP] 模型 %s: 高度 %.2f mm, 体积 %.2f mm^3", model.name, height, info.volume))

    local layers = HsBa.layerCount(model.name, layer_height, first_layer_height)
    HsBa.setLayers(layers)

    -- 2. 逐层喷射轮廓
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

    -- 3. 组装粘结剂喷射工艺配置
    HsBa.progress(75, "生成配置")
    local config = (pipeline_config and #pipeline_config > 0)
        and pipeline_config
        or string.format(
            '{ "process": "3DP", "layers": %d, "layer_height": %.3f, '
            .. '"head_count": %d, "drop_spacing": %.3f, "binder_saturation": %.2f, '
            .. '"curing_time": %.2f, "bed_temperature": %.1f, "binder_mode": "%s" }',
            layers, layer_height,
            param("head_count", 128),
            param("drop_spacing", 0.05),
            param("binder_saturation", 0.6),
            param("curing_time", 1.0),
            param("bed_temperature", 40.0),
            param("binder_mode", "SINGLE"))

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
        return false, "3DP 打包失败"
    end

    HsBa.setOutputPath(output)
    HsBa.removeModel(model.name)
    HsBa.progress(100, "完成")

    return string.format("Lua 3DP 流水线完成: %d 层, 高度 %.2f mm, 输出 %s", layers, height, output)
end
