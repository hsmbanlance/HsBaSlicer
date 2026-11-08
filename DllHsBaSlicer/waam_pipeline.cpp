/** @file waam_pipeline.cpp
 * @brief Implementation of the WAAM (wire arc additive manufacturing) slicing pipeline C ABI.
 * @author HsBa
 */
#include "waam_pipeline.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"
#include "LibHsBaSlicer/Path/waam_export.hpp"
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

struct InternalWaamResult
{
    bool success = false;
    int total_layers = 0;
    std::string output_path;
    std::string error_message;
    double elapsed_seconds = 0.0;
    bool cancelled = false;
    long long geometry_id = 0;  ///< Snapshot id of the captured layer geometry (0 = none)
};

struct InternalWaamConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.8F;
    float first_layer_height = 1.0F;
    float bead_width = 1.2F;
    float travel_speed = 8.0F;
    float wire_feed_speed = 5.0F;
    float arc_current = 180.0F;
    float arc_voltage = 22.0F;
    float gas_flow_rate = 15.0F;
    HsBaWaamMaterial_t material = HSBA_WAAM_MATERIAL_STEEL;
    HsBaWaamWeldProcess_t welding_process = HSBA_WAAM_WELD_ARC;
    HsBaWaamProtection_t protection = HSBA_WAAM_PROTECTION_SHIELD_GAS;
    HsBaMetalProtectGas_t protect_gas = HSBA_METAL_GAS_ARGON;
    float interpass_temperature = 100.0F;
    HsBaWaamRobotType_t robot_type = HSBA_WAAM_ROBOT_ABB;
    std::string path_lua_script;
    std::string path_lua_func;
    std::string output_path;
    bool spiral_mode = false;
    bool randomize_start_point = false;
    HsBaWaamProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
    bool capture_geometry = false;
    const void* cancel_flag = nullptr;  ///< std::atomic<bool>* set by the host or by its job handle

    // Cancellation is cooperative and only observed at stage boundaries: throwing from the
    // throttled progress callback of the bead-parallel slicing stage would abandon sibling
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

void ReportProgress(const InternalWaamConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

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
    ic.spiral_mode = cfg->spiral_mode != 0;
    ic.randomize_start_point = cfg->randomize_start_point != 0;
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    ic.capture_geometry = cfg->capture_geometry != 0;
    ic.cancel_flag = cfg->cancel_flag;
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
    cr.geometry_id = ir.geometry_id;
    return cr;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
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

        // ========== Stage 2: Slicing (wire-feed process keeps open contours) ==========
        ReportProgress(cfg, 15, "Slicing...");
        std::vector<PolygonsD> layer_outlines(total_layers);
        std::vector<float> layer_z_heights(total_layers);
        float z_offset = info.bbox_min.z();

        // Build the slicing topology once and slice bead layers in parallel: each
        // layer is independent and SliceLayer only const-reads the shared topology.
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
                if (cfg.randomize_start_point)
                {
                    // Stagger bead start seams: rotate each closed contour to a
                    // per-layer deterministic random start point on an edge interior.
                    Polygons const int_polys = Integerization(layer_outlines[i]);
                    layer_outlines[i] =
                        UnIntegerization(RandomizePathStartPoints(int_polys, static_cast<std::uint64_t>(i) + 1ULL));
                }
            },
            [&](int done)
            {
                int const progress = 15 + (done * 45) / total_layers;
                ReportProgress(cfg, progress, "Slicing bead layer");
            });
        cfg.CheckCancel();
        ReportProgress(cfg, 60, "Slicing complete");

        // ========== Stage 3: Robot path export ==========
        // WAAM output is a robot language program built from the deposition
        // contours; a Lua script is optional (built-in ABB/KUKA/FANUC if absent).
        cfg.CheckCancel();
        ReportProgress(cfg, 70, "Generating robot path...");

        // WAAM has no infill or support slots, so only the (already seam-rotated)
        // bead outlines are published.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(layer_z_heights[i]);
            }
            // Spiral runs deposit one continuous rising weld bead, so publish the same helix
            // the exporter derives from these contours (SpiralizeOuterWall is a pure function).
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
            result.geometry_id =
                CaptureLayerGeometry(HSBA_PIPELINE_WAAM, layer_outlines, {}, {}, geometry_zs, path_xyz);
        }

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
        pkg.spiral_mode = cfg.spiral_mode;

        std::string lua_error;
        bool const export_ok =
            SaveWaamRobotPath(pkg, output_path, cfg.path_lua_script,
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
struct WaamAsyncThunk
{
    HsBaWaamResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunWaamPipelineAsync.
 *
 * The job owns the temporary result struct and its strings, and releases both once this
 * callback returns, matching the copy-then-HsBaFreeWaamPipelineResult contract.
 */
void DeliverWaamResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<WaamAsyncThunk> owned(static_cast<WaamAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaWaamPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // anonymous namespace

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
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::WaamAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_WAAM, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverWaamResult, thunk);
}

HSBA_SLICER_API void HsBaFreeWaamPipelineResult(HsBaWaamPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->output_path, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
