/** @file fdm_pipeline.cpp
 * @brief FDM pipeline orchestration kernel implementation.
 *
 * Ported from the DllHsBaSlicer coroutine `RunPipelineAsync` (fdm_pipeline.cpp), made
 * synchronous so the same code drives the Dll blocking C entry, the job thread and the
 * C++20 module. Stage order, progress percentages, spiral/vase handling, top/bottom
 * solid layering, density-driven spacing, seam staggering, model-preview thumbnails,
 * cooperative cancellation and geometry capture are preserved byte-for-byte.
 * @author HsBa
 */
#include "LibHsBaSlicer/Pipeline/fdm_pipeline.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper2/clipper.h>
#include <Eigen/Core>

#include "2D/FloatPolygons.hpp"
#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "base/encoding_convert.hpp"
#include "base/error.hpp"
#include "base/parallel.hpp"
#include "cipher/encoder.hpp"
#include "paths/gcodepath.hpp"

namespace HsBa::Slicer::Kernel
{

using HsBa::Slicer::Pipeline::ParallelForLayers;
namespace
{

// Kernel-internal view of the C config plus the native hooks. Mirrors the former Dll
// InternalConfig so the ported stage body reads unchanged.
struct Cfg
{
    std::string model_name;
    std::string model_path;
    float layer_height = 0.2F;
    float first_layer_height = 0.25F;
    double fill_spacing = 0.4;
    FillMode fill_mode = FillMode::Zigzag;
    double fill_angle = 45.0;
    int wall_count = 3;
    int top_layer_count = 3;
    int bottom_layer_count = 3;
    double infill_density = 0.2;
    bool enable_support = true;
    bool spiral_mode = false;
    bool randomize_start_point = false;
    std::string support_lua_script;
    std::string support_lua_func;
    std::string infill_lua_script;
    std::string infill_lua_func;
    Support::FdmSupportConfig support_config;
    FdmPathConfig path_config;
    GCodePrinterConfig printer_config;
    GCodeFirmware firmware = GCodeFirmware::Marlin;
    std::string output_path;
    std::function<void(int, std::string_view)> progress;
    bool capture_geometry = false;
    const std::atomic<bool>* cancel_flag = nullptr;
    bool gcode_thumbnails = false;
    int gcode_thumbnail_size = 0;

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

bool SaveGCode(const std::string& gcode, const std::string& output_path)
{
    const std::filesystem::path path = utf8_to_path(output_path);
    std::error_code ec;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            return false;
        }
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    out.write(gcode.data(), static_cast<std::streamsize>(gcode.size()));
    out.flush();
    return out.good();
}

// ============================================================
// G-code preview images
// ============================================================

constexpr Eigen::Index kPreviewTriangleBudget = 200000;
constexpr int kPreviewDefaultEdge = 256;
constexpr int kPreviewMinEdge = 32;
constexpr int kPreviewMaxEdge = 512;
constexpr std::size_t kPreviewBase64Budget = 48U * 1024U;

int PreviewEdge(int requested)
{
    if (requested <= 0)
    {
        return kPreviewDefaultEdge;
    }
    return std::clamp(requested, kPreviewMinEdge, kPreviewMaxEdge);
}

PolygonsD ProjectMeshSilhouette(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, int plane)
{
    const int axis_u = (plane == 2) ? 1 : 0;
    const int axis_v = (plane == 0) ? 1 : 2;

    PolygonsD triangles;
    triangles.reserve(static_cast<std::size_t>(faces.rows()));
    for (Eigen::Index f = 0; f < faces.rows(); ++f)
    {
        double const u0 = vertices(faces(f, 0), axis_u);
        double const v0 = vertices(faces(f, 0), axis_v);
        double const u1 = vertices(faces(f, 1), axis_u);
        double const v1 = vertices(faces(f, 1), axis_v);
        double const u2 = vertices(faces(f, 2), axis_u);
        double const v2 = vertices(faces(f, 2), axis_v);
        double const twice_area = (u1 - u0) * (v2 - v0) - (u2 - u0) * (v1 - v0);
        if (std::fabs(twice_area) < 1e-12)
        {
            continue;
        }
        PolygonD tri;
        tri.reserve(3);
        tri.push_back(Point2D{u0, v0});
        tri.push_back(Point2D{u1, v1});
        tri.push_back(Point2D{u2, v2});
        if (twice_area < 0.0)
        {
            std::reverse(tri.begin(), tri.end());
        }
        triangles.push_back(std::move(tri));
    }
    return HsBa::Slicer::Union(triangles, PolygonsD{}, Clipper2Lib::FillRule::NonZero);
}

GCodeThumbnail MakePreviewThumbnail(const std::string& name, const PolygonsD& silhouette, int edge,
                                    const std::string& failure_reason)
{
    GCodeThumbnail thumb;
    thumb.name = name;
    thumb.width = edge;
    thumb.height = edge;
    thumb.format = "png";
    std::string bytes;
    if (!failure_reason.empty())
    {
        thumb.reason = failure_reason;
    }
    else if (silhouette.empty())
    {
        thumb.reason = "empty_silhouette";
    }
    else if (!RenderPolygonsToImageBuffer(silhouette, edge, edge, ".png", bytes))
    {
        thumb.reason = "render_failed";
    }
    else
    {
        thumb.base64 = Cipher::Encoder::base64_encode(bytes);
        if (thumb.base64.size() > kPreviewBase64Budget)
        {
            thumb.base64.clear();
            thumb.reason = "image_too_large";
        }
    }
    return thumb;
}

std::vector<GCodeThumbnail> BuildModelThumbnails(const IModel& model, int requested_edge)
{
    const int edge = PreviewEdge(requested_edge);
    std::vector<GCodeThumbnail> thumbs;
    thumbs.reserve(2);

    std::pair<Eigen::MatrixXf, Eigen::MatrixXi> mesh;
    try
    {
        mesh = model.TriangleMesh();
    }
    catch (const std::exception&)
    {
        for (const char* name : {"model_top", "model_side"})
        {
            thumbs.push_back(MakePreviewThumbnail(name, {}, edge, "render_failed"));
        }
        return thumbs;
    }

    const Eigen::MatrixXf& vertices = mesh.first;
    const Eigen::MatrixXi& faces = mesh.second;
    if (faces.rows() > kPreviewTriangleBudget)
    {
        for (const char* name : {"model_top", "model_side"})
        {
            thumbs.push_back(MakePreviewThumbnail(name, {}, edge, "mesh_too_large"));
        }
        return thumbs;
    }

    static constexpr std::pair<int, const char*> kViews[] = {{0, "model_top"}, {1, "model_side"}};
    for (const auto& [plane, name] : kViews)
    {
        PolygonsD silhouette;
        std::string projection_failure;
        try
        {
            silhouette = ProjectMeshSilhouette(vertices, faces, plane);
        }
        catch (const std::exception&)
        {
            projection_failure = "render_failed";
        }
        thumbs.push_back(MakePreviewThumbnail(name, silhouette, edge, projection_failure));
    }
    return thumbs;
}

Cfg BuildCfg(const HsBaFdmPipelineConfig_t& cfg, const Hooks& hooks)
{
    Cfg ic;
    ic.model_name = cfg.model_name ? cfg.model_name : "";
    ic.model_path = cfg.model_path ? cfg.model_path : "";
    ic.layer_height = cfg.layer_height;
    ic.first_layer_height = cfg.first_layer_height;
    ic.fill_spacing = cfg.fill_spacing;
    ic.fill_mode = static_cast<FillMode>(static_cast<int>(cfg.fill_mode));
    ic.fill_angle = cfg.fill_angle;
    ic.wall_count = cfg.wall_count;
    ic.top_layer_count = cfg.top_layer_count;
    ic.bottom_layer_count = cfg.bottom_layer_count;
    ic.infill_density = cfg.infill_density;
    ic.enable_support = cfg.enable_support != 0;
    ic.spiral_mode = cfg.spiral_mode != 0;
    ic.randomize_start_point = cfg.randomize_start_point != 0;
    ic.support_lua_script = cfg.support_lua_script ? cfg.support_lua_script : "";
    ic.support_lua_func = cfg.support_lua_func ? cfg.support_lua_func : "";
    ic.infill_lua_script = cfg.infill_lua_script ? cfg.infill_lua_script : "";
    ic.infill_lua_func = cfg.infill_lua_func ? cfg.infill_lua_func : "";

    ic.support_config.overhang_angle_threshold = cfg.overhang_angle;
    ic.support_config.layer_height = cfg.layer_height;
    ic.support_config.support_gap = cfg.support_gap;
    ic.support_config.support_diameter = cfg.support_diameter;
    ic.support_config.support_density = cfg.support_density;
    ic.support_config.support_pattern = static_cast<int>(cfg.support_pattern);
    ic.support_config.interface_layers = cfg.interface_layers;
    ic.support_config.honeycomb_cell_size = 5.0F;

    ic.path_config.layer_height = cfg.layer_height;
    ic.path_config.line_width = cfg.line_width;
    ic.path_config.print_speed = cfg.print_speed;
    ic.path_config.travel_speed = cfg.travel_speed;
    ic.path_config.extrusion_multiplier = cfg.extrusion_multiplier;

    ic.printer_config.nozzle_diameter = cfg.nozzle_diameter;
    ic.printer_config.filament_diameter = cfg.filament_diameter;
    ic.printer_config.nozzle_temp = cfg.nozzle_temp;
    ic.printer_config.bed_temp = cfg.bed_temp;
    ic.printer_config.retract_length = cfg.retract_length;
    ic.printer_config.retract_speed = cfg.retract_speed;
    ic.printer_config.print_speed = cfg.print_speed;
    ic.printer_config.travel_speed = cfg.travel_speed;
    ic.printer_config.first_layer_speed = cfg.first_layer_speed;
    ic.printer_config.layer_height = cfg.layer_height;
    ic.printer_config.line_width = cfg.line_width;
    ic.printer_config.extrusion_multiplier = cfg.extrusion_multiplier;

    ic.firmware = static_cast<GCodeFirmware>(static_cast<int>(cfg.gcode_firmware));

    ic.output_path = cfg.output_path ? cfg.output_path : "";
    ic.progress = hooks.progress;
    ic.capture_geometry = cfg.capture_geometry != 0;
    ic.cancel_flag = static_cast<const std::atomic<bool>*>(cfg.cancel_flag);
    ic.gcode_thumbnails = cfg.gcode_thumbnails != 0;
    ic.gcode_thumbnail_size = cfg.gcode_thumbnail_size;
    return ic;
}

FdmOutput RunPipeline(const Cfg& cfg)
{
    FdmOutput result;
    auto start_time = std::chrono::steady_clock::now();

    // Install the common custom types' AnyObject/Lua registration functions into the shared
    // registry pool for use by each stage's Lua environment.
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
        float z_offset = info.bbox_min.z();

        auto topo = BuildSliceTopology(*model);
        ParallelForLayers(
            total_layers,
            [&](int i)
            {
                if (cfg.CancelRequested())
                {
                    return;
                }
                float const z = GetLayerZ(i, cfg.first_layer_height, cfg.layer_height) + z_offset;
                layer_outlines[i] = SliceLayer(*topo, z);
            },
            [&](int done)
            {
                int const progress = 15 + (done * 25) / total_layers;
                ReportProgress(cfg, progress, "Slicing layer");
            });
        cfg.CheckCancel();
        ReportProgress(cfg, 40, "Slicing complete");

        // ========== Stage 3: Support ==========
        cfg.CheckCancel();
        std::vector<PolygonsD> layer_supports(total_layers);
        if (cfg.spiral_mode)
        {
            ReportProgress(cfg, 60, "Support disabled (spiral mode)");
        }
        else if (cfg.enable_support)
        {
            ReportProgress(cfg, 45, "Generating supports...");
            if (!cfg.support_lua_script.empty())
            {
                std::string const func = cfg.support_lua_func.empty() ? "generate_support" : cfg.support_lua_func;
                std::ifstream ifs(cfg.support_lua_script);
                std::string const script_content((std::istreambuf_iterator<char>(ifs)),
                                                 std::istreambuf_iterator<char>());
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
        cfg.CheckCancel();
        ReportProgress(cfg, 65, "Generating fills...");
        std::vector<PolygonsD> layer_fills(total_layers);

        const int bottom_end = cfg.bottom_layer_count;
        const int top_start = total_layers - cfg.top_layer_count;
        const bool has_lua_infill = !cfg.infill_lua_script.empty();
        const std::string infill_func = cfg.infill_lua_func.empty() ? "generate_fill" : cfg.infill_lua_func;

        const int spiral_solid = cfg.spiral_mode ? std::clamp(bottom_end, 0, total_layers) : 0;

        double middle_spacing = cfg.fill_spacing;
        if (cfg.infill_density > 0.0 && cfg.infill_density < 1.0)
        {
            middle_spacing = cfg.fill_spacing / cfg.infill_density;
        }

        auto cpp_fill_layer = [&](int i)
        {
            if (cfg.CancelRequested())
            {
                return;
            }
            if (layer_outlines[i].empty())
            {
                return;
            }
            Polygons const int_polys = Integerization(layer_outlines[i]);
            bool const is_solid = (i < bottom_end) || (i >= top_start);
            double const spacing = is_solid ? cfg.fill_spacing : middle_spacing;
            Polygons const fill_result =
                FillWithBorder(int_polys, spacing, cfg.wall_count, cfg.fill_mode, cfg.fill_angle);
            if (cfg.randomize_start_point)
            {
                layer_fills[i] =
                    UnIntegerization(RandomizePathStartPoints(fill_result, static_cast<std::uint64_t>(i) + 1ULL));
                return;
            }
            layer_fills[i] = UnIntegerization(fill_result);
        };

        if (cfg.spiral_mode)
        {
            for (int i = 0; i < spiral_solid; ++i)
            {
                cpp_fill_layer(i);
            }
            ReportProgress(cfg, 85, "Fill skipped above the solid base (spiral mode)");
        }
        else if (has_lua_infill)
        {
            for (int i = 0; i < total_layers; ++i)
            {
                cfg.CheckCancel();
                if (!layer_outlines[i].empty())
                {
                    Polygons const int_polys = Integerization(layer_outlines[i]);
                    bool const is_solid = (i < bottom_end) || (i >= top_start);
                    if (!is_solid)
                    {
                        Polygons const fill_result = LuaCustomFillByFile(int_polys, cfg.infill_lua_script, infill_func);
                        layer_fills[i] = UnIntegerization(fill_result);
                    }
                    else
                    {
                        cpp_fill_layer(i);
                    }
                }
                int const progress = 65 + (i * 20) / total_layers;
                ReportProgress(cfg, progress, "Filling layer");
            }
        }
        else
        {
            ParallelForLayers(total_layers, cpp_fill_layer,
                              [&](int done)
                              {
                                  int const progress = 65 + (done * 20) / total_layers;
                                  ReportProgress(cfg, progress, "Filling layer");
                              });
        }
        cfg.CheckCancel();
        ReportProgress(cfg, 85, "Fill generation complete");

        // ========== Stage 5: Path generation ==========
        ReportProgress(cfg, 90, "Generating G-code paths...");

        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(GetLayerZ(i, cfg.first_layer_height, cfg.layer_height));
            }
            std::vector<double> path_xyz;
            if (cfg.spiral_mode && spiral_solid < total_layers)
            {
                const std::vector<PolygonsD> helix_layers(layer_outlines.begin() + spiral_solid,
                                                          layer_outlines.end());
                const std::vector<double> helix_zs(geometry_zs.begin() + spiral_solid, geometry_zs.end());
                const std::vector<SpiralPoint> helix = SpiralizeOuterWall(helix_layers, helix_zs);
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
            result.geometry.fills = layer_fills;
            result.geometry.supports = layer_supports;
            result.geometry.zs = std::move(geometry_zs);
            result.geometry.path_xyz = std::move(path_xyz);
        }

        std::vector<GCodeThumbnail> thumbnails;
        if (cfg.gcode_thumbnails)
        {
            ReportProgress(cfg, 92, "Rendering preview images...");
            thumbnails = BuildModelThumbnails(*model, cfg.gcode_thumbnail_size);
        }

        if (cfg.spiral_mode)
        {
            std::vector<double> layer_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                layer_zs[i] = static_cast<double>(GetLayerZ(i, cfg.first_layer_height, cfg.layer_height));
            }

            auto gcode_path = GenerateGCodePathSpiral(layer_outlines, layer_zs, cfg.printer_config,
                                                      static_cast<std::size_t>(spiral_solid), layer_fills);
            if (gcode_path)
            {
                if (!thumbnails.empty())
                {
                    gcode_path->SetThumbnails(thumbnails);
                }
                result.gcode = gcode_path->ToGCode(cfg.firmware);
                result.success = true;
            }
            else
            {
                result.success = false;
                result.error = "Failed to generate spiral G-code path";
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
                if (!thumbnails.empty())
                {
                    gcode_path->SetThumbnails(thumbnails);
                }
                result.gcode = gcode_path->ToGCode(cfg.firmware);
                result.success = true;
            }
            else
            {
                result.success = false;
                result.error = "Failed to generate G-code path";
            }
        }

        if (result.success && !cfg.output_path.empty())
        {
            ReportProgress(cfg, 97, "Writing G-code file...");
            if (!SaveGCode(result.gcode, cfg.output_path))
            {
                result.success = false;
                result.error = "Failed to write G-code to the job output file";
            }
        }

        ReportProgress(cfg, 100, "Pipeline complete");
    }
    catch (const CancelledError& e)
    {
        result.success = false;
        result.cancelled = true;
        result.geometry.valid = false;
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

FdmOutput RunFdm(const HsBaFdmPipelineConfig_t& cfg, const Hooks& hooks)
{
    return RunPipeline(BuildCfg(cfg, hooks));
}

}  // namespace HsBa::Slicer::Kernel
