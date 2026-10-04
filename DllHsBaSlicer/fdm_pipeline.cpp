/** @file fdm_pipeline.cpp
 * @brief Implementation of the FDM slicing pipeline C ABI.
 * @author HsBa
 */
#include "fdm_pipeline.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "base/coroutine.hpp"
#include "base/error.hpp"
#include "paths/gcodepath.hpp"
#include "pipeline_parallel.hpp"

namespace HsBa::Slicer::Pipeline
{

// Internal result (namespace-visible, referenced by lambdas)
struct InternalResult
{
    bool success = false;
    int total_layers = 0;
    std::string gcode_content;
    std::string error_message;
    double elapsed_seconds = 0.0;
};

// Internal configuration
struct InternalConfig
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.2f;
    float first_layer_height = 0.25f;
    double fill_spacing = 0.4;
    FillMode fill_mode = FillMode::Zigzag;
    double fill_angle = 45.0;
    int wall_count = 3;
    int top_layer_count = 3;
    int bottom_layer_count = 3;
    double infill_density = 0.2;
    bool enable_support = true;
    bool spiral_mode = false;
    std::string support_lua_script;
    std::string support_lua_func;
    std::string infill_lua_script;
    std::string infill_lua_func;
    Support::FdmSupportConfig support_config;
    FdmPathConfig path_config;
    GCodePrinterConfig printer_config;
    GCodeFirmware firmware = GCodeFirmware::Marlin;
    std::string output_path;
    HsBaProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
};

namespace
{

// RAII guard: ensures malloc-allocated C strings are freed on exception/early return
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

    // Release ownership and return the raw pointer (caller is responsible for freeing)
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

void ReportProgress(const InternalConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

}  // anonymous namespace

InternalConfig BuildConfig(const HsBaFdmPipelineConfig_t* cfg, HsBaProgressCallback cb, void* ud)
{
    InternalConfig ic;
    ic.model_name = cfg->model_name ? cfg->model_name : "";
    ic.model_path = cfg->model_path ? cfg->model_path : "";
    ic.layer_height = cfg->layer_height;
    ic.first_layer_height = cfg->first_layer_height;
    ic.fill_spacing = cfg->fill_spacing;
    ic.fill_mode = static_cast<FillMode>(static_cast<int>(cfg->fill_mode));
    ic.fill_angle = cfg->fill_angle;
    ic.wall_count = cfg->wall_count;
    ic.top_layer_count = cfg->top_layer_count;
    ic.bottom_layer_count = cfg->bottom_layer_count;
    ic.infill_density = cfg->infill_density;
    ic.enable_support = cfg->enable_support != 0;
    ic.spiral_mode = cfg->spiral_mode != 0;
    ic.support_lua_script = cfg->support_lua_script ? cfg->support_lua_script : "";
    ic.support_lua_func = cfg->support_lua_func ? cfg->support_lua_func : "";
    ic.infill_lua_script = cfg->infill_lua_script ? cfg->infill_lua_script : "";
    ic.infill_lua_func = cfg->infill_lua_func ? cfg->infill_lua_func : "";

    ic.support_config.overhang_angle_threshold = cfg->overhang_angle;
    ic.support_config.layer_height = cfg->layer_height;
    ic.support_config.support_gap = cfg->support_gap;
    ic.support_config.support_diameter = cfg->support_diameter;
    ic.support_config.support_density = cfg->support_density;
    ic.support_config.support_pattern = static_cast<int>(cfg->support_pattern);
    ic.support_config.interface_layers = cfg->interface_layers;
    ic.support_config.honeycomb_cell_size = 5.0f;

    ic.path_config.layer_height = cfg->layer_height;
    ic.path_config.line_width = cfg->line_width;
    ic.path_config.print_speed = cfg->print_speed;
    ic.path_config.travel_speed = cfg->travel_speed;
    ic.path_config.extrusion_multiplier = cfg->extrusion_multiplier;

    ic.printer_config.nozzle_diameter = cfg->nozzle_diameter;
    ic.printer_config.filament_diameter = cfg->filament_diameter;
    ic.printer_config.nozzle_temp = cfg->nozzle_temp;
    ic.printer_config.bed_temp = cfg->bed_temp;
    ic.printer_config.retract_length = cfg->retract_length;
    ic.printer_config.retract_speed = cfg->retract_speed;
    ic.printer_config.print_speed = cfg->print_speed;
    ic.printer_config.travel_speed = cfg->travel_speed;
    ic.printer_config.first_layer_speed = cfg->first_layer_speed;
    ic.printer_config.layer_height = cfg->layer_height;
    ic.printer_config.line_width = cfg->line_width;
    ic.printer_config.extrusion_multiplier = cfg->extrusion_multiplier;

    ic.firmware = static_cast<GCodeFirmware>(static_cast<int>(cfg->gcode_firmware));

    ic.output_path = cfg->output_path ? cfg->output_path : "";
    ic.progress_cb = cb;
    ic.progress_user_data = ud;
    return ic;
}

HsBaFdmPipelineResult_t ToCResult(const InternalResult& ir)
{
    OwnedCString content(ir.gcode_content);
    OwnedCString error(ir.error_message);

    HsBaFdmPipelineResult_t cr{};
    cr.success = ir.success ? 1 : 0;
    cr.total_layers = ir.total_layers;
    cr.gcode_content = content.release();
    cr.error_message = error.release();
    cr.elapsed_seconds = ir.elapsed_seconds;
    return cr;
}

// ModelLoader is already included at the top of this file.
// InternalConfig does not reference ModelLoader, so it is unaffected by its non-copyability.

/**
 * @brief Core coroutine implementation of the FDM pipeline.
 */
Utils::Task<InternalResult> RunPipelineAsync(const InternalConfig& cfg)
{
    // Install the common custom types' AnyObject/Lua registration functions into the shared registry pool for use by each stage's Lua environment
    HsBa::Slicer::InstallCommonAnyObjectTypes();

    InternalResult result;
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
        ParallelForLayers(
            total_layers,
            [&](int i)
            {
                float z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
                layer_outlines[i] = SliceLayer(*topo, z);
            },
            [&](int done)
            {
                int progress = 15 + (done * 25) / total_layers;
                ReportProgress(cfg, progress, "Slicing layer");
            });
        ReportProgress(cfg, 40, "Slicing complete");

        // ========== Stage 3: Support ==========
        std::vector<PolygonsD> layer_supports(total_layers);
        if (cfg.spiral_mode)
        {
            // Vase/spiral mode emits a single continuous wall: no support, no infill.
            ReportProgress(cfg, 60, "Support disabled (spiral mode)");
        }
        else if (cfg.enable_support)
        {
            ReportProgress(cfg, 45, "Generating supports...");
            if (!cfg.support_lua_script.empty())
            {
                // Lua custom support via LibHsBaSlicer API
                std::string func = cfg.support_lua_func.empty() ? "generate_support" : cfg.support_lua_func;
                std::ifstream ifs(cfg.support_lua_script);
                std::string script_content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                layer_supports = GenerateAllLuaSupport(layer_outlines, cfg.support_config,
                                                       std::string_view(script_content), std::string_view(func));
            }
            else
            {
                layer_supports = GenerateAllFdmSupport(layer_outlines, cfg.support_config);
            }
            ReportProgress(cfg, 60, "Support generation complete");
        }
        else
        {
            ReportProgress(cfg, 60, "Support disabled");
        }

        // ========== Stage 4: Infill ==========
        ReportProgress(cfg, 65, "Generating fills...");
        std::vector<PolygonsD> layer_fills(total_layers);

        // Determine top/bottom/middle layer ranges
        const int bottom_end = cfg.bottom_layer_count;             // [0, bottom_end) is bottom
        const int top_start = total_layers - cfg.top_layer_count;  // [top_start, total_layers) is top
        const bool has_lua_infill = !cfg.infill_lua_script.empty();
        const std::string infill_func = cfg.infill_lua_func.empty() ? "generate_fill" : cfg.infill_lua_func;

        // Middle-layer fill spacing: lower density means larger spacing
        double middle_spacing = cfg.fill_spacing;
        if (cfg.infill_density > 0.0 && cfg.infill_density < 1.0)
        {
            middle_spacing = cfg.fill_spacing / cfg.infill_density;
        }

        // FillWithBorder (Clipper2) is a pure per-layer computation and parallelizes
        // safely; Lua infill uses a shared interpreter and must stay serial.
        auto cpp_fill_layer = [&](int i)
        {
            if (layer_outlines[i].empty())
                return;
            Polygons int_polys = Integerization(layer_outlines[i]);
            bool is_solid = (i < bottom_end) || (i >= top_start);  // solid fill for top/bottom layers
            double spacing = is_solid ? cfg.fill_spacing : middle_spacing;
            Polygons fill_result = FillWithBorder(int_polys, spacing, cfg.wall_count, cfg.fill_mode, cfg.fill_angle);
            layer_fills[i] = UnIntegerization(fill_result);
        };

        if (cfg.spiral_mode)
        {
            // Spiral/vase mode has no infill; skip the fill stage entirely.
            ReportProgress(cfg, 85, "Fill skipped (spiral mode)");
        }
        else if (has_lua_infill)
        {
            // Serial per layer: Lua interpreter state is not shared across threads
            for (int i = 0; i < total_layers; ++i)
            {
                if (!layer_outlines[i].empty())
                {
                    Polygons int_polys = Integerization(layer_outlines[i]);
                    bool is_solid = (i < bottom_end) || (i >= top_start);
                    if (!is_solid)
                    {
                        // Lua custom fill via LibHsBaSlicer API
                        Polygons fill_result = LuaCustomFillByFile(int_polys, cfg.infill_lua_script, infill_func);
                        layer_fills[i] = UnIntegerization(fill_result);
                    }
                    else
                    {
                        cpp_fill_layer(i);
                    }
                }
                int progress = 65 + (i * 20) / total_layers;
                ReportProgress(cfg, progress, "Filling layer");
            }
        }
        else
        {
            ParallelForLayers(total_layers, cpp_fill_layer,
                              [&](int done)
                              {
                                  int progress = 65 + (done * 20) / total_layers;
                                  ReportProgress(cfg, progress, "Filling layer");
                              });
        }
        ReportProgress(cfg, 85, "Fill generation complete");

        // ========== Stage 5: Path generation ==========
        ReportProgress(cfg, 90, "Generating G-code paths...");

        if (cfg.spiral_mode)
        {
            // Vase/spiral mode: merge per-layer outer contours into one continuous,
            // Z-rising helix (no per-layer travel, no infill/support).
            std::vector<double> layer_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
                layer_zs[i] = static_cast<double>(GetLayerZ(i, cfg.first_layer_height, cfg.layer_height));

            auto gcode_path = GenerateGCodePathSpiral(layer_outlines, layer_zs, cfg.printer_config);
            if (gcode_path)
            {
                result.gcode_content = gcode_path->ToGCode(cfg.firmware);
                result.success = true;
            }
            else
            {
                result.success = false;
                result.error_message = "Failed to generate spiral G-code path";
            }
        }
        else
        {
            std::vector<LayerPathData> layer_path_data(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                layer_path_data[i].outlines = layer_outlines[i];
                layer_path_data[i].fills = layer_fills[i];
                layer_path_data[i].supports = layer_supports[i];
                layer_path_data[i].z_height = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height);
            }

            auto gcode_path = GenerateGCodePathV2(layer_path_data, cfg.path_config, cfg.printer_config);

            if (gcode_path)
            {
                result.gcode_content = gcode_path->ToGCode(cfg.firmware);
                result.success = true;
            }
            else
            {
                result.success = false;
                result.error_message = "Failed to generate G-code path";
            }
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

}  // namespace HsBa::Slicer::Pipeline

// ========== C export interface ==========

HSBA_SLICER_API HsBaFdmPipelineConfig_t HsBaCreateDefaultConfig(void)
{
    return HsBaFdmConfigDefault();
}

HSBA_SLICER_API HsBaFdmPipelineResult_t HsBaRunFdmPipeline(const HsBaFdmPipelineConfig_t* config,
                                                           HsBaProgressCallback callback, void* user_data)
{
    auto ic = HsBa::Slicer::Pipeline::BuildConfig(config, callback, user_data);
    auto task = HsBa::Slicer::Pipeline::RunPipelineAsync(ic);
    auto ir = task.get_result();
    return HsBa::Slicer::Pipeline::ToCResult(ir);
}

HSBA_SLICER_API void HsBaRunFdmPipelineAsync(const HsBaFdmPipelineConfig_t* config, HsBaProgressCallback callback,
                                             void* user_data, HsBaResultCallback result_callback,
                                             void* result_user_data)
{
    // Heap-allocate config to keep its lifetime safe during coroutine execution
    auto shared_cfg = std::make_shared<HsBa::Slicer::Pipeline::InternalConfig>(
        HsBa::Slicer::Pipeline::BuildConfig(config, callback, user_data));
    auto task = HsBa::Slicer::Pipeline::RunPipelineAsync(*shared_cfg);
    task.then(
        [shared_cfg, result_callback, result_user_data](HsBa::Slicer::Pipeline::InternalResult ir)
        {
            auto cr = HsBa::Slicer::Pipeline::ToCResult(ir);
            if (result_callback)
            {
                result_callback(cr, result_user_data);
            }
        });
}

HSBA_SLICER_API void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result)
{
    if (!result)
        return;
    std::free(std::exchange(result->gcode_content, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
