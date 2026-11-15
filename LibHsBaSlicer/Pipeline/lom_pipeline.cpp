/** @file lom_pipeline.cpp
 * @brief LOM pipeline orchestration kernel implementation.
 *
 * Ported from the DllHsBaSlicer coroutine `RunLomPipelineAsync`, made synchronous so the same
 * code drives the Dll blocking C entry, the job thread and the C++20 module. LOM cuts/bonds
 * sheet-by-sheet; the contour outlines are packaged the same way as SLS via a Lua export script.
 * Stage order, progress percentages, cooperative cancellation and geometry capture are preserved
 * byte-for-byte.
 * @author HsBa
 */
#include "LibHsBaSlicer/Pipeline/lom_pipeline.hpp"

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
    float layer_height = 0.2F;
    float first_layer_height = 0.2F;
    float cut_speed = 300.0F;
    float cut_margin = 0.5F;
    float cut_power = 0.8F;
    float bond_temperature = 150.0F;
    float bond_pressure = 1.0F;
    float bond_time = 5.0F;
    int seal_contour = 1;
    HsBaLomCutMode_t cut_mode = HSBA_LOM_CUT_CONTOUR;
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

std::string BuildLomConfigJson(const Cfg& cfg, int total_layers)
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": \"1.0\",\n";
    json << "  \"process\": \"LOM\",\n";
    json << "  \"slice\": {\n";
    json << "    \"layer_height\": " << cfg.layer_height << ",\n";
    json << "    \"first_layer_height\": " << cfg.first_layer_height << ",\n";
    json << "    \"total_layers\": " << total_layers << "\n";
    json << "  },\n";
    json << "  \"cut\": {\n";
    json << "    \"speed\": " << cfg.cut_speed << ",\n";
    json << "    \"margin\": " << cfg.cut_margin << ",\n";
    json << "    \"power\": " << cfg.cut_power << ",\n";
    json << "    \"seal_contour\": " << (cfg.seal_contour ? "true" : "false") << ",\n";
    json << R"(    "mode": ")" << (cfg.cut_mode == HSBA_LOM_CUT_HALFTONE ? "halftone" : "contour") << "\"\n";
    json << "  },\n";
    json << "  \"bond\": {\n";
    json << "    \"temperature\": " << cfg.bond_temperature << ",\n";
    json << "    \"pressure\": " << cfg.bond_pressure << ",\n";
    json << "    \"time\": " << cfg.bond_time << "\n";
    json << "  }\n";
    json << "}\n";
    return json.str();
}

Cfg BuildCfg(const HsBaLomPipelineConfig_t& cfg, const Hooks& hooks)
{
    Cfg ic;
    ic.model_name = cfg.model_name ? cfg.model_name : "";
    ic.model_path = cfg.model_path ? cfg.model_path : "";
    ic.layer_height = cfg.layer_height;
    ic.first_layer_height = cfg.first_layer_height;
    ic.cut_speed = cfg.cut_speed;
    ic.cut_margin = cfg.cut_margin;
    ic.cut_power = cfg.cut_power;
    ic.bond_temperature = cfg.bond_temperature;
    ic.bond_pressure = cfg.bond_pressure;
    ic.bond_time = cfg.bond_time;
    ic.seal_contour = cfg.seal_contour;
    ic.cut_mode = cfg.cut_mode;
    ic.export_lua_script = cfg.export_lua_script ? cfg.export_lua_script : "";
    ic.export_lua_func = cfg.export_lua_func ? cfg.export_lua_func : "";
    ic.output_path = cfg.output_path ? cfg.output_path : "";
    ic.progress = hooks.progress;
    ic.capture_geometry = cfg.capture_geometry != 0;
    ic.cancel_flag = static_cast<const std::atomic<bool>*>(cfg.cancel_flag);
    return ic;
}

LomOutput RunPipeline(const Cfg& cfg)
{
    LomOutput result;
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

        // ========== Stage 2: Slicing (one contour set per sheet) ==========
        ReportProgress(cfg, 15, "Slicing...");
        std::vector<PolygonsD> layer_outlines(total_layers);
        std::vector<float> layer_z_heights(total_layers);
        float z_offset = info.bbox_min.z();

        // Build the slicing topology once and slice sheets in parallel: each layer
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
                ReportProgress(cfg, progress, "Slicing sheet");
            });
        cfg.CheckCancel();
        ReportProgress(cfg, 50, "Slicing complete");

        // ========== Stage 3: Export via Lua ==========
        // LOM cuts/bonds sheet-by-sheet; the contour outlines are packaged the
        // same way as SLS (zip + optional database) via a Lua export script.
        if (cfg.export_lua_script.empty())
        {
            result.success = false;
            result.error = "LOM export requires a Lua export script (export_lua_script)";
            return result;
        }

        // LOM has no infill or support slots, so only the sheet outlines are published.
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
            output_path = cfg.model_name + "_lom_output.zip";
        }

        std::string const config_json = BuildLomConfigJson(cfg, total_layers);

        SlsPackage pkg;
        pkg.layer_outlines = layer_outlines;
        pkg.layer_z_heights = layer_z_heights;
        pkg.config_json = config_json;

        std::string const func = cfg.export_lua_func.empty() ? "export_lom" : cfg.export_lua_func;
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
            result.error = "Failed to export LOM package via Lua script: " + lua_error;
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

LomOutput RunLom(const HsBaLomPipelineConfig_t& cfg, const Hooks& hooks)
{
    return RunPipeline(BuildCfg(cfg, hooks));
}

}  // namespace HsBa::Slicer::Kernel
