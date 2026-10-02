#include "waam_pipeline.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Path/waam_export.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "base/coroutine.hpp"

namespace HsBa::Slicer::Pipeline
{

struct InternalWaamResult
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

struct InternalWaamConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.8f;
    float first_layer_height = 1.0f;
    float bead_width = 1.2f;
    float travel_speed = 8.0f;
    float wire_feed_speed = 5.0f;
    float arc_current = 180.0f;
    float arc_voltage = 22.0f;
    float gas_flow_rate = 15.0f;
    HsBaWaamMaterial_t material = HSBA_WAAM_MATERIAL_STEEL;
    HsBaWaamWeldProcess_t welding_process = HSBA_WAAM_WELD_ARC;
    HsBaWaamProtection_t protection = HSBA_WAAM_PROTECTION_SHIELD_GAS;
    HsBaMetalProtectGas_t protect_gas = HSBA_METAL_GAS_ARGON;
    float interpass_temperature = 100.0f;
    HsBaWaamRobotType_t robot_type = HSBA_WAAM_ROBOT_ABB;
    std::string path_lua_script;
    std::string path_lua_func;
    std::string output_path;
    HsBaWaamProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
};

namespace
{

struct OwnedCString
{
    char* data = nullptr;

    OwnedCString() = default;
    explicit OwnedCString(const std::string& str)
    {
        if (!str.empty())
        {
            data = static_cast<char*>(std::malloc(str.size() + 1));
            if (data)
                std::memcpy(data, str.c_str(), str.size() + 1);
        }
    }

    OwnedCString(const OwnedCString&) = delete;
    OwnedCString& operator=(const OwnedCString&) = delete;

    OwnedCString(OwnedCString&& other) noexcept : data(std::exchange(other.data, nullptr)) {}
    OwnedCString& operator=(OwnedCString&& other) noexcept
    {
        if (this != &other)
        {
            std::free(data);
            data = std::exchange(other.data, nullptr);
        }
        return *this;
    }

    ~OwnedCString() { std::free(data); }

    char* release() { return std::exchange(data, nullptr); }
};

int CalculateLayerCount(const ModelInfo& info, float layer_height, float first_layer_height)
{
    float model_height = info.bbox_max.z() - info.bbox_min.z();
    if (model_height <= 0.0f)
        return 0;
    float remaining = model_height - first_layer_height;
    if (remaining <= 0.0f)
        return 1;
    return 1 + static_cast<int>(std::ceil(remaining / layer_height));
}

float GetLayerZ(int layer_index, float first_layer_height, float layer_height)
{
    if (layer_index == 0)
        return first_layer_height;
    return first_layer_height + layer_index * layer_height;
}

void ReportProgress(const InternalWaamConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

}  // anonymous namespace

InternalWaamConfig BuildWaamConfig(const HsBaWaamPipelineConfig_t* cfg, HsBaWaamProgressCallback cb, void* ud)
{
    InternalWaamConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.bead_width = cfg->bead_width;
    ic.travel_speed = cfg->travel_speed;
    ic.wire_feed_speed = cfg->wire_feed_speed;
    ic.arc_current = cfg->arc_current;
    ic.arc_voltage = cfg->arc_voltage;
    ic.gas_flow_rate = cfg->gas_flow_rate;
    ic.material = cfg->material;
    ic.welding_process = cfg->welding_process;
    ic.protection = cfg->protection;
    ic.protect_gas = cfg->protect_gas;
    ic.interpass_temperature = cfg->interpass_temperature;
    ic.robot_type = cfg->robot_type;
    ic.path_lua_script = cfg->path_lua_script ? cfg->path_lua_script : "";
    ic.path_lua_func = cfg->path_lua_func ? cfg->path_lua_func : "";
    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaWaamPipelineResult_t ToCResult(const InternalWaamResult& ir)
{
    OwnedCString output_path(ir.output_path);
    OwnedCString error(ir.error_message);

    HsBaWaamPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.output_path = output_path.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

Utils::Task<InternalWaamResult> RunWaamPipelineAsync(const InternalWaamConfig& cfg)
{
    InternalWaamResult result;
    auto start_time = std::chrono::steady_clock::now();

    try
    {
        // ========== Stage 1: Preprocess ==========
        ReportProgress(cfg, 0, "Loading model...");
        auto model = GetModel(cfg.model_name);
        if (!model)
        {
            model = LoadModel(cfg.model_name, cfg.model_path);
        }
        if (!model)
        {
            result.success = false;
            result.error_message = "Failed to load model: " + cfg.model_path;
            co_return result;
        }

        ModelInfo info;
        model->BoundingBox(info.bbox_min, info.bbox_max);
        info.volume = model->Volume();
        int total_layers = CalculateLayerCount(info, cfg.layer_height, cfg.first_layer_height);
        if (total_layers <= 0)
        {
            result.success = false;
            result.error_message = "Invalid model height";
            co_return result;
        }
        result.total_layers = total_layers;
        ReportProgress(cfg, 10, "Model loaded");

        // ========== Stage 2: Slicing (wire-feed process keeps open contours) ==========
        ReportProgress(cfg, 15, "Slicing...");
        std::vector<PolygonsD> layer_outlines(total_layers);
        std::vector<float> layer_z_heights(total_layers);
        float z_offset = info.bbox_min.z();

        for (int i = 0; i < total_layers; ++i)
        {
            float z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
            layer_z_heights[i] = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height);
            layer_outlines[i] = NormalizeUnSafePolygons(UnSafeSlice(*model, z));
            int progress = 15 + (i * 45) / total_layers;
            ReportProgress(cfg, progress, "Slicing bead layer");
        }
        ReportProgress(cfg, 60, "Slicing complete");

        // ========== Stage 3: Robot path export ==========
        // WAAM output is a robot language program built from the deposition
        // contours; a Lua script is optional (built-in ABB/KUKA/FANUC if absent).
        ReportProgress(cfg, 70, "Generating robot path...");

        std::string output_path = cfg.output_path;
        if (output_path.empty())
        {
            output_path = cfg.model_name + "_waam_robot.txt";
        }

        WaamRobotPackage pkg;
        pkg.layer_outlines = layer_outlines;
        pkg.layer_z_heights = layer_z_heights;
        pkg.bead_width = cfg.bead_width;
        pkg.robot_type = static_cast<int>(cfg.robot_type);
        pkg.weld.current = cfg.arc_current;
        pkg.weld.voltage = cfg.arc_voltage;
        pkg.weld.wire_feed_speed = cfg.wire_feed_speed;
        pkg.weld.gas_flow_rate = cfg.gas_flow_rate;
        pkg.weld.travel_speed = cfg.travel_speed;
        pkg.weld.process = (cfg.welding_process == HSBA_WAAM_WELD_LASER) ? 1 : 0;

        std::string lua_error;
        bool export_ok = SaveWaamRobotPath(pkg, output_path, cfg.path_lua_script,
                                           cfg.path_lua_func.empty() ? "export_waam" : cfg.path_lua_func, &lua_error);

        if (export_ok)
        {
            result.output_path = output_path;
            result.success = true;
        }
        else
        {
            result.success = false;
            result.error_message = "Failed to export WAAM robot path: " + lua_error;
        }

        ReportProgress(cfg, 100, "Pipeline complete");
    }
    catch (const std::exception& e)
    {
        result.success = false;
        result.error_message = std::string("Pipeline error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    co_return result;
}

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaWaamPipelineConfig_t HsBaCreateDefaultWaamConfig(void)
{
    return HsBaWaamConfigDefault();
}

HSBA_SLICER_API HsBaWaamPipelineResult_t HsBaRunWaamPipeline(const HsBaWaamPipelineConfig_t* config,
                                                             HsBaWaamProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildWaamConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunWaamPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunWaamPipelineAsync(const HsBaWaamPipelineConfig_t* config, HsBaWaamProgressCallback callback,
                                              void* user_data, HsBaWaamResultCallback result_callback,
                                              void* result_user_data)
{
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalWaamConfig>(
        HsBa::Slicer::Pipeline::BuildWaamConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunWaamPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalWaamResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreeWaamPipelineResult(HsBaWaamPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->output_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
