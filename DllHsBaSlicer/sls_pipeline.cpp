#include "sls_pipeline.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "base/coroutine.hpp"

namespace HsBa::Slicer::Pipeline
{

struct InternalSlsResult
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

struct InternalSlsConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.1f;
    float first_layer_height = 0.15f;
    float laser_power = 30.0f;
    float scan_speed = 2000.0f;
    float hatch_spacing = 0.15f;
    float hatch_rotation = 90.0f;
    float bed_temperature = 180.0f;
    std::string export_lua_script;
    std::string export_lua_func;
    std::string output_path;
    HsBaSlsProgressCallback progress_cb = nullptr;
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

void ReportProgress(const InternalSlsConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

std::string BuildSlsConfigJson(const InternalSlsConfig& cfg, int total_layers)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": \"1.0\",\n";
    json << "  \"process\": \"SLS\",\n";
    json << "  \"slice\": {\n";
    json << "    \"layer_height\": " << cfg.layer_height << ",\n";
    json << "    \"first_layer_height\": " << cfg.first_layer_height << ",\n";
    json << "    \"total_layers\": " << total_layers << "\n";
    json << "  },\n";
    json << "  \"laser\": {\n";
    json << "    \"power\": " << cfg.laser_power << ",\n";
    json << "    \"scan_speed\": " << cfg.scan_speed << ",\n";
    json << "    \"hatch_spacing\": " << cfg.hatch_spacing << ",\n";
    json << "    \"hatch_rotation\": " << cfg.hatch_rotation << "\n";
    json << "  },\n";
    json << "  \"bed_temperature\": " << cfg.bed_temperature << "\n";
    json << "}\n";
    return json.str();
}

}  // anonymous namespace

InternalSlsConfig BuildSlsConfig(const HsBaSlsPipelineConfig_t* cfg, HsBaSlsProgressCallback cb, void* ud)
{
    InternalSlsConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.laser_power = cfg->laser_power;
    ic.scan_speed = cfg->scan_speed;
    ic.hatch_spacing = cfg->hatch_spacing;
    ic.hatch_rotation = cfg->hatch_rotation;
    ic.bed_temperature = cfg->bed_temperature;
    ic.export_lua_script = cfg->export_lua_script ? cfg->export_lua_script : "";
    ic.export_lua_func = cfg->export_lua_func ? cfg->export_lua_func : "";
    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaSlsPipelineResult_t ToCResult(const InternalSlsResult& ir)
{
    OwnedCString export_path(ir.export_path);
    OwnedCString error(ir.error_message);

    HsBaSlsPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.export_path = export_path.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

Utils::Task<InternalSlsResult> RunSlsPipelineAsync(const InternalSlsConfig& cfg)
{
    InternalSlsResult result;
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

        // ========== Stage 2: Slicing ==========
        ReportProgress(cfg, 15, "Slicing...");
        std::vector<PolygonsD> layer_outlines(total_layers);
        std::vector<float> layer_z_heights(total_layers);
        float z_offset = info.bbox_min.z();

        for (int i = 0; i < total_layers; ++i)
        {
            float z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
            layer_z_heights[i] = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height);
            layer_outlines[i] = NormalizeUnSafePolygons(UnSafeSlice(*model, z));
            int progress = 15 + (i * 35) / total_layers;
            ReportProgress(cfg, progress, "Slicing layer");
        }
        ReportProgress(cfg, 50, "Slicing complete");

        // ========== Stage 3: Export via Lua ==========
        // SLS has no standard output format; export is entirely through Lua script.
        // The Lua script handles zip archive creation and database registration.
        if (cfg.export_lua_script.empty())
        {
            result.success = false;
            result.error_message = "SLS export requires a Lua export script (export_lua_script)";
            co_return result;
        }

        ReportProgress(cfg, 60, "Exporting via Lua script...");

        std::string output_path = cfg.output_path;
        if (output_path.empty())
        {
            output_path = cfg.model_name + "_sls_output.zip";
        }

        std::string config_json = BuildSlsConfigJson(cfg, total_layers);

        SlsPackage pkg;
        pkg.layer_outlines = layer_outlines;
        pkg.layer_z_heights = layer_z_heights;
        pkg.config_json = config_json;

        std::string func = cfg.export_lua_func.empty() ? "export_sls" : cfg.export_lua_func;
        bool export_ok = SaveSlsPackageLua(pkg, output_path, cfg.export_lua_script, func);

        if (export_ok)
        {
            result.export_path = output_path;
            result.success = true;
        }
        else
        {
            result.success = false;
            result.error_message = "Failed to export SLS package via Lua script";
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

HSBA_SLICER_API HsBaSlsPipelineConfig_t HsBaCreateDefaultSlsConfig(void)
{
    HsBaSlsPipelineConfig_t cfg{};

    cfg.layer_height = 0.1f;
    cfg.first_layer_height = 0.15f;
    cfg.laser_power = 30.0f;
    cfg.scan_speed = 2000.0f;
    cfg.hatch_spacing = 0.15f;
    cfg.hatch_rotation = 90.0f;
    cfg.bed_temperature = 180.0f;
    cfg.export_lua_script = nullptr;
    cfg.export_lua_func = nullptr;
    cfg.output_path = nullptr;

    return cfg;
}

HSBA_SLICER_API HsBaSlsPipelineResult_t HsBaRunSlsPipeline(const HsBaSlsPipelineConfig_t* config,
                                                            HsBaSlsProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildSlsConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunSlsPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunSlsPipelineAsync(const HsBaSlsPipelineConfig_t* config,
                                              HsBaSlsProgressCallback callback, void* user_data,
                                              HsBaSlsResultCallback result_callback,
                                              void* result_user_data)
{
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalSlsConfig>(
        HsBa::Slicer::Pipeline::BuildSlsConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunSlsPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalSlsResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreeSlsPipelineResult(HsBaSlsPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
