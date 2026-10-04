-- my_waam_pipeline.lua
-- 完全由 Lua 脚本驱动的 WAAM（机器人金属丝沉积）流水线
--
-- WAAM 与粉末床 / 叠层工艺根本不同：它沿逐层轮廓驱动机器人逐道熔覆焊丝，
-- 输出是机器人语言程序（ABB / KUKA / FANUC），而非 zip 包。
--
-- 本脚本自行决定“加载模型 -> 逐层切片 -> 生成机器人路径”的全部阶段，并调用
-- HsBa.saveWaamPackage 算子（内部构建 WeldRobotPath 并写出机器人程序）。
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
    local output = (output_path and #output_path > 0) and output_path or "output/custom_waam_pipeline.txt"

    -- WAAM 每层是较厚的沉积道，层高远大于粉末床工艺
    local layer_height = param("layer_height", 0.8)
    local first_layer_height = param("first_layer_height", 1.0)

    -- 1. 加载模型
    HsBa.progress(2, "加载模型")
    HsBa.loadModel(model.name, model.path)
    local info = HsBa.modelInfo(model.name)
    local height = info.bbox_max.z - info.bbox_min.z
    print(string.format("[Lua WAAM] 模型 %s: 高度 %.2f mm, 体积 %.2f mm^3", model.name, height, info.volume))

    local layers = HsBa.layerCount(model.name, layer_height, first_layer_height)
    HsBa.setLayers(layers)

    -- 2. 逐层熔覆轮廓（沉积路径）
    local z_base = info.bbox_min.z
    local outlines = {}
    local z_heights = {}
    for i = 0, layers - 1 do
        local z = HsBa.layerZ(i, layer_height, first_layer_height)
        outlines[i + 1] = HsBa.slice(model.name, z_base + z)
        z_heights[i + 1] = z
        if i % 10 == 0 then
            HsBa.progress(5 + math.floor(70 * i / math.max(layers, 1)), "切片")
        end
    end

    -- 3. 组装焊接工艺配置（可选，写入机器人程序头部或旁路文件）
    HsBa.progress(78, "生成配置")
    local config = (pipeline_config and #pipeline_config > 0)
        and pipeline_config
        or string.format(
            '{ "process": "WAAM", "layers": %d, "layer_height": %.3f, "bead_width": %.2f }',
            layers, layer_height, param("bead_width", 1.2))

    -- 4. 生成机器人程序（robotType 0=ABB/1=KUKA/2=FANUC；不传 script 用内置代码生成）
    HsBa.progress(85, "生成机器人路径")
    local ok, err = HsBa.saveWaamPackage({
        outlines = outlines,
        zHeights = z_heights,
        weld = {
            current = param("arc_current", 180.0),
            voltage = param("arc_voltage", 22.0),
            wireFeedSpeed = param("wire_feed_speed", 5.0),
            gasFlowRate = param("gas_flow_rate", 15.0),
            travelSpeed = param("travel_speed", 8.0),
            process = param("welding_process", 0),
        },
        robotType = param("robot_type", 0),
        beadWidth = param("bead_width", 1.2),
        config = config,
        output = output,
    })

    if not ok then
        HsBa.removeModel(model.name)
        return false, "WAAM 机器人路径生成失败: " .. tostring(err)
    end

    HsBa.setOutputPath(output)
    HsBa.removeModel(model.name)
    HsBa.progress(100, "完成")

    return string.format("Lua WAAM 流水线完成: %d 层, 高度 %.2f mm, 机器人程序 %s", layers, height, output)
end
