/// @file hsba_slicer.cppm
/// @brief C++20 module interface for HsBaSlicer (single-file: declarations + definitions).
///
/// Provides a class-based C++ API wrapping LibHsBaSlicer's free functions.
/// Consumers use `import hsba.slicer;` to access the full slicing pipeline.
///
/// NOTE: This is a single interface unit (no separate implementation unit)
/// to avoid MSVC C2572 errors caused by implicit-import std:: redefinition.

module;

// ---- Global module fragment (not exported) ----
#ifdef _WIN32
#include <windows.h>
#endif

// Standard library
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Third-party: Clipper2
#include <clipper2/clipper.h>
#include <clipper2/clipper.offset.h>

// Third-party: Eigen
#include <Eigen/Core>
#include <Eigen/Geometry>

// Third-party: Lua
#include <lua.hpp>

// Project headers (types + declarations)
#include "pipelinetypes/pipeline_types.h"
#include "2D/IntPolygon.hpp"
#include "2D/FloatPolygons.hpp"
#include "2D/PolygonFill.hpp"
#include "base/IModel.hpp"
#include "meshmodel/FullTopoModel.hpp"
#include "paths/IPath.hpp"
#include "paths/pointspath.hpp"
#include "paths/gcodepath.hpp"
#include "support/SupportConfig.hpp"
#include "LibHsBaSlicer/export.h"
#include "LibHsBaSlicer/version_info.hpp"
#include "LibHsBaSlicer/Preprocess/model_preprocess.hpp"
#include "LibHsBaSlicer/Slice/mesh_slice.hpp"
#include "LibHsBaSlicer/Support/fdm_support.hpp"
#include "LibHsBaSlicer/Fill/polygon_fill.hpp"
#include "LibHsBaSlicer/Path/path_generator.hpp"
#include "LibHsBaSlicer/Path/sls_export.hpp"
#include "LibHsBaSlicer/Floor/sla_floor.hpp"
#include "LibHsBaSlicer/Extends/LuaAddFunction.hpp"

// ---- Module interface ----
export module hsba.slicer;

export namespace HsBa::Slicer
{

// ===========================================================================
// Exception
// ===========================================================================

/// @brief Base exception for all slicer errors.
class SlicerError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// ===========================================================================
// Type aliases (re-exported for consumer convenience)
// ===========================================================================

using Point2    = Clipper2Lib::Point64;
using Polygon   = Clipper2Lib::Path64;
using Polygons  = Clipper2Lib::Paths64;
using Point2D   = Clipper2Lib::PointD;
using PolygonD  = Clipper2Lib::PathD;
using PolygonsD = Clipper2Lib::PathsD;

// Re-export pipeline_types.h config/result types
using ::HsBaFillMode_t;
using ::HsBaSupportPattern_t;
using ::HsBaGCodeFirmware_t;
using ::HsBaSlaSupportPattern_t;
using ::HsBaSlaImageType_t;
using ::HsBaFdmPipelineConfig_t;
using ::HsBaFdmPipelineResult_t;
using ::HsBaSlaPipelineConfig_t;
using ::HsBaSlaPipelineResult_t;
using ::HsBaSlsPipelineConfig_t;
using ::HsBaSlsPipelineResult_t;

// Re-export support config types into HsBa::Slicer namespace
using Support::SupportConfig;
using Support::FdmSupportConfig;
using Support::SlaSupportConfig;

// Re-export Lua registration function type
using LuaRegFunc = std::function<void(lua_State*)>;

/// @brief Create default FDM pipeline config.
HsBaFdmPipelineConfig_t defaultFdmConfig();
/// @brief Create default SLA pipeline config.
HsBaSlaPipelineConfig_t defaultSlaConfig();
/// @brief Create default SLS pipeline config.
HsBaSlsPipelineConfig_t defaultSlsConfig();

// ===========================================================================
// Model (RAII wrapper)
// ===========================================================================

/// @brief RAII model handle. Loads on construction, removes on destruction.
class Model
{
public:
    /// @brief Load a model into the pool.
    /// @throws SlicerError on failure.
    Model(std::string name, const std::filesystem::path& file);
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    inline Model(Model&& other) noexcept;
    inline Model& operator=(Model&& other) noexcept;

    /// @brief Get model bounding box and volume.
    inline ModelInfo info() const;

    /// @brief Apply translation.
    inline void translate(const Eigen::Vector3f& t);
    /// @brief Apply rotation.
    inline void rotate(const Eigen::Quaternionf& r);
    /// @brief Apply uniform scale.
    inline void scale(float s);
    /// @brief Apply non-uniform scale.
    inline void scale(const Eigen::Vector3f& s);

    /// @brief Slice at given height (integer coordinates).
    inline Polygons slice(float height) const;
    /// @brief Slice at given height (double coordinates).
    inline PolygonsD sliceD(float height) const;

    /// @brief Access underlying IModel.
    inline const IModel& raw() const;
    /// @brief Model name.
    inline const std::string& name() const;

private:
    std::string name_;
    std::shared_ptr<IModel> ptr_;
};

// ===========================================================================
// FDM Pipeline
// ===========================================================================

/// @brief FDM pipeline result (C++ style).
struct FdmResult
{
    std::string gcode;
    int total_layers = 0;
};

/// @brief FDM full pipeline: Slice -> Support -> Fill -> Path.
class FdmPipeline
{
public:
    explicit FdmPipeline(HsBaFdmPipelineConfig_t cfg = HsBaFdmConfigDefault());

    /// @brief Run full pipeline on a model.
    /// @throws SlicerError on failure.
    FdmResult run(const Model& model) const;

    /// @brief Slice all layers.
    inline std::vector<Polygons> sliceAll(const Model& model) const;
    /// @brief Generate supports for all layers.
    inline std::vector<PolygonsD> generateSupports(const std::vector<PolygonsD>& layers) const;
    /// @brief Fill a single layer contour.
    inline Polygons fill(const Polygons& contour) const;
    /// @brief Generate G-code path from layer data.
    inline std::unique_ptr<GCodePath> generatePath(const std::vector<LayerPathData>& data) const;

private:
    HsBaFdmPipelineConfig_t cfg_;
};

// ===========================================================================
// SLA Pipeline
// ===========================================================================

/// @brief SLA pipeline result (C++ style).
struct SlaResult
{
    bool saved        = false;
    int total_layers  = 0;
};

/// @brief SLA full pipeline: Slice -> Support -> Floor -> Render -> Package.
class SlaPipeline
{
public:
    explicit SlaPipeline(HsBaSlaPipelineConfig_t cfg = HsBaSlaConfigDefault());

    /// @brief Run full SLA pipeline and save to zip.
    /// @throws SlicerError on failure.
    inline SlaResult run(const Model& model, const std::filesystem::path& output_zip) const;

    /// @brief Generate floor/raft from bottom layer.
    inline Polygons generateFloor(const Polygons& bottom_layer) const;
    /// @brief Render polygons to image file.
    inline  bool renderLayer(const PolygonsD& polys, int width, int height,
                     const std::string& out_path) const;
    /// @brief Save SLA package to zip.
    inline bool savePackage(const SlaPackage& pkg, const std::string& output_zip) const;

private:
    HsBaSlaPipelineConfig_t cfg_;
};

// ===========================================================================
// SLS Pipeline
// ===========================================================================

/// @brief SLS pipeline (Lua-driven export).
class SlsPipeline
{
public:
    explicit SlsPipeline(HsBaSlsPipelineConfig_t cfg = HsBaSlsConfigDefault());

    /// @brief Run SLS export via Lua script.
    /// @throws SlicerError on failure.
    inline bool run(const Model& model) const;

private:
    HsBaSlsPipelineConfig_t cfg_;
};

// ===========================================================================
// Lua custom functions
// ===========================================================================

/// @brief Custom fill via Lua script file.
inline Polygons luaCustomFill(const Polygons& poly, const std::string& script_path,
                       const std::string& func_name = "generate_fill",
                       double line_thickness = 0.5);

/// @brief Custom floor via Lua script file.
inline Polygons luaCustomFloor(const Polygons& bottom_layer, const std::string& script_path,
                        const std::string& func_name, const SlaFloorConfig& config);

/// @brief Custom support via Lua script.
inline std::vector<PolygonsD> luaCustomSupport(const std::vector<PolygonsD>& layers,
                                         const SupportConfig& config,
                                         std::string_view script,
                                         std::string_view func_name = "generate_support");

// ===========================================================================
// External Lua function registration
// ===========================================================================

/// @brief Register an external 2D Lua function (available in Support, Fill, SLA Output stages).
inline void add2DFunction(LuaRegFunc func);

/// @brief Register an external 3D Lua function (available in Slice, Support stages).
inline void add3DFunction(LuaRegFunc func);

/// @brief Register an external File Lua function (available in SLS Output, SLA Output stages).
inline void addFileFunction(LuaRegFunc func);

/// @brief Register an event callback by name (e.g. "zipper.on_add").
inline void addEventCallback(const std::string& event_name, LuaRegFunc func);

// ===========================================================================
// Version
// ===========================================================================

/// @brief Get version info as JSON.
inline std::string versionJson();
/// @brief Get version info as XML.
inline std::string versionXml();

// ===========================================================================
// Utilities
// ===========================================================================

/// @brief Convert integer polygons to double precision.
inline PolygonsD toDouble(const Polygons& polys);
/// @brief Convert double polygons to integer precision.
inline Polygons toInt(const PolygonsD& polys);

}  // namespace HsBa::Slicer

// ===========================================================================
// ======================== IMPLEMENTATION ===================================
// ===========================================================================

namespace HsBa::Slicer
{

// ===========================================================================
// Default config factories (defined here so consumers don't need GMF statics)
// ===========================================================================

HsBaFdmPipelineConfig_t defaultFdmConfig() { return HsBaFdmConfigDefault(); }
HsBaSlaPipelineConfig_t defaultSlaConfig() { return HsBaSlaConfigDefault(); }
HsBaSlsPipelineConfig_t defaultSlsConfig() { return HsBaSlsConfigDefault(); }

// ===========================================================================
// Model
// ===========================================================================

Model::Model(std::string name, const std::filesystem::path& file)
    : name_(std::move(name))
{
    ptr_ = LoadModel(name_, file.string());
    if (!ptr_)
        throw SlicerError("Failed to load model: " + name_);
}

Model::~Model()
{
    if (!name_.empty())
        RemoveModel(name_);
}

Model::Model(Model&& other) noexcept
    : name_(std::move(other.name_)), ptr_(std::move(other.ptr_))
{
    other.name_.clear();
}

Model& Model::operator=(Model&& other) noexcept
{
    if (this != &other)
    {
        if (!name_.empty())
            RemoveModel(name_);
        name_ = std::move(other.name_);
        ptr_  = std::move(other.ptr_);
        other.name_.clear();
    }
    return *this;
}

ModelInfo Model::info() const { return GetModelInfo(name_); }

void Model::translate(const Eigen::Vector3f& t) { TranslateModel(name_, t); }
void Model::rotate(const Eigen::Quaternionf& r) { RotateModel(name_, r); }
void Model::scale(float s) { ScaleModel(name_, s); }
void Model::scale(const Eigen::Vector3f& s) { ScaleModel(name_, s); }

Polygons Model::slice(float height) const { return Slice(*ptr_, height); }

PolygonsD Model::sliceD(float height) const
{
    return UnIntegerization(Slice(*ptr_, height));
}

const IModel& Model::raw() const { return *ptr_; }
const std::string& Model::name() const { return name_; }

// ===========================================================================
// FdmPipeline
// ===========================================================================

FdmPipeline::FdmPipeline(HsBaFdmPipelineConfig_t cfg) : cfg_(cfg) {}

std::vector<Polygons> FdmPipeline::sliceAll(const Model& model) const
{
    const auto mi     = model.info();
    const float height = mi.bbox_max.z() - mi.bbox_min.z();
    const int layers   = static_cast<int>(height / cfg_.layer_height) + 1;

    std::vector<Polygons> result;
    result.reserve(layers);
    for (int i = 0; i < layers; ++i)
    {
        float z = (i == 0) ? cfg_.first_layer_height
                           : cfg_.first_layer_height + i * cfg_.layer_height;
        result.push_back(Slice(model.raw(), z));
    }
    return result;
}

std::vector<PolygonsD> FdmPipeline::generateSupports(const std::vector<PolygonsD>& layers) const
{
    if (!cfg_.enable_support)
        return {};

    Support::FdmSupportConfig sc;
    sc.overhang_angle_threshold = cfg_.overhang_angle;
    sc.layer_height             = cfg_.layer_height;
    sc.support_gap              = cfg_.support_gap;
    sc.support_diameter         = cfg_.support_diameter;
    sc.support_density          = cfg_.support_density;
    sc.support_pattern          = static_cast<int>(cfg_.support_pattern);
    sc.interface_layers         = cfg_.interface_layers;
    sc.interface_density        = cfg_.interface_density;

    if (cfg_.support_lua_script)
    {
        return GenerateAllLuaSupport(layers, sc, cfg_.support_lua_script,
                                     cfg_.support_lua_func ? cfg_.support_lua_func : "generate_support");
    }
    return GenerateAllFdmSupport(layers, sc);
}

Polygons FdmPipeline::fill(const Polygons& contour) const
{
    static constexpr FillMode kFillModes[] = { FillMode::Line, FillMode::SimpleZigzag, FillMode::Zigzag };
    FillMode mode = kFillModes[static_cast<int>(cfg_.fill_mode)];

    if (cfg_.infill_lua_script)
    {
        return LuaCustomFillByFile(contour, cfg_.infill_lua_script,
                                   cfg_.infill_lua_func ? cfg_.infill_lua_func : "generate_fill",
                                   cfg_.fill_spacing);
    }
    return FillWithBorder(contour, cfg_.fill_spacing, cfg_.wall_count, mode, cfg_.fill_angle);
}

std::unique_ptr<GCodePath> FdmPipeline::generatePath(const std::vector<LayerPathData>& data) const
{
    FdmPathConfig pc;
    pc.layer_height         = cfg_.layer_height;
    pc.line_width           = cfg_.line_width;
    pc.print_speed          = cfg_.print_speed;
    pc.travel_speed         = cfg_.travel_speed;
    pc.extrusion_multiplier = cfg_.extrusion_multiplier;
    pc.units                = GCodeUnits::mm;

    GCodePrinterConfig printer_cfg;
    printer_cfg.nozzle_diameter       = cfg_.nozzle_diameter;
    printer_cfg.filament_diameter     = cfg_.filament_diameter;
    printer_cfg.nozzle_temp           = cfg_.nozzle_temp;
    printer_cfg.bed_temp              = cfg_.bed_temp;
    printer_cfg.retract_length        = cfg_.retract_length;
    printer_cfg.retract_speed         = cfg_.retract_speed;
    printer_cfg.print_speed           = cfg_.print_speed;
    printer_cfg.travel_speed          = cfg_.travel_speed;
    printer_cfg.first_layer_speed     = cfg_.first_layer_speed;
    printer_cfg.layer_height          = cfg_.layer_height;
    printer_cfg.line_width            = cfg_.line_width;
    printer_cfg.extrusion_multiplier  = cfg_.extrusion_multiplier;

    return GenerateGCodePathV2(data, pc, printer_cfg);
}

FdmResult FdmPipeline::run(const Model& model) const
{
    // 1. Slice
    auto contours = sliceAll(model);
    const int total_layers = static_cast<int>(contours.size());

    // 2. Convert to double for support
    std::vector<PolygonsD> layers_d;
    layers_d.reserve(total_layers);
    for (const auto& c : contours)
        layers_d.push_back(UnIntegerization(c));

    // 3. Support
    auto supports = generateSupports(layers_d);

    // 4. Fill + assemble layer data
    std::vector<LayerPathData> all_data;
    all_data.reserve(total_layers);
    for (int i = 0; i < total_layers; ++i)
    {
        LayerPathData ld;
        ld.z_height = (i == 0) ? cfg_.first_layer_height
                               : cfg_.first_layer_height + i * cfg_.layer_height;
        ld.outlines = layers_d[i];

        if (!contours[i].empty())
            ld.fills = UnIntegerization(fill(contours[i]));

        if (i < static_cast<int>(supports.size()))
            ld.supports = supports[i];

        all_data.push_back(std::move(ld));
    }

    // 5. Path generation
    auto path = generatePath(all_data);

    FdmResult result;
    result.total_layers = total_layers;
    auto firmware = static_cast<GCodeFirmware>(static_cast<int>(cfg_.gcode_firmware));
    result.gcode        = path->ToGCode(firmware);

    // 6. Optional save to file
    if (cfg_.output_path)
        path->SaveGCode(cfg_.output_path, firmware);

    return result;
}

// ===========================================================================
// SlaPipeline
// ===========================================================================

SlaPipeline::SlaPipeline(HsBaSlaPipelineConfig_t cfg) : cfg_(cfg) {}

Polygons SlaPipeline::generateFloor(const Polygons& bottom_layer) const
{
    SlaFloorConfig fc;
    fc.raft_offset     = cfg_.floor_raft_offset;
    fc.border_width    = cfg_.floor_border_width;
    fc.fill_spacing    = cfg_.floor_fill_spacing;
    fc.fill_angle_deg  = cfg_.floor_fill_angle;
    fc.border_count    = cfg_.floor_border_count;
    fc.use_convex_hull = (cfg_.floor_use_convex_hull != 0);

    if (cfg_.floor_lua_script)
    {
        return LuaCustomFloorByFile(bottom_layer, cfg_.floor_lua_script,
                                    cfg_.floor_lua_func ? cfg_.floor_lua_func : "generate_floor", fc);
    }
    return GenerateFloorRaft(bottom_layer, fc);
}

bool SlaPipeline::renderLayer(const PolygonsD& polys, int width, int height,
                              const std::string& out_path) const
{
    return RenderPolygonsToImage(polys, width, height, out_path);
}

bool SlaPipeline::savePackage(const SlaPackage& pkg, const std::string& output_zip) const
{
    if (cfg_.export_lua_script)
    {
        return SaveSlaPackageLua(pkg, output_zip, cfg_.export_lua_script,
                                 cfg_.export_lua_func ? cfg_.export_lua_func : "export_sla");
    }
    return SaveSlaPackage(pkg, output_zip);
}

SlaResult SlaPipeline::run(const Model& model, const std::filesystem::path& output_zip) const
{
    // 1. Slice all layers
    const auto mi      = model.info();
    const float height = mi.bbox_max.z() - mi.bbox_min.z();
    const int layers   = static_cast<int>(height / cfg_.layer_height) + 1;

    std::vector<PolygonsD> layers_d;
    layers_d.reserve(layers);
    for (int i = 0; i < layers; ++i)
    {
        float z = (i == 0) ? cfg_.first_layer_height
                           : cfg_.first_layer_height + i * cfg_.layer_height;
        layers_d.push_back(UnIntegerization(Slice(model.raw(), z)));
    }

    // 2. Support
    std::vector<PolygonsD> supports;
    if (cfg_.enable_support)
    {
        Support::SlaSupportConfig sc;
        sc.overhang_angle_threshold = cfg_.overhang_angle;
        sc.layer_height             = cfg_.layer_height;
        sc.support_gap              = cfg_.support_gap;
        sc.support_diameter         = cfg_.support_diameter;
        sc.support_density          = cfg_.support_density;

        if (cfg_.support_lua_script)
        {
            supports = GenerateAllLuaSupport(layers_d, sc, cfg_.support_lua_script,
                                            cfg_.support_lua_func ? cfg_.support_lua_func : "generate_support");
        }
        else
        {
            supports = GenerateAllSlaSupport(layers_d, sc);
        }
    }

    // 3. Floor
    PolygonsD floor_polygons;
    if (!layers_d.empty())
    {
        auto bottom_int = Integerization(layers_d.front());
        auto floor_int  = generateFloor(bottom_int);
        floor_polygons  = UnIntegerization(floor_int);
    }

    // 4. Package
    static const char* kImageExts[] = { ".png", ".jpg", ".svg" };
    const char* ext = kImageExts[static_cast<int>(cfg_.image_type)];

    SlaPackage pkg;
    pkg.layer_outlines  = std::move(layers_d);
    pkg.layer_supports  = std::move(supports);
    pkg.floor_polygons  = std::move(floor_polygons);
    pkg.image_width     = cfg_.image_width;
    pkg.image_height    = cfg_.image_height;
    pkg.image_extension = ext;

    SlaResult result;
    result.total_layers = static_cast<int>(pkg.layer_outlines.size());
    result.saved        = savePackage(pkg, output_zip.string());
    return result;
}

// ===========================================================================
// SlsPipeline
// ===========================================================================

SlsPipeline::SlsPipeline(HsBaSlsPipelineConfig_t cfg) : cfg_(cfg) {}

bool SlsPipeline::run(const Model& model) const
{
    if (!cfg_.export_lua_script)
        throw SlicerError("SLS pipeline requires export_lua_script");

    // Slice
    const auto mi      = model.info();
    const float height = mi.bbox_max.z() - mi.bbox_min.z();
    const int layers   = static_cast<int>(height / cfg_.layer_height) + 1;

    SlsPackage pkg;
    pkg.layer_outlines.reserve(layers);
    pkg.layer_z_heights.reserve(layers);
    for (int i = 0; i < layers; ++i)
    {
        float z = cfg_.first_layer_height + i * cfg_.layer_height;
        pkg.layer_outlines.push_back(UnIntegerization(Slice(model.raw(), z)));
        pkg.layer_z_heights.push_back(z);
    }

    const char* output = cfg_.output_path ? cfg_.output_path : "";
    return SaveSlsPackageLua(pkg, output, cfg_.export_lua_script,
                             cfg_.export_lua_func ? cfg_.export_lua_func : "export_sls");
}

// ===========================================================================
// Lua custom functions
// ===========================================================================

Polygons luaCustomFill(const Polygons& poly, const std::string& script_path,
                       const std::string& func_name, double line_thickness)
{
    return LuaCustomFillByFile(poly, script_path, func_name, line_thickness);
}

Polygons luaCustomFloor(const Polygons& bottom_layer, const std::string& script_path,
                        const std::string& func_name, const SlaFloorConfig& config)
{
    return LuaCustomFloorByFile(bottom_layer, script_path, func_name, config);
}

std::vector<PolygonsD> luaCustomSupport(const std::vector<PolygonsD>& layers,
                                         const SupportConfig& config,
                                         std::string_view script,
                                         std::string_view func_name)
{
    return GenerateAllLuaSupport(layers, config, script, func_name);
}

// ===========================================================================
// External Lua function registration
// ===========================================================================

void add2DFunction(LuaRegFunc func) { Add2DFunctions(std::move(func)); }
void add3DFunction(LuaRegFunc func) { Add3DFunctions(std::move(func)); }
void addFileFunction(LuaRegFunc func) { AddFileFunctions(std::move(func)); }
void addEventCallback(const std::string& event_name, LuaRegFunc func)
{
    AddEventCallback(event_name, std::move(func));
}

// ===========================================================================
// Version
// ===========================================================================

std::string versionJson() { return GetVersionJson(); }
std::string versionXml() { return GetVersionXml(); }

// ===========================================================================
// Utilities
// ===========================================================================

PolygonsD toDouble(const Polygons& polys) { return UnIntegerization(polys); }
Polygons toInt(const PolygonsD& polys) { return Integerization(polys); }

}  // namespace HsBa::Slicer
