/** @file tdp_pipeline.cpp
 * @brief Implementation of the 3DP (three-dimensional printing) slicing pipeline C ABI.
 * @author HsBa
 */
#include "tdp_pipeline.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "base/coroutine.hpp"
#include "base/error.hpp"
#include "pipeline_parallel.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

struct InternalTdpResult
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

struct InternalTdpConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.1F;
    float first_layer_height = 0.12F;
    int head_count = 128;
    float drop_spacing = 0.05F;
    float binder_saturation = 0.6F;
    float ink_curing_time = 1.0F;
    float bed_temperature = 40.0F;
    HsBaTdpBinderMode_t binder_mode = HSBA_TDP_SINGLE;
    std::string export_lua_script;
    std::string export_lua_func;
    std::string output_path;
    bool spiral_mode = false;
    bool randomize_start_point = false;
    HsBaTdpProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
};

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
            {
                std::memcpy(data, str.c_str(), str.size() + 1);
            }
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
    float const model_height = info.bbox_max.z() - info.bbox_min.z();
    if (model_height <= 0.0F)
    {
        return 0;
    }
    float const remaining = model_height - first_layer_height;
    if (remaining <= 0.0F)
    {
        return 1;
    }
    return 1 + static_cast<int>(std::ceil(remaining / layer_height));
}

float GetLayerZ(int layer_index, float first_layer_height, float layer_height)
{
    if (layer_index == 0)
    {
        return first_layer_height;
    }
    return first_layer_height + static_cast<float>(layer_index) * layer_height;
}

void ReportProgress(const InternalTdpConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

const char* BinderModeName(HsBaTdpBinderMode_t m)
{
    switch (m)
    {
    case HSBA_TDP_FULL_COLOR:
        return "full_color";
    case HSBA_TDP_SINGLE:
        return "single";
    case HSBA_TDP_SINTERING:
        return "sintering";
    default:
        return "single";
    }
}

std::string BuildTdpConfigJson(const InternalTdpConfig& cfg, int total_layers)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": \"1.0\",\n";
    json << "  \"process\": \"3DP\",\n";
    json << "  \"slice\": {\n";
    json << "    \"layer_height\": " << cfg.layer_height << ",\n";
    json << "    \"first_layer_height\": " << cfg.first_layer_height << ",\n";
    json << "    \"total_layers\": " << total_layers << "\n";
    json << "  },\n";
    json << "  \"binder\": {\n";
    json << "    \"head_count\": " << cfg.head_count << ",\n";
    json << "    \"drop_spacing\": " << cfg.drop_spacing << ",\n";
    json << "    \"saturation\": " << cfg.binder_saturation << ",\n";
    json << "    \"curing_time\": " << cfg.ink_curing_time << ",\n";
    json << R"(    "mode": ")" << BinderModeName(cfg.binder_mode) << "\"\n";
    json << "  },\n";
    json << "  \"bed_temperature\": " << cfg.bed_temperature << ",\n";
    json << "  \"spiral_mode\": " << (cfg.spiral_mode ? "true" : "false") << ",\n";
    json << "  \"randomize_start_point\": " << (cfg.randomize_start_point ? "true" : "false") << "\n";
    json << "}\n";
    return json.str();
}

InternalTdpConfig BuildTdpConfig(const HsBaTdpPipelineConfig_t* cfg, HsBaTdpProgressCallback cb, void* ud)
{
    InternalTdpConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.head_count = cfg->head_count;
    ic.drop_spacing = cfg->drop_spacing;
    ic.binder_saturation = cfg->binder_saturation;
    ic.ink_curing_time = cfg->ink_curing_time;
    ic.bed_temperature = cfg->bed_temperature;
    ic.binder_mode = cfg->binder_mode;
    ic.export_lua_script = cfg->export_lua_script ? cfg->export_lua_script : "";
    ic.export_lua_func = cfg->export_lua_func ? cfg->export_lua_func : "";
    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.spiral_mode = cfg->spiral_mode != 0;
    ic.randomize_start_point = cfg->randomize_start_point != 0;
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaTdpPipelineResult_t ToCResult(const InternalTdpResult& ir)
{
    OwnedCString export_path(ir.export_path);
    OwnedCString error(ir.error_message);

    HsBaTdpPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.export_path = export_path.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
Utils::Task<InternalTdpResult> RunTdpPipelineAsync(const InternalTdpConfig& cfg)
{
    HsBa::Slicer::InstallCommonAnyObjectTypes();

    InternalTdpResult result;
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

        // Fetch bbox/volume through the Lib model funnel so any third-party
        // geometry exception is translated into the project's RuntimeError family.
        ModelInfo info = GetModelInfo(cfg.model_name);
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

        // Build the slicing topology once and slice layers in parallel: each layer
        // is independent and SliceLayer only const-reads the shared topology.
        auto topo = BuildSliceTopology(*model);
        co_await ParallelForLayersAsync(
            total_layers,
            [&](int i)
            {
                float const z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
                layer_z_heights[i] = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height);
                layer_outlines[i] = SliceLayer(*topo, z);
                if (cfg.randomize_start_point)
                {
                    // Stagger deposition seams: rotate each closed contour to a
                    // per-layer deterministic random start point on an edge interior.
                    Polygons const int_polys = Integerization(layer_outlines[i]);
                    layer_outlines[i] =
                        UnIntegerization(RandomizePathStartPoints(int_polys, static_cast<std::uint64_t>(i) + 1ULL));
                }
            },
            [&](int done)
            {
                int const progress = 15 + (done * 35) / total_layers;
                ReportProgress(cfg, progress, "Slicing layer");
            });
        ReportProgress(cfg, 50, "Slicing complete");

        // ========== Stage 3: Export via Lua ==========
        // 3DP binder-jet outlines are packaged like SLS (zip + optional
        // database) via a Lua export script carrying the binder parameters.
        if (cfg.export_lua_script.empty())
        {
            result.success = false;
            result.error_message = "3DP export requires a Lua export script (export_lua_script)";
            co_return result;
        }

        ReportProgress(cfg, 60, "Exporting via Lua script...");

        std::string output_path = cfg.output_path;
        if (output_path.empty())
        {
            output_path = cfg.model_name + "_3dp_output.zip";
        }

        std::string const config_json = BuildTdpConfigJson(cfg, total_layers);

        SlsPackage pkg;
        pkg.layer_outlines = layer_outlines;
        pkg.layer_z_heights = layer_z_heights;
        pkg.config_json = config_json;
        pkg.spiral_mode = cfg.spiral_mode;

        std::string const func = cfg.export_lua_func.empty() ? "export_tdp" : cfg.export_lua_func;
        std::string lua_error;
        bool const export_ok = SaveSlsPackageLua(pkg, output_path, cfg.export_lua_script, func, &lua_error);

        if (export_ok)
        {
            result.export_path = output_path;
            result.success = true;
        }
        else
        {
            result.success = false;
            result.error_message = "Failed to export 3DP package via Lua script: " + lua_error;
        }

        ReportProgress(cfg, 100, "Pipeline complete");
    }
    catch (const RuntimeError& e)
    {
        result.success = false;
        result.error_message = std::string("Pipeline error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    co_return result;
}

}  // anonymous namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaTdpPipelineConfig_t HsBaCreateDefaultTdpConfig(void)
{
    return HsBaTdpConfigDefault();
}

HSBA_SLICER_API HsBaTdpPipelineResult_t HsBaRunTdpPipeline(const HsBaTdpPipelineConfig_t* config,
                                                           HsBaTdpProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildTdpConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunTdpPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunTdpPipelineAsync(const HsBaTdpPipelineConfig_t* config, HsBaTdpProgressCallback callback,
                                             void* user_data, HsBaTdpResultCallback result_callback,
                                             void* result_user_data)
{
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalTdpConfig>(
        HsBa::Slicer::Pipeline::BuildTdpConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunTdpPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](const HsBa::Slicer::Pipeline::InternalTdpResult& ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });  // NOLINT(clang-analyzer-unix.Malloc) C-ABI ownership transfer: result_callback frees cr strings
}

HSBA_SLICER_API void HsBaFreeTdpPipelineResult(HsBaTdpPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
