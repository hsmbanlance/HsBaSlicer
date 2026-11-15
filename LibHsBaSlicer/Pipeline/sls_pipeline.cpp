/** @file sls_pipeline.cpp
 * @brief SLS pipeline orchestration kernel implementation.
 *
 * Ported from the DllHsBaSlicer coroutine `RunSlsPipelineAsync`, made synchronous so the same
 * code drives the Dll blocking C entry, the job thread and the C++20 module. Stage order,
 * progress percentages, Lua package export, cooperative cancellation and geometry capture are
 * preserved byte-for-byte. SLS produces no infill or support, so only the contours are published.
 * @author HsBa
 */
#include "LibHsBaSlicer/Pipeline/sls_pipeline.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
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
    float first_layer_height = 0.15F;
    float laser_power = 30.0F;
    float scan_speed = 2000.0F;
    float hatch_spacing = 0.15F;
    float hatch_rotation = 90.0F;
    float bed_temperature = 180.0F;
    std::string export_lua_script;
    std::string export_lua_func;
    std::string output_path;
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

std::string BuildSlsConfigJson(const Cfg& cfg, int total_layers)
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

Cfg BuildCfg(const HsBaSlsPipelineConfig_t& cfg, const Hooks& hooks)
{
    Cfg ic;
    ic.model_name = cfg.model_name ? cfg.model_name : "";
    ic.model_path = cfg.model_path ? cfg.model_path : "";
    ic.layer_height = cfg.layer_height;
    ic.first_layer_height = cfg.first_layer_height;
    ic.laser_power = cfg.laser_power;
    ic.scan_speed = cfg.scan_speed;
    ic.hatch_spacing = cfg.hatch_spacing;
    ic.hatch_rotation = cfg.hatch_rotation;
    ic.bed_temperature = cfg.bed_temperature;
    ic.export_lua_script = cfg.export_lua_script ? cfg.export_lua_script : "";
    ic.export_lua_func = cfg.export_lua_func ? cfg.export_lua_func : "";
    ic.output_path = cfg.output_path ? cfg.output_path : "";
    ic.progress = hooks.progress;
    ic.capture_geometry = cfg.capture_geometry != 0;
    ic.cancel_flag = static_cast<const std::atomic<bool>*>(cfg.cancel_flag);
    return ic;
}

SlsOutput RunPipeline(const Cfg& cfg)
{
    SlsOutput result;
    auto start_time = std::chrono::steady_clock::now();

    // Load the AnyObject/Lua registration functions for common custom types into the generic registry pool for the
    // per-stage Lua environments to use
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
            },
            [&](int done)
            {
                int const progress = 15 + (done * 35) / total_layers;
                ReportProgress(cfg, progress, "Slicing layer");
            });
        cfg.CheckCancel();
        ReportProgress(cfg, 50, "Slicing complete");

        // ========== Stage 3: Export via Lua ==========
        // SLS has no standard output format; export is entirely through Lua script.
        // The Lua script handles zip archive creation and database registration.
        if (cfg.export_lua_script.empty())
        {
            result.success = false;
            result.error = "SLS export requires a Lua export script (export_lua_script)";
            return result;
        }

        // SLS produces no infill or support slots, so only the contours are published.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(layer_z_heights[i]);
            }
            result.geometry.valid = true;
            result.geometry.outlines = layer_outlines;
            result.geometry.zs = std::move(geometry_zs);
        }

        ReportProgress(cfg, 60, "Exporting via Lua script...");

        std::string output_path = cfg.output_path;
        if (output_path.empty())
        {
            output_path = cfg.model_name + "_sls_output.zip";
        }

        std::string const config_json = BuildSlsConfigJson(cfg, total_layers);

        SlsPackage pkg;
        pkg.layer_outlines = layer_outlines;
        pkg.layer_z_heights = layer_z_heights;
        pkg.config_json = config_json;

        std::string const func = cfg.export_lua_func.empty() ? "export_sls" : cfg.export_lua_func;
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
            result.error = "Failed to export SLS package via Lua script: " + lua_error;
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

SlsOutput RunSls(const HsBaSlsPipelineConfig_t& cfg, const Hooks& hooks)
{
    return RunPipeline(BuildCfg(cfg, hooks));
}

}  // namespace HsBa::Slicer::Kernel
