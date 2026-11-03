/** @file fdm_pipeline.cpp
 * @brief Implementation of the FDM slicing pipeline C ABI.
 * @author HsBa
 */
#include "fdm_pipeline.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LibHsBaSlicer/Extends/LuaCommonTypes.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/spiral_path.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "base/coroutine.hpp"
#include "base/encoding_convert.hpp"
#include "base/error.hpp"
#include "cipher/encoder.hpp"
#include "geometry_store.hpp"
#include "job_control.h"
#include "paths/gcodepath.hpp"
#include "pipeline_parallel.hpp"

namespace HsBa::Slicer::Pipeline
{
namespace
{

// Internal result (namespace-visible, referenced by lambdas)
struct InternalResult
{
    bool success = false;
    int total_layers = 0;
    std::string gcode_content;
    std::string error_message;
    double elapsed_seconds = 0.0;
    bool cancelled = false;
    long long geometry_id = 0;  ///< Snapshot id of the captured layer geometry (0 = none)
};

// Internal configuration
struct InternalConfig
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
    HsBaProgressCallback progress_cb = nullptr;
    void* progress_user_data = nullptr;
    bool capture_geometry = false;
    const void* cancel_flag = nullptr;  ///< std::atomic<bool>* set by the host or by its job handle
    bool gcode_thumbnails = false;      ///< Project the model into the G-code header as preview images
    int gcode_thumbnail_size = 0;       ///< Long edge of each preview in pixels (0 = default, clamped)

    // Cancellation is cooperative: the flag is only polled at stage boundaries and inside
    // serial loops, because an exception escaping the throttled progress callback of a
    // layer-parallel stage would abandon sibling worker tasks still writing into the
    // caller's layer vectors. Parallel stages therefore observe the request as soon as
    // their remaining iterations are skipped (see CancelRequested in the layer functors).
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

    // Release ownership and return the raw pointer (caller is responsible for freeing)
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

void ReportProgress(const InternalConfig& cfg, int percent, const std::string& stage)
{
    if (cfg.progress_cb)
    {
        cfg.progress_cb(percent, stage.c_str(), cfg.progress_user_data);
    }
}

// Persist the generated G-code when the caller asked for a file. Every other pipeline
// writes its configured output_path; FDM used to keep the content in memory only, so a
// caller that passes nothing but a path (the REST server does) got no artifact back.
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
    // The path is already native (wide on Windows), so ofstream handles any Unicode name.
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

// Each view is made by projecting every triangle of the mesh and letting Clipper merge the
// overlapping soup into one silhouette. A very large mesh would turn that into a pause longer
// than the slice itself, so past this budget the previews are skipped with a reason instead.
constexpr Eigen::Index kPreviewTriangleBudget = 200000;
constexpr int kPreviewDefaultEdge = 256;
constexpr int kPreviewMinEdge = 32;
constexpr int kPreviewMaxEdge = 512;
// The header comment blocks are read back through a preview that only shows the head of the file, so
// one oversized image would push the blocks that follow it out of what any client can see.
constexpr std::size_t kPreviewBase64Budget = 48U * 1024U;

/**
 * @brief Long edge in pixels of a preview image: 0 takes the default, anything else is clamped.
 */
int PreviewEdge(int requested)
{
    if (requested <= 0)
    {
        return kPreviewDefaultEdge;
    }
    return std::clamp(requested, kPreviewMinEdge, kPreviewMaxEdge);
}

/**
 * @brief Orthographic projection of a triangle mesh onto one coordinate plane.
 *
 * @param vertices Mesh vertices, one XYZ row per vertex.
 * @param faces Mesh triangles, one index row per face.
 * @param plane Look direction: 0 looks down the Z axis onto XY (a top view), 1 along Y onto XZ,
 *               2 along X onto YZ.
 * @return The filled silhouette as closed polygons, empty when the mesh has no area there.
 */
PolygonsD ProjectMeshSilhouette(const Eigen::MatrixXf& vertices, const Eigen::MatrixXi& faces, int plane)
{
    // Which two axes survive the look: from above the model reads (x, y) and from the side (x, z).
    // Nothing is rotated, so a view always agrees with the build plate.
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
        // Twice the signed projected area. A face seen edge-on collapses to a line and carries no
        // silhouette at all, so keeping it out of the union only saves work.
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
        // A closed mesh has a front and a back face over every point of the silhouette, and the two
        // wind opposite ways. A non-zero union would add their winding numbers and cut a hole exactly
        // where the model is thickest, so each triangle is turned the same way before merging.
        if (twice_area < 0.0)
        {
            std::reverse(tri.begin(), tri.end());
        }
        triangles.push_back(std::move(tri));
    }
    // Overlapping faces would otherwise leave holes where an odd number of them cancel out, so the
    // triangle soup is merged into its outer boundary before anything is drawn. The wrapper is named
    // explicitly because argument-dependent lookup would also see the identical Clipper2 free function.
    return HsBa::Slicer::Union(triangles, PolygonsD{}, Clipper2Lib::FillRule::NonZero);
}

/**
 * @brief Renders one silhouette into a Base64 PNG entry, or into a skip notice that says why not.
 *
 * A skip notice keeps its name and edge so a reader can still tell which view was asked for; only
 * the picture is replaced by the reason, and the reason distinguishes an empty projection from a
 * renderer that failed and from an image too big to hand out.
 */
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
        thumb.reason = failure_reason;  // a view that never reached the renderer
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

/**
 * @brief The model previews a G-code header can carry, top view first.
 *
 * Previews are derived from the very mesh the slicer cut, so a picture can never disagree with the
 * part being printed. The function never throws: a model that cannot give up a triangle mesh, or a
 * projection that runs out of budget, comes back as skip notices, because a missing preview must not
 * turn a good slice into a failed job.
 */
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
        // Model kinds without a triangulation (a custom or parametric model) are still sliceable,
        // so both views say plainly that nothing could be rendered out of them.
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

    // Plane 0 is the look straight down onto the bed, which is the one view a single-slot preview
    // panel would pick, so it comes first; the side view is the only other one that reads a shape.
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
            // A boolean over a pathological mesh must stay a preview problem, never a slice failure.
            projection_failure = "render_failed";
        }
        thumbs.push_back(MakePreviewThumbnail(name, silhouette, edge, projection_failure));
    }
    return thumbs;
}

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
    ic.randomize_start_point = cfg->randomize_start_point != 0;
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
    ic.support_config.honeycomb_cell_size = 5.0F;

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
    ic.capture_geometry = cfg->capture_geometry != 0;
    ic.cancel_flag = cfg->cancel_flag;
    ic.gcode_thumbnails = cfg->gcode_thumbnails != 0;
    ic.gcode_thumbnail_size = cfg->gcode_thumbnail_size;
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
    cr.geometry_id = ir.geometry_id;
    return cr;
}

// ModelLoader is already included at the top of this file.
// InternalConfig does not reference ModelLoader, so it is unaffected by its non-copyability.

/**
 * @brief Core coroutine implementation of the FDM pipeline.
 */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters) caller keeps cfg alive
Utils::Task<InternalResult> RunPipelineAsync(const InternalConfig& cfg)
{
    // Install the common custom types' AnyObject/Lua registration functions into the shared registry pool for use by
    // each stage's Lua environment
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
            // Vase/spiral mode emits a single continuous wall: no support, no infill.
            ReportProgress(cfg, 60, "Support disabled (spiral mode)");
        }
        else if (cfg.enable_support)
        {
            ReportProgress(cfg, 45, "Generating supports...");
            if (!cfg.support_lua_script.empty())
            {
                // Lua custom support via LibHsBaSlicer API
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
            if (cfg.CancelRequested())
            {
                return;
            }
            if (layer_outlines[i].empty())
            {
                return;
            }
            Polygons const int_polys = Integerization(layer_outlines[i]);
            bool const is_solid = (i < bottom_end) || (i >= top_start);  // solid fill for top/bottom layers
            double const spacing = is_solid ? cfg.fill_spacing : middle_spacing;
            Polygons const fill_result =
                FillWithBorder(int_polys, spacing, cfg.wall_count, cfg.fill_mode, cfg.fill_angle);
            if (cfg.randomize_start_point)
            {
                // Stagger extrusion seams: rotate contour/offset loops to a per-layer
                // deterministic random start point located on an edge interior.
                layer_fills[i] =
                    UnIntegerization(RandomizePathStartPoints(fill_result, static_cast<std::uint64_t>(i) + 1ULL));
                return;
            }
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
                cfg.CheckCancel();
                if (!layer_outlines[i].empty())
                {
                    Polygons const int_polys = Integerization(layer_outlines[i]);
                    bool const is_solid = (i < bottom_end) || (i >= top_start);
                    if (!is_solid)
                    {
                        // Lua custom fill via LibHsBaSlicer API
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
            co_await ParallelForLayersAsync(total_layers, cpp_fill_layer,
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

        // Hand the layer contours to the geometry snapshot pool before path generation
        // consumes them; the copy keeps the pipeline's own data untouched.
        if (cfg.capture_geometry)
        {
            std::vector<double> geometry_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                geometry_zs[i] = static_cast<double>(GetLayerZ(i, cfg.first_layer_height, cfg.layer_height));
            }
            // A vase-mode run deposits one continuous helix, so the snapshot keeps the very
            // polyline it will print: SpiralizeOuterWall is a pure function of the same
            // inputs the exporter uses, which makes the stored path identical to the G-code.
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
            result.geometry_id = CaptureLayerGeometry(HSBA_PIPELINE_FDM, layer_outlines, layer_fills, layer_supports,
                                                      geometry_zs, path_xyz);
        }

        // The previews are projected once from the mesh that was just cut, before the branch, because
        // both the spiral and the ordinary export write the same header and neither may run twice.
        std::vector<GCodeThumbnail> thumbnails;
        if (cfg.gcode_thumbnails)
        {
            ReportProgress(cfg, 92, "Rendering preview images...");
            thumbnails = BuildModelThumbnails(*model, cfg.gcode_thumbnail_size);
        }

        if (cfg.spiral_mode)
        {
            // Vase/spiral mode: merge per-layer outer contours into one continuous,
            // Z-rising helix (no per-layer travel, no infill/support).
            std::vector<double> layer_zs(total_layers);
            for (int i = 0; i < total_layers; ++i)
            {
                layer_zs[i] = static_cast<double>(GetLayerZ(i, cfg.first_layer_height, cfg.layer_height));
            }

            auto gcode_path = GenerateGCodePathSpiral(layer_outlines, layer_zs, cfg.printer_config);
            if (gcode_path)
            {
                if (!thumbnails.empty())
                {
                    gcode_path->SetThumbnails(thumbnails);
                }
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
                if (!thumbnails.empty())
                {
                    gcode_path->SetThumbnails(thumbnails);
                }
                result.gcode_content = gcode_path->ToGCode(cfg.firmware);
                result.success = true;
            }
            else
            {
                result.success = false;
                result.error_message = "Failed to generate G-code path";
            }
        }

        if (result.success && !cfg.output_path.empty())
        {
            ReportProgress(cfg, 97, "Writing G-code file...");
            if (!SaveGCode(result.gcode_content, cfg.output_path))
            {
                result.success = false;
                // Keep the server-side path out of the caller-visible error; the caller already
                // knows cfg.output_path (it set it) and result.output_path echoes it back for the
                // rare default-only case, so the message can stay generic without losing diagnosability.
                result.error_message = "Failed to write G-code to the job output file";
            }
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
struct FdmAsyncThunk
{
    HsBaResultCallback callback = nullptr;
    void* user_data = nullptr;
};

/**
 * @brief Job completion hook of HsBaRunFdmPipelineAsync.
 *
 * The job owns the temporary result struct and its strings; it releases both as soon
 * as this callback returns, which matches the existing contract of taking a copy and
 * calling HsBaFreePipelineResult.
 */
void DeliverFdmResult(void* typed_result, void* thunk)
{
    const std::unique_ptr<FdmAsyncThunk> owned(static_cast<FdmAsyncThunk*>(thunk));
    if (owned->callback && typed_result)
    {
        owned->callback(*static_cast<HsBaFdmPipelineResult_t*>(typed_result), owned->user_data);
    }
}

}  // anonymous namespace

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
    // Delegate to the job machinery: the configuration is deep-copied, so neither config nor
    // its strings have to outlive this call, and the caller is not blocked any more.
    auto* thunk = new HsBa::Slicer::Pipeline::FdmAsyncThunk();
    thunk->callback = result_callback;
    thunk->user_data = result_user_data;
    HsBa::Slicer::Pipeline::SubmitLegacyJob(HSBA_PIPELINE_FDM, config, callback, user_data,
                                            &HsBa::Slicer::Pipeline::DeliverFdmResult, thunk);
}

HSBA_SLICER_API void HsBaFreePipelineResult(HsBaFdmPipelineResult_t* result)
{
    if (!result)
    {
        return;
    }
    std::free(std::exchange(result->gcode_content, nullptr));
    std::free(std::exchange(result->error_message, nullptr));
}
