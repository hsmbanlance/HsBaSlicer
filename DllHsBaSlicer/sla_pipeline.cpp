/** @file sla_pipeline.cpp
 * @brief Implementation of the SLA (stereolithography) slicing pipeline C ABI.
 * @author HsBa
 */
#include "sla_pipeline.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "base/coroutine.hpp"
#include "base/error.hpp"
#include "geometry_store.hpp"
#include "job_control.h"
#include "pipeline_parallel.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

enum class SlaImageType : std::uint8_t
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
    bool cancelled = false;
    long long geometry_id = 0;  ///< Snapshot id of the captured layer geometry (0 = none)
};

struct InternalSlaConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.05F;
    float first_layer_height = 0.1F;
    float bottom_exposure_time = 60.0F;
    float normal_exposure_time = 2.5F;
    float bottom_lift_distance = 5.0F;
    float lift_distance = 3.0F;
    float lift_speed = 60.0F;
    float retract_speed = 150.0F;
    float floor_raft_offset = 2.0F;
    float floor_border_width = 1.0F;
    float floor_fill_spacing = 0.5F;
    float floor_fill_angle = 0.0F;
    int floor_border_count = 2;
    bool floor_use_convex_hull = false;
    bool enable_support = true;
    float overhang_angle = 45.0F;
    float support_gap = 0.5F;
    float support_diameter = 2.0F;
    float support_density = 0.3F;
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
    bool capture_geometry = false;
    const void* cancel_flag = nullptr;  ///< std::atomic<bool>* set by the host or by its job handle

    // Cancellation is cooperative and only observed at stage boundaries: throwing from the
    // throttled progress callback of the layer-parallel slicing stage would abandon sibling
    // worker tasks still writing into layer_outlines (see base/parallel.hpp). The parallel
    // layer functor therefore skips its remaining work as soon as the flag is set.
    /** @brief Whether the host asked this run to stop. */
    bool CancelRequested() const
    {
        return cancel_flag && static_cast<const std::atomic<bool>*>(cancel_flag)->load(std::memory_order_relaxed);
    }

    /** @brief Throw at a cancellation point so the run unwinds as a cancelled result. */
    void CheckCancel() const
    {
        if (CancelRequested())
        {
            throw CancelledError("pipeline cancelled");
        }
    }
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
    case SlaImageType::Jpg:
        return ".jpg";
    case SlaImageType::Svg:
        return ".svg";
    default:
        return ".png";
    }
}

const char* GetImageTypeLabel(SlaImageType type)
{
    switch (type)
    {
    case SlaImageType::Jpg:
        return "jpg";
    case SlaImageType::Svg:
        return "svg";
    default:
        return "png";
    }
}

std::string BuildConfigJson(const InternalSlaConfig& cfg, int total_layers)
{
    std::string const ext = GetImageExtension(cfg.image_type);
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
    json << R"(    "type": ")" << GetImageTypeLabel(cfg.image_type) << "\",\n";
    json << "    \"width\": " << cfg.image_width << ",\n";
    json << "    \"height\": " << cfg.image_height << "\n";
    json << "  },\n";
    json << "  \"layers\": [\n";
    for (int i = 0; i < total_layers; ++i)
    {
        bool const is_bottom = (i == 0);
        json << "    {\n";
        json << "      \"layer\": " << i << ",\n";
        json << R"(      "image_path": "layers/layer_)" << i << ext << "\",\n";
        json << "      \"exposure_time\": " << (is_bottom ? cfg.bottom_exposure_time : cfg.normal_exposure_time)
             << "\n";
        json << "    }";
        if (i < total_layers - 1)
        {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

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
    ic.capture_geometry = cfg->capture_geometry != 0;
    ic.cancel_flag = cfg->cancel_flag;
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
    cr.geometry_id = ir.geometry_id;
    return cr;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
Utils::Task<InternalSlaResult> RunSlaPipelineAsync(const InternalSlaConfig& cfg)
{
    // Load the AnyObject/Lua registration functions for common custom types into the generic registry pool for the
    // per-stage Lua environments to use
    HsBa::Slicer::InstallCommonAnyObjectTypes();

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
        float z_offset = info.bbox_min.z();

        // Build the slicing topology once and slice layers in parallel: each layer
        // is independent and SliceLayer only const-reads the shared topology.
        auto topo = BuildSliceTopology(*model);
        co_await ParallelForLayersAsync(
            total_layers,
            [&](int i)
            {
                if (cfg.CancelRequested())
                {
                    return;  // stop computing, the stage reports cancellation right after the await
                }
                float const z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
                layer_outlines[i] = SliceLayer(*topo, z);
            },
            [&](int done)
            {
                int const progress = 15 + (done * 20) / total_layers;
                ReportProgress(cfg, progress, "Slicing layer");
            });
        cfg.CheckCancel();
        ReportProgress(cfg, 35, "Slicing complete");

        // ========== Stage 3: Floor / Raft ==========
        cfg.CheckCancel();
        ReportProgress(cfg, 40, "Generating floor...");
        Polygons const bottom_int = Integerization(layer_outlines[0]);

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
            std::string const func = cfg.floor_lua_func.empty() ? "generate_floor" : cfg.floor_lua_func;
            Polygons const floor_int = LuaCustomFloorByFile(bottom_int, cfg.floor_lua_script, func, floor_cfg);
            floor_result_d = UnIntegerization(floor_int);
        }
        else
        {
            Polygons const floor_int = GenerateFloorRaft(bottom_int, floor_cfg);
            floor_result_d = UnIntegerization(floor_int);
        }
        ReportProgress(cfg, 50, "Floor generation complete");

        // ========== Stage 4: Support ==========
        cfg.CheckCancel();
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
                // Lua custom support via LibHsBaSlicer API (script content is required, load file first)
                std::string const func = cfg.support_lua_func.empty() ? "generate_support" : cfg.support_lua_func;
                std::ifstream ifs(cfg.support_lua_script);
                std::string const script_content((std::istreambuf_iterator<char>(ifs)),
                                                 std::istreambuf_iterator<char>());
                if (script_content.empty())
                {
                    result.success = false;
                    result.error_message = "Failed to read support Lua script: " + cfg.support_lua_script;
                    co_return result;
                }
                layer_supports = GenerateAllLuaSupport(layer_outlines, sla_support_cfg,
                                                       std::string_view(script_content), std::string_view(func));
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
        cfg.CheckCancel();
        ReportProgress(cfg, 75, "Exporting...");

        // Snapshot the contours before the package takes its own copy. The floor lives only
        // on layer 0, so it is published through the fills slot of that layer.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(GetLayerZ(i, cfg.first_layer_height, cfg.layer_height));
            }
            std::vector<PolygonsD> layer_fills(total_layers);
            layer_fills[0] = floor_result_d;
            result.geometry_id =
                CaptureLayerGeometry(HSBA_PIPELINE_SLA, layer_outlines, layer_fills, layer_supports, geometry_zs);
        }

        std::string output_zip = cfg.output_path;
        if (output_zip.empty())
        {
            output_zip = cfg.model_name + "_sla_output.zip";
        }

        std::string const config_json = BuildConfigJson(cfg, total_layers);

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
            std::string const func = cfg.export_lua_func.empty() ? "export_sla" : cfg.export_lua_func;
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
    catch (const CancelledError& e)
    {
        result.success = false;
        result.cancelled = true;
        // The "Cancelled" prefix is what the C ABI callers use to tell a cancelled run
        // apart from a failed one, since the result structs carry no status field.
        result.error_message = std::string("Cancelled: ") + e.what();
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

// Carries the caller's result callback across the job thread boundary.
struct SlaAsyncThunk
{
    HsBaSlaResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunSlaPipelineAsync.
 *
 * The job owns the temporary result struct and its strings, and releases both once this
 * callback returns, matching the copy-then-HsBaFreeSlaPipelineResult contract.
 */
void DeliverSlaResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<SlaAsyncThunk> owned(static_cast<SlaAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaSlaPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // anonymous namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaSlaPipelineConfig_t HsBaCreateDefaultSlaConfig(void)
{
    return HsBaSlaConfigDefault();
}

HSBA_SLICER_API HsBaSlaPipelineResult_t HsBaRunSlaPipeline(const HsBaSlaPipelineConfig_t* config,
                                                           HsBaSlaProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildSlaConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunSlaPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunSlaPipelineAsync(const HsBaSlaPipelineConfig_t* config, HsBaSlaProgressCallback callback,
                                             void* user_data, HsBaSlaResultCallback result_callback,
                                             void* result_user_data)
{
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::SlaAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_SLA, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverSlaResult, thunk);
}

HSBA_SLICER_API void HsBaFreeSlaPipelineResult(HsBaSlaPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
