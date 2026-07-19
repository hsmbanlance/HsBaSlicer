#include "sla_pipeline.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "base/coroutine.hpp"

namespace HsBa::Slicer::Pipeline
{

enum class SlaImageType
{
    Png = 0,
    Jpg = 1,
    Svg = 2
};

struct InternalSlaResult
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

struct InternalSlaConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.05f;
    float first_layer_height = 0.1f;
    float bottom_exposure_time = 60.0f;
    float normal_exposure_time = 2.5f;
    float bottom_lift_distance = 5.0f;
    float lift_distance = 3.0f;
    float lift_speed = 60.0f;
    float retract_speed = 150.0f;
    float floor_raft_offset = 2.0f;
    float floor_border_width = 1.0f;
    float floor_fill_spacing = 0.5f;
    float floor_fill_angle = 0.0f;
    int floor_border_count = 2;
    bool floor_use_convex_hull = false;
    bool enable_support = true;
    float overhang_angle = 45.0f;
    float support_gap = 0.5f;
    float support_diameter = 2.0f;
    float support_density = 0.3f;
    int support_pattern = 0;
    std::string support_lua_script;
    std::string support_lua_func;
    std::string floor_lua_script;
    std::string floor_lua_func;
    std::string export_lua_script;
    std::string export_lua_func;
    std::string output_path;
    SlaImageType image_type = SlaImageType::Png;
    int image_width = 0;
    int image_height = 0;
    HsBaSlaProgressCallback progress_cb = nullptr;
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

void ReportProgress(const InternalSlaConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

std::string GetImageExtension(SlaImageType type)
{
    switch (type)
    {
        case SlaImageType::Jpg: return ".jpg";
        case SlaImageType::Svg: return ".svg";
        default: return ".png";
    }
}

std::string BuildConfigJson(const InternalSlaConfig& cfg, int total_layers)
{
    std::string ext = GetImageExtension(cfg.image_type);
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": \"1.0\",\n";
    json << "  \"slice\": {\n";
    json << "    \"layer_height\": " << cfg.layer_height << ",\n";
    json << "    \"first_layer_height\": " << cfg.first_layer_height << ",\n";
    json << "    \"total_layers\": " << total_layers << "\n";
    json << "  },\n";
    json << "  \"exposure\": {\n";
    json << "    \"bottom_exposure_time\": " << cfg.bottom_exposure_time << ",\n";
    json << "    \"normal_exposure_time\": " << cfg.normal_exposure_time << "\n";
    json << "  },\n";
    json << "  \"lift\": {\n";
    json << "    \"bottom_lift_distance\": " << cfg.bottom_lift_distance << ",\n";
    json << "    \"lift_distance\": " << cfg.lift_distance << ",\n";
    json << "    \"lift_speed\": " << cfg.lift_speed << ",\n";
    json << "    \"retract_speed\": " << cfg.retract_speed << "\n";
    json << "  },\n";
    json << "  \"floor\": {\n";
    json << "    \"raft_offset\": " << cfg.floor_raft_offset << ",\n";
    json << "    \"border_width\": " << cfg.floor_border_width << ",\n";
    json << "    \"fill_spacing\": " << cfg.floor_fill_spacing << ",\n";
    json << "    \"fill_angle\": " << cfg.floor_fill_angle << ",\n";
    json << "    \"border_count\": " << cfg.floor_border_count << ",\n";
    json << "    \"use_convex_hull\": " << (cfg.floor_use_convex_hull ? "true" : "false") << "\n";
    json << "  },\n";
    json << "  \"support\": {\n";
    json << "    \"enable\": " << (cfg.enable_support ? "true" : "false") << ",\n";
    json << "    \"overhang_angle\": " << cfg.overhang_angle << ",\n";
    json << "    \"gap\": " << cfg.support_gap << ",\n";
    json << "    \"diameter\": " << cfg.support_diameter << ",\n";
    json << "    \"density\": " << cfg.support_density << ",\n";
    json << "    \"pattern\": " << cfg.support_pattern << "\n";
    json << "  },\n";
    json << "  \"image\": {\n";
    json << "    \"type\": \"" << (cfg.image_type == SlaImageType::Jpg ? "jpg" : cfg.image_type == SlaImageType::Svg ? "svg" : "png") << "\",\n";
    json << "    \"width\": " << cfg.image_width << ",\n";
    json << "    \"height\": " << cfg.image_height << "\n";
    json << "  },\n";
    json << "  \"layers\": [\n";
    for (int i = 0; i < total_layers; ++i)
    {
        bool is_bottom = (i == 0);
        json << "    {\n";
        json << "      \"layer\": " << i << ",\n";
        json << "      \"image_path\": \"layers/layer_" << i << ext << "\",\n";
        json << "      \"exposure_time\": " << (is_bottom ? cfg.bottom_exposure_time : cfg.normal_exposure_time) << "\n";
        json << "    }";
        if (i < total_layers - 1)
            json << ",";
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

}  // anonymous namespace

InternalSlaConfig BuildSlaConfig(const HsBaSlaPipelineConfig_t* cfg, HsBaSlaProgressCallback cb, void* ud)
{
    InternalSlaConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.bottom_exposure_time = cfg->bottom_exposure_time;
    ic.normal_exposure_time = cfg->normal_exposure_time;
    ic.bottom_lift_distance = cfg->bottom_lift_distance;
    ic.lift_distance = cfg->lift_distance;
    ic.lift_speed = cfg->lift_speed;
    ic.retract_speed = cfg->retract_speed;
    ic.floor_raft_offset = cfg->floor_raft_offset;
    ic.floor_border_width = cfg->floor_border_width;
    ic.floor_fill_spacing = cfg->floor_fill_spacing;
    ic.floor_fill_angle = cfg->floor_fill_angle;
    ic.floor_border_count = cfg->floor_border_count;
    ic.floor_use_convex_hull = cfg->floor_use_convex_hull != 0;
    ic.enable_support = cfg->enable_support != 0;
    ic.overhang_angle = cfg->overhang_angle;
    ic.support_gap = cfg->support_gap;
    ic.support_diameter = cfg->support_diameter;
    ic.support_density = cfg->support_density;
    ic.support_pattern = static_cast<int>(cfg->support_pattern);
    ic.support_lua_script = cfg->support_lua_script ? cfg->support_lua_script : "";
    ic.support_lua_func = cfg->support_lua_func ? cfg->support_lua_func : "";
    ic.floor_lua_script = cfg->floor_lua_script ? cfg->floor_lua_script : "";
    ic.floor_lua_func = cfg->floor_lua_func ? cfg->floor_lua_func : "";
    ic.export_lua_script = cfg->export_lua_script ? cfg->export_lua_script : "";
    ic.export_lua_func = cfg->export_lua_func ? cfg->export_lua_func : "";
    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.image_type = static_cast<SlaImageType>(static_cast<int>(cfg->image_type));
    ic.image_width = cfg->image_width;
    ic.image_height = cfg->image_height;
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaSlaPipelineResult_t ToCResult(const InternalSlaResult& ir)
{
    OwnedCString export_path(ir.export_path);
    OwnedCString error(ir.error_message);

    HsBaSlaPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.export_path = export_path.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

Utils::Task<InternalSlaResult> RunSlaPipelineAsync(const InternalSlaConfig& cfg)
{
    InternalSlaResult result;
    auto start_time = std::chrono::steady_clock::now();

    try
    {
        // ========== Stage 1: Preprocess ==========
        // Use LibHsBaSlicer model management (thread-local pool)
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
        float z_offset = info.bbox_min.z();

        for (int i = 0; i < total_layers; ++i)
        {
            float z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
            layer_outlines[i] = NormalizeUnSafePolygons(UnSafeSlice(*model, z));
            int progress = 15 + (i * 20) / total_layers;
            ReportProgress(cfg, progress, "Slicing layer");
        }
        ReportProgress(cfg, 35, "Slicing complete");

        // ========== Stage 3: Floor / Raft ==========
        ReportProgress(cfg, 40, "Generating floor...");
        Polygons bottom_int = Integerization(layer_outlines[0]);

        SlaFloorConfig floor_cfg;
        floor_cfg.raft_offset = cfg.floor_raft_offset;
        floor_cfg.border_width = cfg.floor_border_width;
        floor_cfg.fill_spacing = cfg.floor_fill_spacing;
        floor_cfg.fill_angle_deg = cfg.floor_fill_angle;
        floor_cfg.border_count = cfg.floor_border_count;
        floor_cfg.use_convex_hull = cfg.floor_use_convex_hull;

        PolygonsD floor_result_d;
        if (!cfg.floor_lua_script.empty())
        {
            std::string func = cfg.floor_lua_func.empty() ? "generate_floor" : cfg.floor_lua_func;
            Polygons floor_int = LuaCustomFloorByFile(bottom_int, cfg.floor_lua_script, func, floor_cfg);
            floor_result_d = UnIntegerization(floor_int);
        }
        else
        {
            Polygons floor_int = GenerateFloorRaft(bottom_int, floor_cfg);
            floor_result_d = UnIntegerization(floor_int);
        }
        ReportProgress(cfg, 50, "Floor generation complete");

        // ========== Stage 4: Support ==========
        std::vector<PolygonsD> layer_supports(total_layers);
        if (cfg.enable_support)
        {
            ReportProgress(cfg, 55, "Generating supports...");

            Support::SlaSupportConfig sla_support_cfg;
            sla_support_cfg.overhang_angle_threshold = cfg.overhang_angle;
            sla_support_cfg.layer_height = cfg.layer_height;
            sla_support_cfg.support_gap = cfg.support_gap;
            sla_support_cfg.support_diameter = cfg.support_diameter;
            sla_support_cfg.support_density = cfg.support_density;
            sla_support_cfg.support_pattern = cfg.support_pattern;

            if (!cfg.support_lua_script.empty())
            {
                // Lua custom support via LibHsBaSlicer API
                std::string func = cfg.support_lua_func.empty() ? "generate_support" : cfg.support_lua_func;
                layer_supports = GenerateAllLuaSupport(layer_outlines, sla_support_cfg,
                                                       std::string_view(cfg.support_lua_script),
                                                       std::string_view(func));
            }
            else
            {
                layer_supports = GenerateAllSlaSupport(layer_outlines, sla_support_cfg);
            }
            ReportProgress(cfg, 70, "Support generation complete");
        }
        else
        {
            ReportProgress(cfg, 70, "Support disabled");
        }

        // ========== Stage 5: Export via LibHsBaSlicer SlaPackage ==========
        ReportProgress(cfg, 75, "Exporting...");

        std::string output_zip = cfg.output_path;
        if (output_zip.empty())
        {
            output_zip = cfg.model_name + "_sla_output.zip";
        }

        std::string config_json = BuildConfigJson(cfg, total_layers);

        SlaPackage pkg;
        pkg.layer_outlines = layer_outlines;
        pkg.layer_supports = layer_supports;
        pkg.floor_polygons = floor_result_d;
        pkg.config_json = config_json;
        pkg.image_width = cfg.image_width;
        pkg.image_height = cfg.image_height;
        pkg.image_extension = GetImageExtension(cfg.image_type);
        pkg.include_floor_images = true;
        pkg.include_support_images = cfg.enable_support;

        bool export_ok = false;
        if (!cfg.export_lua_script.empty())
        {
            std::string func = cfg.export_lua_func.empty() ? "export_sla" : cfg.export_lua_func;
            export_ok = SaveSlaPackageLua(pkg, output_zip, cfg.export_lua_script, func);
        }
        else
        {
            export_ok = SaveSlaPackage(pkg, output_zip);
        }

        if (export_ok)
        {
            result.export_path = output_zip;
            result.success = true;
        }
        else
        {
            result.success = false;
            result.error_message = "Failed to export SLA package";
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

HSBA_SLICER_API HsBaSlaPipelineConfig_t HsBaCreateDefaultSlaConfig(void)
{
    HsBaSlaPipelineConfig_t cfg{};

    cfg.layer_height = 0.05f;
    cfg.first_layer_height = 0.1f;
    cfg.bottom_exposure_time = 60.0f;
    cfg.normal_exposure_time = 2.5f;
    cfg.bottom_lift_distance = 5.0f;
    cfg.lift_distance = 3.0f;
    cfg.lift_speed = 60.0f;
    cfg.retract_speed = 150.0f;
    cfg.floor_raft_offset = 2.0f;
    cfg.floor_border_width = 1.0f;
    cfg.floor_fill_spacing = 0.5f;
    cfg.floor_fill_angle = 0.0f;
    cfg.floor_border_count = 2;
    cfg.floor_use_convex_hull = 0;
    cfg.enable_support = 1;
    cfg.overhang_angle = 45.0f;
    cfg.support_gap = 0.5f;
    cfg.support_diameter = 2.0f;
    cfg.support_density = 0.3f;
    cfg.support_pattern = HSBA_SLA_SUPPORT_SACRIFICIAL;
    cfg.support_lua_script = nullptr;
    cfg.support_lua_func = nullptr;
    cfg.floor_lua_script = nullptr;
    cfg.floor_lua_func = nullptr;
    cfg.export_lua_script = nullptr;
    cfg.export_lua_func = nullptr;
    cfg.output_path = nullptr;
    cfg.image_type = HSBA_SLA_IMAGE_PNG;
    cfg.image_width = 0;
    cfg.image_height = 0;

    return cfg;
}

HSBA_SLICER_API HsBaSlaPipelineResult_t HsBaRunSlaPipeline(const HsBaSlaPipelineConfig_t* config,
                                                           HsBaSlaProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildSlaConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunSlaPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunSlaPipelineAsync(const HsBaSlaPipelineConfig_t* config,
                                              HsBaSlaProgressCallback callback, void* user_data,
                                              HsBaSlaResultCallback result_callback,
                                              void* result_user_data)
{
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalSlaConfig>(
        HsBa::Slicer::Pipeline::BuildSlaConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunSlaPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalSlaResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreeSlaPipelineResult(HsBaSlaPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
