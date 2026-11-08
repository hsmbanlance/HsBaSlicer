/** @file sls_pipeline.cpp
 * @brief Implementation of the SLS (selective laser sintering) slicing pipeline C ABI.
 * @author HsBa
 */
#include "sls_pipeline.h"

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

struct InternalSlsResult
{
    bool success = false;
    int total_layers = 0;
    std::string export_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
    bool cancelled = false;
    long long geometry_id = 0;  ///< Snapshot id of the captured layer geometry (0 = none)
};

struct InternalSlsConfig
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
    HsBaSlsProgressCallback progress_cb = nullptr;
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
    ic.capture_geometry = cfg->capture_geometry != 0;
    ic.cancel_flag = cfg->cancel_flag;
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
    cr.geometry_id = ir.geometry_id;
    return cr;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
Utils::Task<InternalSlsResult> RunSlsPipelineAsync(const InternalSlsConfig& cfg)
{
    // Load the AnyObject/Lua registration functions for common custom types into the generic registry pool for the
    // per-stage Lua environments to use
    HsBa::Slicer::InstallCommonAnyObjectTypes();

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
            result.error_message = "SLS export requires a Lua export script (export_lua_script)";
            co_return result;
        }

        // SLS produces no infill or support slots, so only the contours are published.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(layer_z_heights[i]);
            }
            result.geometry_id = CaptureLayerGeometry(HSBA_PIPELINE_SLS, layer_outlines, {}, {}, geometry_zs);
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
            result.error_message = "Failed to export SLS package via Lua script: " + lua_error;
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
struct SlsAsyncThunk
{
    HsBaSlsResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunSlsPipelineAsync.
 *
 * The job owns the temporary result struct and its strings, and releases both once this
 * callback returns, matching the copy-then-HsBaFreeSlsPipelineResult contract.
 */
void DeliverSlsResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<SlsAsyncThunk> owned(static_cast<SlsAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaSlsPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // anonymous namespace

}  // namespace HsBa::Slicer::Pipeline

// ========== C API ==========

HSBA_SLICER_API HsBaSlsPipelineConfig_t HsBaCreateDefaultSlsConfig(void)
{
    return HsBaSlsConfigDefault();
}

HSBA_SLICER_API HsBaSlsPipelineResult_t HsBaRunSlsPipeline(const HsBaSlsPipelineConfig_t* config,
                                                           HsBaSlsProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildSlsConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunSlsPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunSlsPipelineAsync(const HsBaSlsPipelineConfig_t* config, HsBaSlsProgressCallback callback,
                                             void* user_data, HsBaSlsResultCallback result_callback,
                                             void* result_user_data)
{
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::SlsAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_SLS, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverSlsResult, thunk);
}

HSBA_SLICER_API void HsBaFreeSlsPipelineResult(HsBaSlsPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->export_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
