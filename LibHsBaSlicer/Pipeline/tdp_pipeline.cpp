/** @file tdp_pipeline.cpp
 * @brief 3DP pipeline orchestration kernel implementation.
 *
 * Ported from the DllHsBaSlicer coroutine `RunTdpPipelineAsync`, made synchronous so the same
 * code drives the Dll blocking C entry, the job thread and the C++20 module. 3DP binder-jet
 * outlines are packaged like SLS via a Lua export script carrying the binder parameters. Stage
 * order, progress percentages, seam staggering, spiral path derivation, cooperative cancellation
 * and geometry capture are preserved byte-for-byte.
 * @author HsBa
 */
#include "LibHsBaSlicer/Pipeline/tdp_pipeline.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "base/error.hpp"
#include "base/parallel.hpp"

namespace HsBa::Slicer::Kernel
{

using HsBa::Slicer::Pipeline::ParallelForLayers;
namespace
{

struct Cfg
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
    std::function<void(int, std::string_view)> progress;
    bool capture_geometry = false;
    const std::atomic<bool>* cancel_flag = nullptr;

    bool CancelRequested() const
    {
        return cancel_flag && cancel_flag->load(std::memory_order_relaxed);
    }

    void CheckCancel() const
    {
        if (CancelRequested())
        {
            throw CancelledError("pipeline cancelled");
        }
    }
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

void ReportProgress(const Cfg& cfg, int percent, const std::string& stage)
{
    if (cfg.progress)
    {
        cfg.progress(percent, std::string_view(stage));
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

std::string BuildTdpConfigJson(const Cfg& cfg, int total_layers)
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

Cfg BuildCfg(const HsBaTdpPipelineConfig_t& cfg, const Hooks& hooks)
{
    Cfg ic;
    ic.model_name = cfg.model_name ? cfg.model_name : "";
    ic.model_path = cfg.model_path ? cfg.model_path : "";
    ic.layer_height = cfg.layer_height;
    ic.first_layer_height = cfg.first_layer_height;
    ic.head_count = cfg.head_count;
    ic.drop_spacing = cfg.drop_spacing;
    ic.binder_saturation = cfg.binder_saturation;
    ic.ink_curing_time = cfg.ink_curing_time;
    ic.bed_temperature = cfg.bed_temperature;
    ic.binder_mode = cfg.binder_mode;
    ic.export_lua_script = cfg.export_lua_script ? cfg.export_lua_script : "";
    ic.export_lua_func = cfg.export_lua_func ? cfg.export_lua_func : "";
    ic.output_path = cfg.output_path ? cfg.output_path : "";
    ic.spiral_mode = cfg.spiral_mode != 0;
    ic.randomize_start_point = cfg.randomize_start_point != 0;
    ic.progress = hooks.progress;
    ic.capture_geometry = cfg.capture_geometry != 0;
    ic.cancel_flag = static_cast<const std::atomic<bool>*>(cfg.cancel_flag);
    return ic;
}

TdpOutput RunPipeline(const Cfg& cfg)
{
    TdpOutput result;
    auto start_time = std::chrono::steady_clock::now();

    InstallCommonAnyObjectTypes();

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
            result.error = "Failed to load model: " + cfg.model_path;
            return result;
        }

        // Fetch bbox/volume through the Lib model funnel so any third-party
        // geometry exception is translated into the project's RuntimeError family.
        ModelInfo info = GetModelInfo(cfg.model_name);
        int total_layers = CalculateLayerCount(info, cfg.layer_height, cfg.first_layer_height);
        if (total_layers <= 0)
        {
            result.success = false;
            result.error = "Invalid model height";
            return result;
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
        ParallelForLayers(
            total_layers,
            [&](int i)
            {
                if (cfg.CancelRequested())
                {
                    return;  // stop computing, the stage reports cancellation right after the loop
                }
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
        cfg.CheckCancel();
        ReportProgress(cfg, 50, "Slicing complete");

        // ========== Stage 3: Export via Lua ==========
        // 3DP binder-jet outlines are packaged like SLS (zip + optional
        // database) via a Lua export script carrying the binder parameters.
        if (cfg.export_lua_script.empty())
        {
            result.success = false;
            result.error = "3DP export requires a Lua export script (export_lua_script)";
            return result;
        }

        // 3DP has no infill or support slots, so only the (already seam-rotated) outlines
        // are published.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(layer_z_heights[i]);
            }
            // Spiral runs deposit one continuous rising path, so publish the same helix the
            // exporter derives from these contours (SpiralizeOuterWall is a pure function).
            std::vector<double> path_xyz;
            if (cfg.spiral_mode)
            {
                const std::vector<SpiralPoint> helix = SpiralizeOuterWall(layer_outlines, geometry_zs);
                path_xyz.reserve(helix.size() * 3);
                for (const SpiralPoint& point : helix)
                {
                    path_xyz.push_back(point.x);
                    path_xyz.push_back(point.y);
                    path_xyz.push_back(point.z);
                }
            }
            result.geometry.valid = true;
            result.geometry.outlines = layer_outlines;
            result.geometry.zs = std::move(geometry_zs);
            result.geometry.path_xyz = std::move(path_xyz);
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
            result.error = "Failed to export 3DP package via Lua script: " + lua_error;
        }

        ReportProgress(cfg, 100, "Pipeline complete");
    }
    catch (const CancelledError& e)
    {
        result.success = false;
        result.cancelled = true;
        result.geometry.valid = false;
        // The "Cancelled" prefix is what the C ABI callers use to tell a cancelled run
        // apart from a failed one, since the result structs carry no status field.
        result.error = std::string("Cancelled: ") + e.what();
    }
    catch (const RuntimeError& e)
    {
        result.success = false;
        result.error = std::string("Pipeline error: ") + e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

    return result;
}

}  // namespace

TdpOutput RunTdp(const HsBaTdpPipelineConfig_t& cfg, const Hooks& hooks)
{
    return RunPipeline(BuildCfg(cfg, hooks));
}

}  // namespace HsBa::Slicer::Kernel
