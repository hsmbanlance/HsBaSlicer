/** @file lom_pipeline.cpp
 * @brief Implementation of the LOM (laminated object manufacturing) slicing pipeline C ABI.
 * @author HsBa
 */
#include "lom_pipeline.h"

#include <atomic>
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
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "base/coroutine.hpp"
#include "base/error.hpp"
#include "geometry_store.hpp"
#include "job_control.h"
#include "pipeline_parallel.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

struct InternalLomResult
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
    bool cancelled = false;
    long long geometry_id = 0;  ///< Snapshot id of the captured layer geometry (0 = none)
};

struct InternalLomConfig
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
    HsBaLomProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
    bool capture_geometry = false;
    const void* cancel_flag = nullptr;  ///< std::atomic<bool>* set by the host or by its job handle

    // Cancellation is cooperative and only observed at stage boundaries: throwing from the
    // throttled progress callback of the sheet-parallel slicing stage would abandon sibling
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

void ReportProgress(const InternalLomConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

std::string BuildLomConfigJson(const InternalLomConfig& cfg, int total_layers)
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

InternalLomConfig BuildLomConfig(const HsBaLomPipelineConfig_t* cfg, HsBaLomProgressCallback cb, void* ud)
{
    InternalLomConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.cut_speed = cfg->cut_speed;
    ic.cut_margin = cfg->cut_margin;
    ic.cut_power = cfg->cut_power;
    ic.bond_temperature = cfg->bond_temperature;
    ic.bond_pressure = cfg->bond_pressure;
    ic.bond_time = cfg->bond_time;
    ic.seal_contour = cfg->seal_contour;
    ic.cut_mode = cfg->cut_mode;
    ic.export_lua_script = cfg->export_lua_script ? cfg->export_lua_script : "";
    ic.export_lua_func = cfg->export_lua_func ? cfg->export_lua_func : "";
    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    ic.capture_geometry = cfg->capture_geometry != 0;
    ic.cancel_flag = cfg->cancel_flag;
    return ic;
}

HsBaLomPipelineResult_t ToCResult(const InternalLomResult& ir)
{
    OwnedCString export_path(ir.export_path);
    OwnedCString error(ir.error_message);

    HsBaLomPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.export_path = export_path.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    cr.geometry_id = ir.geometry_id;
    return cr;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
Utils::Task<InternalLomResult> RunLomPipelineAsync(const InternalLomConfig& cfg)
{
    HsBa::Slicer::InstallCommonAnyObjectTypes();

    InternalLomResult result;
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

        // ========== Stage 2: Slicing (one contour set per sheet) ==========
        ReportProgress(cfg, 15, "Slicing...");
        std::vector<PolygonsD> layer_outlines(total_layers);
        std::vector<float> layer_z_heights(total_layers);
        float z_offset = info.bbox_min.z();

        // Build the slicing topology once and slice sheets in parallel: each layer
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
            result.error_message = "LOM export requires a Lua export script (export_lua_script)";
            co_return result;
        }

        // LOM has no infill or support slots, so only the sheet outlines are published.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(layer_z_heights[i]);
            }
            result.geometry_id = CaptureLayerGeometry(HSBA_PIPELINE_LOM, layer_outlines, {}, {}, geometry_zs);
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
            result.error_message = "Failed to export LOM package via Lua script: " + lua_error;
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
struct LomAsyncThunk
{
    HsBaLomResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunLomPipelineAsync.
 *
 * The job owns the temporary result struct and its strings, and releases both once this
 * callback returns, matching the copy-then-HsBaFreeLomPipelineResult contract.
 */
void DeliverLomResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<LomAsyncThunk> owned(static_cast<LomAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaLomPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // anonymous namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaLomPipelineConfig_t HsBaCreateDefaultLomConfig(void)
{
    return HsBaLomConfigDefault();
}

HSBA_SLICER_API HsBaLomPipelineResult_t HsBaRunLomPipeline(const HsBaLomPipelineConfig_t* config,
                                                           HsBaLomProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildLomConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunLomPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunLomPipelineAsync(const HsBaLomPipelineConfig_t* config, HsBaLomProgressCallback callback,
                                             void* user_data, HsBaLomResultCallback result_callback,
                                             void* result_user_data)
{
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::LomAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_LOM, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverLomResult, thunk);
}

HSBA_SLICER_API void HsBaFreeLomPipelineResult(HsBaLomPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
